// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_WeaponSwitch.generated.h"

/**
 * Steps to the next weapon in the pawn's loadout.
 *
 * One key cycles rather than one key per weapon, so a character with five weapons does not need five rows
 * on its input array -- and a loadout row added later needs no binding at all.
 */
UCLASS(meta = (DisplayName = "L2H Ability Weapon Switch"))
class LASTTWOHOURS_API UL2H_Ability_WeaponSwitch : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_WeaponSwitch();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
