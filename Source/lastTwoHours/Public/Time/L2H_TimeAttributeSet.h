// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "L2H_TimeAttributeSet.generated.h"

/**
 * Time is life AND energy, and Health is the cushion in front of it.
 *
 * Time is the run: one counter per player, 0 means Downed, and it never kills. Health sits in front
 * of it and absorbs the individual mistake -- a fall, a bullet, a bad landing -- so that one bad
 * moment costs a slice of the run instead of all of it. A point of Health lost also costs
 * TimePerHealthPoint seconds, which is what keeps a mistake meaningful rather than free.
 *
 * Health running out IS death. That is the one way a run ends without the clock reaching zero, and
 * it is deliberately a separate flag from Downed: AreAllPlayersDowned() must not read a dead player
 * as merely downed, or a wipe would be misread as a recoverable pause.
 *
 * Every mutation is server-authoritative. The HUD only ever reads the replicated value,
 * which is why nothing here is client-writable.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_TimeAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UL2H_TimeAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * Clamps the BASE value. Every time effect is AddBase, and PreAttributeChange only clamps the
	 * current value -- without this the base keeps sinking below 0 while the drain runs on a
	 * downed player, and a later restore can never lift them back above 0.
	 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** Seconds of life/energy remaining. Reaching 0 means Downed, never death. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", ReplicatedUsing = OnRep_Time)
	FGameplayAttributeData Time;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, Time)

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", ReplicatedUsing = OnRep_MaxTime)
	FGameplayAttributeData MaxTime;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, MaxTime)

	/** Passive drain in seconds per second, driven by the level config (default 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", ReplicatedUsing = OnRep_TimeDrainRate)
	FGameplayAttributeData TimeDrainRate;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, TimeDrainRate)

	// ---------------------------------------------------------------------------------------
	// Health. The cushion in front of Time: it absorbs a hit, and translates that hit into
	// seconds in the same breath.
	// ---------------------------------------------------------------------------------------

	/** Hit points. 0 Health is death -- the one way a run ends with the clock still running. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, Health)

	/** Ceiling Health is clamped to. 0 disables the health layer entirely, so a target that only
	 *  ever wanted the clock still takes pure time damage. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, MaxHealth)

	/**
	 * Scratch damage, in health points. A hit lands HERE first and PostGameplayEffectExecute turns it
	 * into the Health loss, which is what makes the amount that killed a player readable in one place
	 * instead of having to be inferred back out of two attribute deltas.
	 *
	 * Deliberately not replicated. It is written and consumed on the authority inside a single effect
	 * execution, and a client has nothing to do with the number once it has been applied.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Health")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UL2H_TimeAttributeSet, IncomingDamage)

	/** 0..1 for a health bar. 1.0 whenever the health layer is disabled, so the bar reads "full". */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	float GetHealthPercent() const
	{
		const float Maximum = GetMaxHealth();
		return Maximum > 0.0f ? FMath::Clamp(GetHealth() / Maximum, 0.0f, 1.0f) : 1.0f;
	}

protected:
	UFUNCTION()
	virtual void OnRep_Time(const FGameplayAttributeData& OldTime);

	UFUNCTION()
	virtual void OnRep_MaxTime(const FGameplayAttributeData& OldMaxTime);

	UFUNCTION()
	virtual void OnRep_TimeDrainRate(const FGameplayAttributeData& OldTimeDrainRate);

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
