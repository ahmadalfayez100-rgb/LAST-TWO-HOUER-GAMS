// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_WeaponReload.h"

#include "Character/L2H_BaseCharacter.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"

UL2H_Ability_WeaponReload::UL2H_Ability_WeaponReload()
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
		const FGameplayTag ReloadTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Weapon.Reload")), false);
		if (ReloadTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(ReloadTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_WeaponReload::ActivateAbility(
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
		L2HTime::LogImportant(TEXT("Reload refused: the avatar is not an L2H character"));
		Finish(true);
		return;
	}

	UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>();
	if (!Weapons)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Reload refused: %s has no weapon component"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// Refused cases are logged by the component, which is the side that knows why -- no magazine, or
	// already full. Nothing left to report from here.
	// Animated only when the refill actually happened: the component refuses an empty magazine and an
	// already-full one, and a reload animation for a refused reload would be a lie.
	if (Weapons->ServerRefillClip())
	{
		Weapons->PlayReloadAnimation();
	}

	Finish(false);
}
