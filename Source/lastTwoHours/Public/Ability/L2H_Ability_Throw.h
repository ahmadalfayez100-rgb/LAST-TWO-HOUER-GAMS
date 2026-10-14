// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Throw.generated.h"

/**
 * Puts the carried object down the aim line.
 *
 * The throw is deliberately not its own input. Aim already raises the sights and fire already means
 * "act now", so while something is carried those two verbs mean aim-the-throw and release-the-throw.
 * That is one meaning per key at any moment, which is what keeps carry, throw and shooting from ever
 * contending for a button.
 *
 * Server only: an impulse is authoritative physics, and a client that threw for itself would have the
 * object snapped back the moment the server disagreed.
 */
UCLASS(meta = (DisplayName = "L2H Ability Throw"))
class LASTTWOHOURS_API UL2H_Ability_Throw : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Throw();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
