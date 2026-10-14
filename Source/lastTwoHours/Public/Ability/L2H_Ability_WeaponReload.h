// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_WeaponReload.generated.h"

/**
 * Refills the magazine of whatever the pawn is holding.
 *
 * A pipe authored with ClipSize 0 has no magazine, so a reload on one is refused rather than silently
 * doing nothing -- the log line is what tells a designer the weapon was authored without one.
 */
UCLASS(meta = (DisplayName = "L2H Ability Weapon Reload"))
class LASTTWOHOURS_API UL2H_Ability_WeaponReload : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_WeaponReload();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
