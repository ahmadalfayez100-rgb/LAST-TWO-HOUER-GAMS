// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

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
class LASTTWOHOURS_API AL2H_TimeShard : public AActor, public IL2H_InteractableInterface
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

	// A shard placed in the level is a hidden clock: taking it is remembered by the checkpoints like a
	// pickup, so a rewind puts it back and a save after taking it keeps it gone. A spilled shard has no
	// identity (it is not part of the level) and is left out.
	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override {}
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

protected:
	virtual void BeginPlay() override;

	/** Shown or hidden for a placed shard that was taken; a spilled shard is destroyed instead. */
	void SetTaken(bool bNewTaken);

	/** Where a placed shard sits in the level, so a rewind puts it back after the magnet moved it. */
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	bool bTaken = false;

	UPROPERTY(Replicated)
	bool bPlaced = false;

	protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Shard")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Shard")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How close a pawn has to be for the shard to be taken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shard", meta = (ClampMin = "10.0"))
	float PickupRadius = 90.0f;

	/** Seconds handed over. Set on a placed shard (a hidden clock); the spill sets its own. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "L2H|Shard", meta = (ClampMin = "0.0"))
	float TimeValue = 20.0f;

	/** 0 = never fades (the default for a placed shard). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "L2H|Shard", meta = (ClampMin = "0.0"))
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
