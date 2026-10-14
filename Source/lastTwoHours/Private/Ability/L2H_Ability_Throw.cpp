// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Throw.h"

#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Grab/L2H_GrabbableInterface.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_Throw::UL2H_Ability_Throw()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Authoritative physics, like the hook and the carry it partners with.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Free by design. Carry, throw and hook are base verbs and never spend time.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;
}

void UL2H_Ability_Throw::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Every path out of this ability ends it, so the next press can throw again.
	const auto Finish = [this, Handle, ActorInfo, ActivationInfo](bool bWasCancelled)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, bWasCancelled);
	};

	if (!HasServerAuthority())
	{
		Finish(true);
		return;
	}

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		L2HTime::LogImportant(TEXT("Throw refused: the avatar is not an L2H character"));
		Finish(true);
		return;
	}

	AActor* Carried = Character->GetCarriedActor();
	if (!Carried)
	{
		// Not an error: the fire input is also the shot, and this ran while nothing was in hand.
		L2HTime::LogVerbose(TEXT("Throw: nothing in hand"));
		Finish(false);
		return;
	}

	// A partner is set down, never thrown. Their capsule does not simulate, so an impulse would do
	// nothing, and launching a teammate across the room is not what "throw" promises anyway.
	if (Cast<AL2H_BaseCharacter>(Carried))
	{
		Character->EndCarry();
		L2HTime::LogImportant(FString::Printf(TEXT("Throw: %s set %s down"), *GetNameSafe(Character), *GetNameSafe(Carried)));
		Finish(false);
		return;
	}

	UPrimitiveComponent* CarriedRoot = Cast<UPrimitiveComponent>(Carried->GetRootComponent());
	if (!CarriedRoot)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Throw refused: %s has no primitive root to push"), *GetNameSafe(Carried)));
		Finish(false);
		return;
	}

	// The release point and velocity come from the character, which is also what draws the preview arc,
	// so the arc the player aimed with is the arc the object flies.
	FVector ReleasePoint = FVector::ZeroVector;
	FVector ThrowVelocity = FVector::ZeroVector;
	if (!Character->GetThrowLaunch(ReleasePoint, ThrowVelocity))
	{
		Finish(false);
		return;
	}

	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(Carried);
	const float TargetMass = Profile ? Profile->ResolveMass() : 0.0f;
	const float ThrowSpeed = ThrowVelocity.Size();

	// Put down first. EndCarry restores the physics and collision it took away and returns the object to
	// the world, so the velocity is written to a body that is actually simulating rather than to one that
	// is still parented to the hand.
	Character->EndCarry(/*bPlayPutDown=*/false);

	Carried->SetActorLocation(ReleasePoint, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// A velocity, not a raw impulse: the speed was already scaled by mass, and a raw impulse would scale it
	// a second time by whatever the body's own mass happens to be.
	CarriedRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
	CarriedRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	CarriedRoot->AddImpulse(ThrowVelocity, NAME_None, /*bVelChange=*/true);

	if (Carried->Implements<UL2H_GrabbableInterface>())
	{
		IL2H_GrabbableInterface::Execute_OnReleased(Carried, Character);
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Throw: %s released %s at %.0f cm/s (%.1f kg)"),
		*GetNameSafe(Character), *GetNameSafe(Carried), ThrowSpeed, TargetMass));

	Finish(false);
}
