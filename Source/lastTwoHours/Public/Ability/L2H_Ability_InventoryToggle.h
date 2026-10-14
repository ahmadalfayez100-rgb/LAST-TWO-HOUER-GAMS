// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_InventoryToggle.generated.h"

/**
 * Opens and closes the pawn's inventory screen.
 *
 * The screen itself already exists as UL2H_InventoryComponent::ToggleInventoryUI; this ability is only the
 * route to it from the same input array every other ability uses, so opening the bag is a loadout row and
 * not a special case in SetupPlayerInputComponent.
 */
UCLASS(meta = (DisplayName = "L2H Ability Inventory Toggle"))
class LASTTWOHOURS_API UL2H_Ability_InventoryToggle : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_InventoryToggle();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
