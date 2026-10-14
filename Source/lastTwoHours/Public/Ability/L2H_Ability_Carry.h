// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Carry.generated.h"

class AL2H_BaseCharacter;
class UL2H_LevelTimeConfig;

/**
 * Carry: pick up, hold and put down.
 *
 * A toggle on one button. The ability itself is instant -- the held state lives on the pawn, which is
 * what makes "press again to drop" work without an ability left running for the whole carry. The
 * mass rules decide who may lift what:
 *
 *   mass <= SingleCarryMass          one player, slower the heavier it is
 *   mass <= TwoPlayerCarryMass       needs the second player nearby, alive
 *   mass >  TwoPlayerCarryMass       immovable, and the refusal is logged
 *
 * Carrying a downed partner goes down the same path, because a downed partner is just another
 * actor with a mass profile.
 *
 * Free by design: carrying never costs time. The extra drain for hauling a downed partner lives on
 * the level config and is applied by the time director, not here.
 */
UCLASS(meta = (DisplayName = "L2H Ability Carry"))
class LASTTWOHOURS_API UL2H_Ability_Carry : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Carry();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Grab reach. Shorter than the hook: carrying is hands-on by design. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "50.0"))
	float CarryRange = 250.0f;

	/** Walk-speed multiplier at exactly SingleCarryMass. 1.0 means no penalty at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float FastestCarrySpeedScale = 1.0f;

	/** Walk-speed multiplier at exactly SingleCarryMass. Heavier is always slower. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HeaviestSoloSpeedScale = 0.55f;

	/** True when another player is close enough and able to help with a two-player load. */
	bool HasCarryPartner(const AL2H_BaseCharacter* Character, const UL2H_LevelTimeConfig* Config) const;
};
