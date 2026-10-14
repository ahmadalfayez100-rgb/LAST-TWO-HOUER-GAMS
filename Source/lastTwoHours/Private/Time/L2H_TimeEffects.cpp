// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_TimeEffects.h"

#include "Time/L2H_TimeAttributeSet.h"

UL2H_GE_TimeEffectBase::UL2H_GE_TimeEffectBase()
{
	// Instant unless a subclass says otherwise. Only the drain is non-instant, and a duration that
	// is never checked is the safer default: an instant effect cannot be left stuck on a pawn.
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

void UL2H_GE_TimeEffectBase::AddTimeModifier(const FName& MagnitudeName)
{
	AddAttributeModifier(UL2H_TimeAttributeSet::GetTimeAttribute(), MagnitudeName);
}

void UL2H_GE_TimeEffectBase::AddAttributeModifier(const FGameplayAttribute& Attribute, const FName& MagnitudeName)
{
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::AddBase;

	FSetByCallerFloat SetByCaller;
	// DataName rather than DataTag on purpose: a plain FName needs no gameplay-tag manager lookup,
	// so this can run while class default objects are constructed during module load.
	SetByCaller.DataName = MagnitudeName;

	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}

UL2H_GE_TimeDrain::UL2H_GE_TimeDrain()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// One second period: the drain is a per-second rate, not a per-frame one, so every client can
	// predict nothing and the server stays the only writer.
	Period.Value = 1.0f;

	// Execute on application as well, so the clock starts in the frame the effect lands instead of
	// one second later -- which is what makes a ten second measurement read ten.
	bExecutePeriodicEffectOnApplication = true;

	AddTimeModifier(TEXT("Data.Time.Drain"));
}

UL2H_GE_TimeCost::UL2H_GE_TimeCost()
{
	AddTimeModifier(TEXT("Data.Time.Amount"));
}

UL2H_GE_TimeDamage::UL2H_GE_TimeDamage()
{
	AddTimeModifier(TEXT("Data.Time.Amount"));
}

UL2H_GE_TimeRestore::UL2H_GE_TimeRestore()
{
	AddTimeModifier(TEXT("Data.Time.Amount"));
}

UL2H_GE_HealthDamage::UL2H_GE_HealthDamage()
{
	// Straight onto the scratch attribute. The attribute set owns the conversion, so this effect
	// never has to know what the health ceiling is or what a point of health is worth in seconds.
	AddAttributeModifier(UL2H_TimeAttributeSet::GetIncomingDamageAttribute(), TEXT("Data.Health.Damage"));
}

UL2H_GE_HealthRestore::UL2H_GE_HealthRestore()
{
	AddAttributeModifier(UL2H_TimeAttributeSet::GetHealthAttribute(), TEXT("Data.Health.Amount"));
}
