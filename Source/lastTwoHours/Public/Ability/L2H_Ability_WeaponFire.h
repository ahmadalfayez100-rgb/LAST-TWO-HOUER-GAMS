// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "L2H_Ability_WeaponFire.generated.h"

/**
 * Spends one round of whatever the pawn is holding, and takes time off whatever it hit.
 *
 * Server only, because the trace has to agree across machines and the time it takes off is a shared
 * number. The client's part is the aiming the player already did.
 */
UCLASS(meta = (DisplayName = "L2H Ability Weapon Fire"))
class LASTTWOHOURS_API UL2H_Ability_WeaponFire : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_WeaponFire();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Visibility by default, so a weapon never shoots through the wall it is standing against. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|WeaponFire")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * Applied when the trace lands on the head bone and the target authored no response of its own.
	 *
	 * The authored multiplier lives on the target's damage response, keyed by EL2H_DamageType. This is the
	 * fallback that keeps an un-authored target hittable instead of merely not multiplying.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|WeaponFire", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 2.0f;

	/** The Mannequin's head bone, authored so a different rig can name its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|WeaponFire")
	FName HeadBoneName = TEXT("head");

private:
	/** World time of the last shot this pawn fired, for the weapon's FireRate. Instanced per actor. */
	float LastShotTime = -1000.0f;
};
