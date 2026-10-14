// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Carry.h"

#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Grab/L2H_GrabbableInterface.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_Carry::UL2H_Ability_Carry()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Server-only: attaching an actor and stopping its physics has to be decided once. The attachment
	// itself is reproduced on clients by AL2H_BaseCharacter::OnRep_CarriedActor.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Free by design. Carrying never spends time.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;
}

bool UL2H_Ability_Carry::HasCarryPartner(const AL2H_BaseCharacter* Character, const UL2H_LevelTimeConfig* Config) const
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !Character)
	{
		return false;
	}

	// Proximity rather than a second held button. Requiring a simultaneous input would need an assist
	// binding the design has not chosen yet, and it would have two players contending for one object.
	const float AssistRadius = Config ? Config->TwoPlayerAssistRadius : 250.0f;
	const float AssistRadiusSquared = AssistRadius * AssistRadius;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const AL2H_BaseCharacter* Other = PC ? Cast<AL2H_BaseCharacter>(PC->GetPawn()) : nullptr;

		if (!Other || Other == Character)
		{
			continue;
		}

		// A downed partner cannot help lift anything.
		if (Other->IsDowned())
		{
			continue;
		}

		if (FVector::DistSquared(Other->GetActorLocation(), Character->GetActorLocation()) <= AssistRadiusSquared)
		{
			return true;
		}
	}

	return false;
}

void UL2H_Ability_Carry::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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

	// Second press drops. The ability is a toggle, which is what lets one button on a pad run it.
	if (Character->IsCarrying())
	{
		AActor* Dropped = Character->GetCarriedActor();
		Character->EndCarry();

		L2HTime::LogImportant(FString::Printf(TEXT("Carry: dropped %s"), *GetNameSafe(Dropped)));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FVector HitLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	AActor* Target = FindGrabTarget(CarryRange, HitLocation, ViewDirection);

	if (!Target)
	{
		L2HTime::LogVerbose(TEXT("Carry: nothing grabbable in front"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(Character);
	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(Target);

	const float TargetMass = ResolveCarryMass(Target);
	const float SingleCarryMass = Config ? Config->SingleCarryMass : 60.0f;
	const float TwoPlayerCarryMass = Config ? Config->TwoPlayerCarryMass : 120.0f;

	// An anchor is scenery that happens to be reachable. It never moves.
	if (Profile && Profile->CarryClass == EL2H_CarryClass::Anchor)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Carry: %s is an anchor and cannot be moved"), *GetNameSafe(Target)));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (TargetMass > TwoPlayerCarryMass)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Carry: %s is %.1f kg, over the %.1f kg two-player limit -- immovable"),
			*GetNameSafe(Target), TargetMass, TwoPlayerCarryMass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const bool bNeedsTwoPlayers = (Profile && Profile->bRequiresTwoPlayers) || TargetMass > SingleCarryMass;

	if (bNeedsTwoPlayers && !HasCarryPartner(Character, Config))
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Carry: %s is %.1f kg and needs both players -- the partner must be within %.0f cm and standing"),
			*GetNameSafe(Target), TargetMass, Config ? Config->TwoPlayerAssistRadius : 250.0f));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Heaviness is a continuous penalty rather than a band, so a 50 kg crate does not feel identical to
	// a 55 kg one. Both ends come from this ability, so a designer tunes the feel without a rebuild.
	const float MassAlpha = SingleCarryMass > 0.0f
		? FMath::Clamp(TargetMass / SingleCarryMass, 0.0f, 1.0f)
		: 0.0f;

	const float SpeedScale = FMath::Lerp(FastestCarrySpeedScale, HeaviestSoloSpeedScale, MassAlpha);

	const FName Socket = Profile ? Profile->CarrySocket : NAME_None;

	if (!Character->BeginCarry(Target, SpeedScale, Socket))
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Carry: could not pick up %s"), *GetNameSafe(Target)));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Carry: picked up %s (%.1f kg, speed x%.2f%s)"),
		*GetNameSafe(Target),
		TargetMass,
		SpeedScale,
		bNeedsTwoPlayers ? TEXT(", two-player load") : TEXT("")));

	// The hold is on the pawn, not this ability. Ending here is what keeps "press again to drop"
	// working without an ability parked active for the whole carry.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
