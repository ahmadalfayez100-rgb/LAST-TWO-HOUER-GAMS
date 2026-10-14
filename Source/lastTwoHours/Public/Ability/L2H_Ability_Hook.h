// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Hook.generated.h"

/**
 * Hook: reach out and either drag the object to you or drag yourself to it.
 *
 * Which way it goes is a mass decision against the level's HookPullCapacity, so the same button
 * climbs a wall and reels in a crate. A profile can override the decision with bHookReelIn, which is
 * what turns a fixed anchor into something to swing from rather than something to collect.
 *
 * Free by design: the hook never costs time. Only the super abilities do.
 */
UCLASS(meta = (DisplayName = "L2H Ability Hook"))
class LASTTWOHOURS_API UL2H_Ability_Hook : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Hook();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** How far ahead the hook looks, measured from the camera. The camera sits ~2 m behind the body, so
	 *  1200 is about 10 m of reach from the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "50.0"))
	float HookRange = 1200.0f;

	/** Unused since the pull became a landing arc (velocity from distance, not a fixed impulse). Kept so
	 *  existing Blueprint overrides still load. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float PullImpulse = 120000.0f;

	/** Launch speed used when the player travels instead of the object. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float ReelSpeed = 1400.0f;

	/** Lifts the reel direction, so arriving at a ledge clears the lip instead of hitting the wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float ReelUpwardBias = 0.25f;
};
