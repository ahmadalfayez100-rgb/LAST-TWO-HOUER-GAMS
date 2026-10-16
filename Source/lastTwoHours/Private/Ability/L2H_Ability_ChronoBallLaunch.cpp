// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_ChronoBallLaunch.h"

#include "Ball/L2H_TimeBall.h"
#include "Character/L2H_BaseCharacter.h"
#include "GameplayTagContainer.h"

UL2H_Ability_ChronoBallLaunch::UL2H_Ability_ChronoBallLaunch()
{
	// The orb lives on the server, so the decision about it has to be made there too.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Time is spent through the director's GameplayEffects, never through an ability cost effect.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Registered by the project's tag list. Skipped rather than asserted when it is missing, so a
		// tag list edit can never break the ability from loading.
		//
		// Deliberately a sibling of Ability.ChronoBall rather than a child of it.
		// TryActivateAbilitiesByTag matches with parent-tag expansion -- FGameplayTagContainer::HasAll
		// checks the query tag against both the container's tags and their parents -- so a child tag
		// would also answer a query for Ability.ChronoBall. The take-out button would then run this
		// ability in the same frame and release the orb the hold had only just picked up. Two sibling
		// tags keep the two buttons separate.
		const FGameplayTag LaunchTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.ChronoBallLaunch")), false);
		if (LaunchTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(LaunchTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_ChronoBallLaunch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());

	// Nothing out means nothing to launch and nowhere to go. The press is refused silently on purpose:
	// this button is shared with the weapon, and an unrelated press must not fill the log.
	if (AL2H_TimeBall* Orb = Character ? AL2H_TimeBall::FindForOwner(this, Character) : nullptr)
	{
		Orb->HandleLaunchPress();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
