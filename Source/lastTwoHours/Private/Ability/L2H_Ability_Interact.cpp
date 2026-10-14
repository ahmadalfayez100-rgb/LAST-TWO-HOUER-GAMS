// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Interact.h"

#include "Ability/L2H_Ability_Carry.h"
#include "Ability/L2H_Ability_Hook.h"
#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_Interact::UL2H_Ability_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Server-only, like both abilities it forwards to. Which one wins is one authority decision; a
	// client picking for itself would only produce a second answer that the server then contradicts.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	// Defaults rather than hard references, so a project that never opens this class still has a working
	// context button. They keep the override: a pawn may point either half at its own ability, and both
	// classes stay selectable on the class defaults.
	CarryAbilityClass = UL2H_Ability_Carry::StaticClass();
	HookAbilityClass = UL2H_Ability_Hook::StaticClass();
}

void UL2H_Ability_Interact::ActivateAbility(
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

	// Read before the ability ends: this is the component the forwarded ability is started through.
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();

	TSubclassOf<UGameplayAbility> Next;

	if (Character->IsCarrying())
	{
		Next = CarryAbilityClass;
	}
	else
	{
		FVector HitLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		AActor* Target = FindGrabTarget(InteractRange, HitLocation, ViewDirection);

		if (!Target)
		{
			// Not an error: most presses are aimed at empty air, or at scenery that is not grabbable.
			L2HTime::LogVerbose(TEXT("Interact: nothing grabbable in front"));

			// Nothing to carry, so the press is free for the next use: climbing down a ledge the player
			// stands on. The Blueprint decides (no ladder nearby, free movement, an edge in front).
			Character->ClientInteractFoundNothing();

			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}

		// The interact key always means "carry". It used to hand anything over the solo band to the hook,
		// which now grapples whatever it touches -- so pressing Triangle on a two-player crate yanked the
		// player across the room. The carry ability owns the weight rules: it lifts what one player may,
		// asks for the partner on a two-player load, and says so when something will not move at all.
		Next = CarryAbilityClass;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

	if (!Next)
	{
		// A class left unset on the class defaults is a wiring mistake, not a refusal, so it is loud.
		L2HTime::LogImportant(TEXT("Interact: no ability is wired for this target"));
		return;
	}

	if (AbilitySystem)
	{
		AbilitySystem->TryActivateAbilityByClass(Next);
	}
}
