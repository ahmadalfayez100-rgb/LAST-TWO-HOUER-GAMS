// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Aim.h"

#include "Character/L2H_BaseCharacter.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"

UL2H_Ability_Aim::UL2H_Ability_Aim()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Predicted, not server only: the authority owns the walk speed, the owning machine owns the camera,
	// and both halves have to happen somewhere for aiming to read as one action.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	// The input row that fires this ability matches on a tag rather than on the class, so a pawn reaches
	// it through ServerTryActivateAbilityByTag like every other ability. Set on the CDO only, because
	// that is the object the ability system reads the tags from.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag AimTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Weapon.Aim")), false);
		if (AimTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(AimTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_Aim::ActivateAbility(
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

	UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>();
	if (!Weapons)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Aim refused: %s has no weapon component"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// A press always means "raise the sights". The input is a hold and this ability is only ever fired on
	// the press -- there is no release event to toggle back with -- so toggling a local flag meant every
	// second press lowered the speed and FOV while the button was still down, and wrote an aim state
	// opposite to the one the pawn derives from the held input. That left the laser and the fire gate
	// disagreeing with the aim pose. The pawn lowers all three halves when the input comes up.
	const bool bNowAiming = true;
	bAimingLocally = bNowAiming;

	float SpeedScale = FallbackAimSpeedScale;
	float FieldOfView = FallbackAimFOV;

	if (const UL2H_WeaponDataAsset* WeaponData = Weapons->GetActiveWeaponData())
	{
		SpeedScale = WeaponData->Data.AimSpeedScale;
		FieldOfView = WeaponData->Data.AimFOV;
	}

	// Applied on every machine that runs the ability, not only the authority. MaxWalkSpeed replicates with
	// COND_SkipOwner, so the owning machine has to compose its own value or its movement prediction drifts
	// away from the server's and the character rubber-bands while aiming.
	Character->SetAimSpeedScale(bNowAiming ? SpeedScale : 1.0f);

	if (HasServerAuthority())
	{
		Weapons->ServerSetAiming(bNowAiming);
	}

	// The camera exists only on the machine looking through it, so the FOV half is applied there and
	// nowhere else.
	if (Character->IsLocallyControlled())
	{
		Character->SetAimFieldOfView(bNowAiming ? FieldOfView : 0.0f);
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Aim %s: %s (speed x%.2f, fov %.0f)"),
		*GetNameSafe(Character),
		bNowAiming ? TEXT("raised") : TEXT("lowered"),
		bNowAiming ? SpeedScale : 1.0f,
		bNowAiming ? FieldOfView : 0.0f));

	Finish(false);
}
