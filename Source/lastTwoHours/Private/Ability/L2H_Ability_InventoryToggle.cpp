// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_InventoryToggle.h"

#include "Character/L2H_BaseCharacter.h"
#include "Item/L2H_InventoryComponent.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_InventoryToggle::UL2H_Ability_InventoryToggle()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Predicted. The request travels through the server like every other ability, but the screen belongs to
	// whoever is holding the pawn, so the ability has to be able to run there as well.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	// The input row that fires this ability matches on a tag rather than on the class, so a pawn reaches
	// it through ServerTryActivateAbilityByTag like every other ability. Set on the CDO only, because
	// that is the object the ability system reads the tags from.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag InventoryTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Inventory.Toggle")), false);
		if (InventoryTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(InventoryTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_InventoryToggle::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const auto Finish = [this, Handle, ActorInfo, ActivationInfo](bool bWasCancelled)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, bWasCancelled);
	};

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		Finish(true);
		return;
	}

	// Only where the pawn is actually being played. The server-side run of a remote client's toggle would
	// otherwise try to build a widget for a screen it cannot draw on.
	if (!Character->IsLocallyControlled())
	{
		Finish(false);
		return;
	}

	UL2H_InventoryComponent* Inventory = Character->FindComponentByClass<UL2H_InventoryComponent>();
	if (!Inventory)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Inventory refused: %s has no inventory component"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	Inventory->ToggleInventoryUI();

	Finish(false);
}
