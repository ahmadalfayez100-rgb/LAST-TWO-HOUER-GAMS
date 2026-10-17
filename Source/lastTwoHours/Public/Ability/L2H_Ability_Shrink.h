// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Shrink.generated.h"

/**
 * Toggles Samer's small state.
 *
 * A toggle rather than a hold because the ability has to end: the shrunk state is carried by the pawn's
 * component, so the player can walk, climb and use other abilities while small without this press being
 * held down.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_Ability_Shrink : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Shrink();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
