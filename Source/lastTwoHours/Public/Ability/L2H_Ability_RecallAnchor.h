// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "L2H_Ability_RecallAnchor.generated.h"

class AL2H_RecallAnchor;

/**
 * Places Samer's return mark, then brings him back to it.
 *
 * One button, one toggle: the first press draws the mark where he stands, the second press returns him to
 * it and spends the mark. A separate recall button would have to answer "which mark" every time, and
 * there is only ever one.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_Ability_RecallAnchor : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_RecallAnchor();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	/** The mark itself. Set on the ability's Blueprint so the visuals live in data, not in code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Recall")
	TSubclassOf<AL2H_RecallAnchor> AnchorClass;

	/** How far ahead of the player the mark is dropped, so it is not hidden under his feet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Recall", meta = (ClampMin = "0.0"))
	float PlaceForwardOffset = 90.0f;

	/** How far down to look for the floor under the mark. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Recall", meta = (ClampMin = "10.0"))
	float FloorTraceDistance = 250.0f;

private:
	/** The mark this character placed, or null when he has not left one. */
	AL2H_RecallAnchor* FindAnchorFor(const APawn* Character) const;
};
