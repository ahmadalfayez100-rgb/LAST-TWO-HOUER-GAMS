// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_CheckpointTerminal.generated.h"

class AL2H_BaseCharacter;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UL2H_StationMarker;

/** Which half of the checkpoint system a terminal belongs to. */
UENUM(BlueprintType)

enum class EL2H_CheckpointTier : uint8
{
	/** The optional checkpoint: you choose to stop here, and paying is what makes it a reference point. */
	Spot UMETA(DisplayName = "Spot (optional)"),

	/** The mandatory anchor: it records a checkpoint on arrival, free and without being asked. */
	Hub UMETA(DisplayName = "Hub (mandatory)")
};

/**
 * A place the players walk to: the designer's hub, or the optional spot they place themselves.
 *
 * The tier is the whole design, so it lives in one enum rather than in two actor classes:
 *
 *   Spot -- the optional checkpoint. Walking in records nothing; standing here opens the same menu
 *           the hub does, and paying the save cost is what turns it into a reference point the run
 *           can travel back to. It is somewhere you choose to stop.
 *   Hub  -- the level's anchor. Stepping in writes a checkpoint without being asked and without a
 *           charge, because a mandatory save is the game's promise, not a purchase.
 *
 * Both tiers offer the menu: it is where the rewind list and the designer-time offer live, and a
 * player standing at either place wants them. The tier decides only whether arriving is itself a
 * save.
 *
 * The terminal owns no time and no save logic. It decides *who is standing here and what this place
 * offers*, and hands every action to the director or the save subsystem.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Checkpoint Terminal"))
class LASTTWOHOURS_API AL2H_CheckpointTerminal : public AActor
{
	GENERATED_BODY()

public:
	AL2H_CheckpointTerminal();

	/** Which half of the system this instance is. Set per placed actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	EL2H_CheckpointTier Tier = EL2H_CheckpointTier::Spot;

	/**
	 * Stable identity the save system keys this place on. Leave empty to use the actor's own name.
	 *
	 * An explicit id exists so a checkpoint can be moved in the level, or rebuilt onto a different
	 * mesh, without the run losing the record of having been there -- the actor's name would change
	 * with it. It is also what the run's travel list names, so it should stay readable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	FName CheckpointId;

	/** Shown as the menu header. Empty falls back to a generic title in the widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	FText DisplayName;

	/**
	 * Hub only: the clock each player starts the next stage with, once both have arrived. 0 uses the
	 * level config's StartingTime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint", meta = (ClampMin = "0.0"))
	float NextSegmentTime = 0.0f;

	/** True when standing here opens the terminal menu. Both tiers do. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	bool OffersMenu() const { return true; }

	/** The identity the save system files this place under. Never empty. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	FName GetCheckpointId() const { return CheckpointId.IsNone() ? GetFName() : CheckpointId; }

	/** True when walking in writes a checkpoint on its own and free. Only the hub; a spot is paid. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	bool SavesOnArrival() const { return Tier == EL2H_CheckpointTier::Hub; }

	/** True while the given pawn is inside the range. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	bool IsPawnInRange(const AL2H_BaseCharacter* Character) const;

	/** Fired on entry, for Blueprint-side polish (lights, audio). */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Checkpoint")
	void OnPawnEntered(AL2H_BaseCharacter* Character);

	/** Fired on exit. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Checkpoint")
	void OnPawnExited(AL2H_BaseCharacter* Character);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** The volume a player has to be inside for this terminal to count. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	TObjectPtr<UBoxComponent> Range;

	/** Decoration only; the mesh is set on the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Coloured pad, pole and label: green for a hub, blue for a save spot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint")
	TObjectPtr<UL2H_StationMarker> Marker;

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AL2H_BaseCharacter>> PawnsInRange;
};
