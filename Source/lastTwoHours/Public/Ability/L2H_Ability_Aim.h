// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Aim.generated.h"

/**
 * Raises and lowers the sights. One press on, the next press off.
 *
 * A toggle rather than a hold, because the pawn exposes no "input released" event and this ability is
 * reached through the server-activation input path the other abilities use. The carried object is already
 * a toggle on the same key, so aiming follows the idiom the game already has instead of inventing one.
 *
 * Aiming has two halves that live on different machines: the speed is movement state the authority owns, and
 * the field of view belongs to the camera, which only exists where the pawn is being looked through. Each
 * half is applied where it belongs, which is why this runs predicted rather than server only.
 */
UCLASS(meta = (DisplayName = "L2H Ability Aim"))
class LASTTWOHOURS_API UL2H_Ability_Aim : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Aim();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Used when the held weapon authored no AimSpeedScale of its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Aim", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float FallbackAimSpeedScale = 0.5f;

	/** Used when the held weapon authored no AimFOV of its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Aim", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float FallbackAimFOV = 55.0f;

	/**
	 * This ability's own memory of the toggle.
	 *
	 * Read back from the weapon component instead, and a client's second press could arrive before the
	 * replicated aim flag does -- and would toggle to the state it is already in.
	 */
	UPROPERTY(Transient)
	bool bAimingLocally = false;
};
