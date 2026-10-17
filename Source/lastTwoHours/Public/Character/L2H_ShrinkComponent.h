// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "L2H_ShrinkComponent.generated.h"

class ACharacter;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Shrinks the owning character and puts him back.
 *
 * The state lives on the component rather than on the ability because the look has to outlive the press:
 * the ring keeps looping and the capsule stays small long after the ability has ended. bShrunk is the
 * replicated truth, so every machine resizes from one value instead of from a local prediction that a
 * late joiner would never receive.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent))
class LASTTWOHOURS_API UL2H_ShrinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_ShrinkComponent();

	UFUNCTION(BlueprintPure, Category = "L2H|Shrink")
	bool IsShrunk() const { return bShrunk; }

	UFUNCTION(BlueprintPure, Category = "L2H|Shrink")
	float GetShrinkScale() const { return ShrinkScale; }

	/**
	 * Sets the state on the authority and returns true only when the value actually changed, so a caller
	 * can report the transition without re-reading the state it just wrote.
	 */
	bool SetShrunk(bool bNewShrunk);

	/** True when the full-size capsule fits where he stands, so growing back cannot put him inside a ceiling. */
	bool HasRoomToGrow() const;

	/**
	 * How big this body reads to the rest of the game: actor scale times the shrink. The robot's sight and
	 * the no-shooting-while-small rule ask this, so either way of being small counts.
	 */
	static float GetBodyScale(const AActor* Who);

	/** Scaled half height of the capsule as it is right now, for anything that has to land on his feet. */
	UFUNCTION(BlueprintPure, Category = "L2H|Shrink")
	float GetCurrentCapsuleHalfHeight() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Small enough to read as a different silhouette, large enough that the floor still grounds him. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shrink", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ShrinkScale = 0.35f;

	/** Multiplies MaxWalkSpeed while small. 1.0 keeps him exactly as fast as he was. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shrink", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float ShrinkSpeedScale = 1.0f;

	/** Loops for as long as he is small: the waist ring and the floor ring from the reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shrink")
	TObjectPtr<UNiagaraSystem> ShrinkRingSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ShrinkRingComponent;

	UPROPERTY(ReplicatedUsing = OnRep_Shrunk)
	bool bShrunk = false;

	UFUNCTION()
	void OnRep_Shrunk();

private:
	/** Rebuilds capsule, mesh and eye height from bShrunk. Safe to call on any machine, any number of times. */
	void ApplyState();

	/** Remembers the full-size numbers once, so restoring is exact rather than a division. */
	void CaptureBaseState();

	float BaseCapsuleRadius = 34.0f;
	float BaseCapsuleHalfHeight = 88.0f;
	float BaseMeshRelativeZ = -88.0f;
	FVector BaseMeshRelativeScale = FVector::OneVector;
	float BaseEyeHeight = 64.0f;
	bool bBaseCaptured = false;

	/** The state ApplyState last built, so the speed and the floor offset change once per transition. */
	bool bAppliedShrunk = false;
};
