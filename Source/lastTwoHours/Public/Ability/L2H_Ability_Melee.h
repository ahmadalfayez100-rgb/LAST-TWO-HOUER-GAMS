// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Melee.generated.h"

class AL2H_BaseCharacter;
class UL2H_WeaponDataAsset;

/**
 * A melee swing: the fire ability of a melee weapon (the pipe), set as its FireAbilityOverride, so it needs
 * no aim and spends no rounds -- the free way to fight, against the rounds the clock pays for.
 *
 * Presses chain through the weapon's CharacterAttack clips as a combo while each comes within ComboWindow
 * of the last; a press while a swing is still committed is ignored. Partway through the clip (HitFraction)
 * a sphere in front of the swinger looks for enemies: each takes the weapon's Damage and is stunned, and a
 * stunned enemy can be finished with Interact.
 */
UCLASS(meta = (DisplayName = "L2H Ability Melee"))
class LASTTWOHOURS_API UL2H_Ability_Melee : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Melee();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Share of the clip at which the blow lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float HitFraction = 0.4f;

	/** Centre of the hit sphere in front of the swinger, and its radius (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "0.0"))
	float HitReach = 110.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "10.0"))
	float HitRadius = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "0.0"))
	float StunSeconds = 1.6f;

	/** A press this long after a swing ends still continues the combo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "0.0"))
	float ComboWindow = 0.6f;

	/** Share of a swing after which the next press is accepted (the rest of the clip is its follow-through). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Melee", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float CommitFraction = 0.65f;

private:
	static void ApplyHit(AL2H_BaseCharacter* Swinger, const UL2H_WeaponDataAsset* Weapon, float Reach, float Radius, float Stun);

	int32 ComboIndex = -1;
	float BusyUntil = 0.0f;
	float LastSwingEnd = -100.0f;
};
