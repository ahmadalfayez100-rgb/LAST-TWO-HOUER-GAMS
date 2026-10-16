// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_TimeBend.generated.h"

class AActor;
class UAnimMontage;

/**
 * Yousef's fixed super ability: slow an arena around him for a while.
 *
 * The arena is a diameter rather than the whole level, so BendDiameter is the single number the ability
 * is tuned with: it decides what is actually slowed, and it is handed to the cast effect as a scale so
 * the ring drawn on the ground and the area that runs slow can never drift apart.
 *
 * Inside the diameter, actors are slowed one at a time through the world subsystem, which caches and
 * restores each actor's original dilation -- so the arena can follow Yousef as he moves without ever
 * compounding the slow, and ending it always restores exactly the actors this bend touched, whichever
 * way it ends (duration, a cancel, or Yousef reaching zero time). Yousef is never included in the list.
 *
 * What this deliberately gives up: AWorldSettings::TimeDilation is one number for the whole level and
 * cannot be cut to a circle, so a bend no longer slows simulated physics the way a world-wide slow did.
 * Only tick-driven actors can be slowed individually.
 */
UCLASS(meta = (DisplayName = "L2H Ability Time Bend"))
class LASTTWOHOURS_API UL2H_Ability_TimeBend : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_TimeBend();

	/**
	 * Server: the button came up. With bHoldToSustain the bend ends here -- or once MinBendSeconds has
	 * passed, so a tap still gives a short slow. Without it, release changes nothing.
	 */
	void ReleaseHold();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Ends the bend from a timer, without a cancelled flag. */
	void HandleBendFinished();

	/** Charges the per-second cost and ends the bend if that left nothing. */
	void HandleCostTick();

	/** Every actor tagged with ExemptActorTag, so one object can keep running at normal speed. */
	TArray<AActor*> FindExemptActors() const;
	
	/** Every actor standing inside the arena right now, minus Yousef and minus the exempt ones. */
	TArray<AActor*> GatherArenaActors() const;
	
	/** Slows whoever has just stepped in, and gives normal speed back to whoever has just stepped out. */
	void RefreshArena();
	
	/** Half of BendDiameter, in cm. */
	float GetBendRadius() const { return BendDiameter * 0.5f; }
	
	/**
	 * The diameter the cast effect asset is authored at, in cm. BendDiameter is divided by this to get the
	 * scale handed to the effect, which is what ties the two sizes together.
	 */
	static constexpr float BendEffectReferenceDiameter = 500.0f;
	
	/** How much of normal speed anything caught inside the arena keeps. 0.25 = a quarter speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float BendDilation = 0.25f;
	
	/**
	 * How wide the arena is, measured across, in cm. 500 = five metres.
	 *
	 * The one number that tunes the bend: it decides both the area that is slowed and the size the cast
	 * effect is drawn at, so the two cannot disagree. Retune it here and both follow.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "100.0", UIMin = "100.0", UIMax = "4000.0"))
	float BendDiameter = 500.0f;
	
	/** Seconds between membership refreshes, so the arena follows Yousef instead of freezing at cast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ArenaRefreshInterval = 0.25f;

	/** How long one activation lasts, in seconds of normal time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.1"))
	float BendDuration = 5.0f;

	/** Charged once when the bend starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.0"))
	float CostFlat = 2.0f;

	/** Charged every second the world is slowed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.0"))
	float CostPerSecond = 1.0f;

	/** Below this much time left the bend is refused outright, so it cannot be started into a downed state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.0"))
	float MinimumTimeToStart = 3.0f;

	/**
	 * Actors carrying this tag keep normal speed while the world is slowed. That is the "exempt one
	 * object" of the design, and it costs a tag rather than a new actor class.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend")
	FName ExemptActorTag = FName(TEXT("TimeBendExempt"));

	/** Seconds between the per-second cost ticks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.1"))
	float CostTickInterval = 1.0f;
	
	/**
	 * One-shot clip played on Yousef the moment the world slows -- the casting pose that explains why the
	 * arena appeared. Cosmetics only: the dilation is the truth and travels on its own, so an absent or
	 * dropped clip costs a pose and never the bend.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend")
	TObjectPtr<UAnimMontage> CastMontage;

	/**
	 * The bend lasts while the button is held (up to MaxHoldSeconds) instead of a fixed BendDuration.
	 * A trial setting: the drain below is what pays for it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold")
	bool bHoldToSustain = true;

	/** A tap still slows for this long. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold", meta = (ClampMin = "0.1"))
	float MinBendSeconds = 1.0f;

	/** Safety cap on one hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold", meta = (ClampMin = "1.0"))
	float MaxHoldSeconds = 30.0f;

	/**
	 * While held, Yousef's clock runs this many times faster than normal at the start. 1.2 = every real
	 * second takes 1.2 s off his clock (the normal second plus 0.2 s for the bend).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold", meta = (ClampMin = "1.0"))
	float HoldDrainStart = 1.2f;

	/** How much the drain rate grows per second the button stays down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold", meta = (ClampMin = "0.0"))
	float HoldDrainRampPerSecond = 0.1f;

	/** The drain rate never climbs past this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend|Hold", meta = (ClampMin = "1.0"))
	float HoldDrainMax = 2.0f;

	/** The drain rate right now (1 = the clock's own pace, no extra). */
	float GetHoldDrainRate() const;

	FTimerHandle BendTimer;
	FTimerHandle CostTimer;

	/** World time the bend started at; the hold drain ramps from here. */
	float BendStartTime = 0.0f;
	FTimerHandle ArenaTimer;
	
	/** The slowed actor, cached so EndAbility can restore the arena after the avatar is gone. */
	TWeakObjectPtr<AActor> BendCaster;
	
	TArray<TObjectPtr<AActor>> ExemptActors;
	
	/**
	 * Exactly the actors this bend has slowed, so the end restores those and nobody else -- including the
	 * ones that walked out of the arena before the bend finished.
	 */
	TArray<TObjectPtr<AActor>> SlowedActors;
};
