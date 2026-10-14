// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_TimeShard.generated.h"

class AL2H_BaseCharacter;
class USphereComponent;
class UStaticMeshComponent;

/**
 * A spilled fragment of time.
 *
 * The director drops one whenever a single hit takes ShardThreshold or more off a player. It exists
 * so a big loss stays recoverable: the seconds leave the victim's clock but remain in the world, and
 * anyone -- including whoever caused the hit -- can walk over and take them back.
 *
 * The pickup rule belongs to the pawn, not to the shard: the shard only asks whether the character
 * still has room under the level's MaxCarriedShards. That is what keeps "one shard at a time" a
 * property of the player rather than of the prop, and what makes the refusal meaningful.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Time Shard"))
class LASTTWOHOURS_API AL2H_TimeShard : public AActor
{
	GENERATED_BODY()

public:
	AL2H_TimeShard();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds this shard hands over when it is absorbed. */
	UFUNCTION(BlueprintPure, Category = "L2H|Shard")
	float GetTimeValue() const { return TimeValue; }

	/** Seconds left before it fades. 0 means it never expires. */
	UFUNCTION(BlueprintPure, Category = "L2H|Shard")
	float GetRemainingLifetime() const { return RemainingLifetime; }

	/** Called by the director on the spill path, right after the spawn. */
	void InitialiseFromConfig(float InTimeValue, float InLifetime);

	/**
	 * Server only. The victim a spilled shard came out of cannot collect it for Seconds. Without this the
	 * magnet hands the seconds straight back on the next tick, and a hit costs less than it says it does.
	 */
	void IgnoreCollectorFor(AActor* InIgnored, float Seconds);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Shard")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Shard")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How close a pawn has to be for the shard to be taken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shard", meta = (ClampMin = "10.0"))
	float PickupRadius = 90.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Shard")
	float TimeValue = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Shard")
	float RemainingLifetime = 0.0f;

	/** Stops two pawns absorbing the same shard on the same frame. */
	UPROPERTY(Transient)
	bool bConsumed = false;

	/** Throttle for the "you are already full" line, so a refusal cannot spam the log. */
	UPROPERTY(Transient)
	float LastRefusalLogTime = -1000.0f;

	/** The pawn this shard came out of, ignored by the magnet until IgnoreCollectorUntil. */
	TWeakObjectPtr<AActor> IgnoredCollector;

	UPROPERTY(Transient)
	float IgnoreCollectorUntil = -1.0f;

	/** Server only. Drifts the shard toward the nearest pawn that still has room for it. */
	void TickMagnet(float DeltaSeconds);

	/** Nearest registered pawn in range with room for a shard, or null. */
	AL2H_BaseCharacter* FindCollector(float MagnetRange) const;

	/** Hands TimeValue over and destroys the shard. False when the pawn turned out to be full. */
	bool TryAbsorb(AL2H_BaseCharacter* Character);
};
