// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_TimeBend.generated.h"

class AActor;

/**
 * Yousef's fixed super ability: slow the world around him for a while.
 *
 * The slow itself is the world subsystem's job -- AWorldSettings::TimeDilation is replicated, so the
 * whole session follows the server without a single client-side line. This ability owns the window:
 * how long, how much, what it costs, and the promise that ending it always restores the original
 * dilation, whichever way it ends (duration, a cancel, or Yousef reaching zero time).
 *
 * Yousef himself stays at normal speed; the compensation is applied for him rather than assumed.
 */
UCLASS(meta = (DisplayName = "L2H Ability Time Bend"))
class LASTTWOHOURS_API UL2H_Ability_TimeBend : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_TimeBend();

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

	/** How much of normal speed the rest of the world keeps. 0.25 = a quarter speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|TimeBend", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float BendDilation = 0.25f;

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

	FTimerHandle BendTimer;
	FTimerHandle CostTimer;

	/** The slowed actor, cached so EndAbility can restore the world after the avatar is gone. */
	TWeakObjectPtr<AActor> BendCaster;

	TArray<TObjectPtr<AActor>> ExemptActors;
};
