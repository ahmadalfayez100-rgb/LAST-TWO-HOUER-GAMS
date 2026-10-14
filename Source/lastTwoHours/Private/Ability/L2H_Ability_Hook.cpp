// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Hook.h"

#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Weapon/L2H_WeaponActor.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Grab/L2H_GrabbableInterface.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_Hook::UL2H_Ability_Hook()
{
	// One instance per pawn, because the ability reads per-pawn input and has no cross-actor state.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Server-only: an impulse and a launch are authoritative physics, so running this on a client
	// would move a body the server disagrees with and the correction would snap it back.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Free by design. Hook and carry are base abilities and never spend time.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;
}

void UL2H_Ability_Hook::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// CommitAbility would run the cost/cooldown path; there is deliberately neither.
	if (!HasServerAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A second press mid-flight lets go: the body falls on from where it is.
	if (Character->IsHookSwinging())
	{
		Character->ReleaseHookSwing();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FVector HitLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	AActor* Target = FindGrabTarget(HookRange, HitLocation, ViewDirection, /*bFromCamera=*/true);

	// The line is drawn whether or not anything was caught: a hook fired into the air that shows nothing
	// reads as a dead button. It leaves the muzzle when a weapon is in hand, the right hand otherwise.
	FVector BeamStart = Character->GetMesh() ? Character->GetMesh()->GetSocketLocation(TEXT("hand_r")) : Character->GetActorLocation();
	if (const UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>())
	{
		if (const AL2H_WeaponActor* Weapon = Weapons->GetActiveWeapon())
		{
			BeamStart = Weapon->GetFireSocketTransform().GetLocation();
		}
	}
	Character->MulticastShotFX(BeamStart, HitLocation, /*bHook=*/true);

	if (!Target)
	{
		// Nothing grabbable on the line, so the world itself is the target. Without this the hook could
		// only ever throw a crate around: the thing a traversal grapple has to catch is the wall, the
		// ledge and the roof, and none of those carry a mass profile.
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		Character->GetAimViewPoint(ViewLocation, ViewRotation);

		const FVector AimEnd = ViewLocation + ViewRotation.Vector() * HookRange;

		FCollisionQueryParams WorldParams(FName(TEXT("L2H_Hook")), /*bTraceComplex=*/false, Character);
		FHitResult WorldHit;

		// The floor under the player is not a grapple point. With the camera tilted a little down the line
		// lands on the ground a few metres ahead, and grappling that hopped the player up and dragged them
		// along the floor -- the "strange jump" on L1. A surface that faces up and sits below head height
		// is ground; walls, ledges and anything overhead still catch.
		const bool bWorldHit = GetWorld() &&
			GetWorld()->LineTraceSingleByChannel(WorldHit, ViewLocation, AimEnd, ECC_Visibility, WorldParams);

		// Only the floor the player is STANDING on is refused. Airborne a downward-facing surface is a landing
		// target, not the ground underfoot: the feet are already high above it, so the height test below is
		// meaningless there and every airborne hook that looked at the floor was thrown away as "aimed at the
		// ground" -- which is the hook doing nothing in mid-air.
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		const bool bGrounded = Move && Move->IsMovingOnGround();
		const float FeetZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
		const bool bIsGround = bGrounded && bWorldHit && WorldHit.ImpactNormal.Z > 0.7f && WorldHit.ImpactPoint.Z < FeetZ + 170.0f;

		if (bIsGround)
		{
			L2HTime::LogVerbose(TEXT("Hook: aimed at the ground -- no grapple"));
		}
		else if (bWorldHit)
		{
			// The player travels. A wall has nothing to drag, so the hook throws the player on one arc
			// to a landing point beside it (or on top of it, for a surface that faces up).
			Character->StartHookSwing(WorldHit.ImpactPoint, WorldHit.ImpactNormal);

			L2HTime::LogImportant(FString::Printf(TEXT("Hook: grappling %s at %s"),
				*GetNameSafe(WorldHit.GetActor()), *WorldHit.ImpactPoint.ToCompactString()));

			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
			return;
		}

		// Not an error: the player hooked empty air past the end of the trace, which is the common case.
		L2HTime::LogVerbose(TEXT("Hook: nothing grabbable in front"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(Target);
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(Character);

	const float TargetMass = Profile ? Profile->ResolveMass() : 0.0f;
	const float PullCapacity = Config ? Config->HookPullCapacity : 40.0f;

	// The hook catches everything. Something light and loose comes to the player; anything else -- an
	// anchor, a heavy crate, a prop that does not simulate, a grapple point -- holds, and the player
	// travels to it instead. Refusing anchors and static props was what made "hook anything" fail: the
	// press found a target, said no, and nothing happened.
	UPrimitiveComponent* TargetRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	const bool bAnchor = Profile && Profile->CarryClass == EL2H_CarryClass::Anchor;
	const bool bLoose = TargetRoot && TargetRoot->IsSimulatingPhysics();
	const bool bReelInToTarget = bAnchor || !bLoose || (Profile && Profile->bHookReelIn) || TargetMass > PullCapacity;

	if (!bReelInToTarget)
	{
		// A velocity, not a raw impulse. The old fixed impulse divided by mass, so a 5 kg box left at
		// 240 m/s and a 25 kg one at 48 m/s. This throws it on an arc that lands at the player's feet
		// whatever it weighs: horizontal speed from the distance, plus the lift gravity needs for that
		// flight time.
		const FVector From = TargetRoot->GetComponentLocation();
		// Lands in front of the player, on the side the object came from. Horizontal speed is kept near
		// 7 m/s: at 14 m/s (measured) the object arrived on time and then slid 12 m past the player.
		const FVector Toward = FVector(From.X - Character->GetActorLocation().X, From.Y - Character->GetActorLocation().Y, 0.0f).GetSafeNormal();
		const FVector To = Character->GetActorLocation() + Toward * 150.0f;
		const FVector Flat = FVector(To.X - From.X, To.Y - From.Y, 0.0f);
		const float FlightTime = FMath::Clamp(Flat.Size() / 700.0f, 0.5f, 1.6f);
		const float Gravity = FMath::Abs(GetWorld() ? GetWorld()->GetGravityZ() : -980.0f);
		const FVector Launch = Flat / FlightTime
			+ FVector(0.0f, 0.0f, (To.Z - From.Z) / FlightTime + 0.5f * Gravity * FlightTime);

		TargetRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
		TargetRoot->AddImpulse(Launch, NAME_None, /*bVelChange=*/true);

		if (Target->Implements<UL2H_GrabbableInterface>())
		{
			IL2H_GrabbableInterface::Execute_OnGrabbed(Target, Character);
		}

		L2HTime::LogImportant(FString::Printf(TEXT("Hook: pulled %s (%.1f kg)"),
			*GetNameSafe(Target), TargetMass));
	}
	else
	{
		// Too heavy, or fixed, so the player travels instead, on the same arc as a world grapple. With no
		// surface normal the landing point is taken short of the hit along the approach.
		Character->StartHookSwing(HitLocation);

		L2HTime::LogImportant(FString::Printf(TEXT("Hook: reeling in to %s (%.1f kg, %s)"),
			*GetNameSafe(Target),
			TargetMass,
			bAnchor ? TEXT("anchor") : (!bLoose ? TEXT("fixed") : ((Profile && Profile->bHookReelIn) ? TEXT("grapple point") : TEXT("too heavy")))));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
