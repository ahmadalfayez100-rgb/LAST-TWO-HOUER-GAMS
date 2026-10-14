// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_WeaponSwitch.h"

#include "Character/L2H_BaseCharacter.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponComponent.h"

UL2H_Ability_WeaponSwitch::UL2H_Ability_WeaponSwitch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	// The input row that fires this ability matches on a tag rather than on the class, so a pawn reaches
	// it through ServerTryActivateAbilityByTag like every other ability. Set on the CDO only, because
	// that is the object the ability system reads the tags from.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag SwitchTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Weapon.Switch")), false);
		if (SwitchTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(SwitchTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_WeaponSwitch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const auto Finish = [this, Handle, ActorInfo, ActivationInfo](bool bWasCancelled)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, bWasCancelled);
	};

	if (!HasServerAuthority())
	{
		Finish(true);
		return;
	}

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		Finish(true);
		return;
	}

	UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>();
	if (!Weapons)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Weapon switch refused: %s has no weapon component"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// A one-weapon loadout cycles to itself and logs the reason rather than appearing broken.
	if (!Weapons->ServerCycleWeapon())
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Weapon switch: %s has %d loadout entr%s"),
			*GetNameSafe(Character),
			Weapons->GetLoadoutSize(),
			Weapons->GetLoadoutSize() == 1 ? TEXT("y") : TEXT("ies")));
	}

	Finish(false);
}
