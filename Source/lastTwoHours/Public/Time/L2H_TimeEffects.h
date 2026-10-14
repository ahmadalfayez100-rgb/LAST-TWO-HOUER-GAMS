// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "L2H_TimeEffects.generated.h"

/**
 * Shared base for every TimeCore time effect.
 *
 * Every one of them changes exactly one attribute with exactly one modifier, so the modifier is
 * assembled once here and a subclass only chooses its duration policy, which attribute it moves,
 * and which SetByCaller name carries the magnitude.
 *
 * Time is the common case and gets the named AddTimeModifier shorthand; the Health effects below
 * reach for AddAttributeModifier instead. Both end up in the same single-modifier shape, which is
 * what keeps "a magnitude is never baked into an effect" true for every one of them.
 *
 * The magnitudes are deliberately NOT baked into the effect. The level's UL2H_LevelTimeConfig owns
 * how much a hit, a cost or a transfer is worth, and passes it in at application time, so retuning
 * a cost never means touching an effect asset.
 */
UCLASS(Abstract)
class LASTTWOHOURS_API UL2H_GE_TimeEffectBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UL2H_GE_TimeEffectBase();

protected:
	/** Appends the single Time modifier, taking its magnitude from MagnitudeName. */
	void AddTimeModifier(const FName& MagnitudeName);

	/** Appends one AddBase modifier on Attribute, taking its magnitude from MagnitudeName. */
	void AddAttributeModifier(const FGameplayAttribute& Attribute, const FName& MagnitudeName);
};

/**
 * Always-on drain. Infinite, one second period, magnitude on Data.Time.Drain.
 *
 * This is the only effect in the game that is never removed: the clock runs from the moment a
 * player spawns until the run ends.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_TimeDrain : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_TimeDrain();
};

/** Instant payment for a super ability. Magnitude on Data.Time.Amount. */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_TimeCost : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_TimeCost();
};

/** Instant damage. Hazards, falls and impacts all use this. Magnitude on Data.Time.Amount. */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_TimeDamage : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_TimeDamage();
};

/** Instant recovery, including handing time to a partner. Magnitude on Data.Time.Amount. */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_TimeRestore : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_TimeRestore();
};

// -----------------------------------------------------------------------------------------------
// Health. The cushion in front of the clock: a hit costs Health, and the director separately
// translates the Health actually lost into seconds. These two effects move the numbers; deciding
// what a hit is worth stays with the level config and the director.
// -----------------------------------------------------------------------------------------------

/**
 * Instant health damage. Magnitude on Data.Health.Damage.
 *
 * The modifier targets IncomingDamage, not Health. A hit lands on the scratch attribute and
 * UL2H_TimeAttributeSet::PostGameplayEffectExecute turns it into the Health loss, so the subtraction
 * is resolved against the current value in exactly one place -- a heal and a hit applied in the same
 * frame can no longer disagree about what Health was.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_HealthDamage : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_HealthDamage();
};

/** Instant healing, including the revive path. Magnitude on Data.Health.Amount. */
UCLASS()
class LASTTWOHOURS_API UL2H_GE_HealthRestore : public UL2H_GE_TimeEffectBase
{
	GENERATED_BODY()

public:
	UL2H_GE_HealthRestore();
};
