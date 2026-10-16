// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_ChronoBallLaunch.generated.h"

class AL2H_TimeBall;

/**
 * Samer's launch button: commit to where the Chrono Ball is.
 *
 * It is a class of its own because a row of AbilityInputBindings carries one input action and one
 * gameplay tag and no verb at all -- ServerTryActivateAbilityByTag hands ActivateAbility no
 * TriggerEventData to read. The orb therefore needs one class per meaning: taking it out and putting it
 * away on Ability.ChronoBall, and the release and the teleport on this one.
 *
 * Nothing is charged here. Taking the orb out already paid the flat cost, and the hold is what the player
 * spent that on; this only decides what happens to an orb that is already real.
 */
UCLASS(meta = (DisplayName = "L2H Ability Chrono Ball Launch"))
class LASTTWOHOURS_API UL2H_Ability_ChronoBallLaunch : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_ChronoBallLaunch();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
