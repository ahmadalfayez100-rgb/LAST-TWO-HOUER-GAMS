// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_SaveGame.generated.h"

/** One player's slot inside a checkpoint. */
USTRUCT(BlueprintType)
struct FL2H_SavedPlayer
{
	GENERATED_BODY()

	/** Where the pawn stood when the checkpoint was written. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FTransform Transform = FTransform::Identity;

	/** Seconds of time it held. Restored as a delta through the director, never written raw. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	float Time = 0.0f;

	/** Kept so a checkpoint taken while a player was downed does not silently revive them. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	bool bDowned = false;

	/**
	 * Banked seconds (the shop currency). A rewind hands back the bank as it was here, so replaying a
	 * stage replaces its payout instead of stacking a second one on top.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	float Chrons = 0.0f;

	/** The bag as it was here. A rewind takes back whatever was picked up after this checkpoint. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TArray<FL2H_ItemInstance> InventorySlots;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TArray<FL2H_ItemInstance> InventoryShortcuts;

	/** Health as it was here. A rewind used to refill it, which made dying a free heal. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	float Health = -1.0f;

	/** The last hub whose stage this player finished, so the clock knows which hub is next after a rewind. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FName LastHubId;
};

/**
 * One placed interactable, as a checkpoint recorded it.
 *
 * Name/flag pairs rather than one field per actor class, because the save layer has no business knowing
 * that a door has an "open" and a "locked" while a drawer has only a "locked". An interactable reports
 * whatever names it likes and reads back only the ones it recognises, so a new interactable type saves
 * without this struct changing -- and an old slot still loads after an actor gains a flag it did not use
 * to have.
 */
USTRUCT(BlueprintType)
struct FL2H_WorldStateRecord
{
	GENERATED_BODY()

	/** Stable identity of the placed actor, from IL2H_InteractableInterface::L2H_GetWorldStateId. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FGuid Id;

	/**
	 * The actor's path at the moment of recording.
	 *
	 * Kept as a fallback for an actor whose authored id was changed between the save and the load. That is
	 * a designer's edit rather than a bug, and it should not cost the players a world state.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FString ActorPath;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TMap<FName, bool> Flags;
};

/**
 * One visited checkpoint: a place the players have stood, and what was recorded there.
 *
 * The record is keyed by Id -- the placed terminal's stable name -- rather than by the actor,
 * because a checkpoint has to survive a level reload and an actor pointer does not. DisplayName is
 * copied in at first arrival so the menu can list a destination whose actor may not even be loaded
 * at the moment the list is drawn.
 */
USTRUCT(BlueprintType)
struct FL2H_CheckpointRecord
{
	GENERATED_BODY()

	/** Stable identity of the placed terminal that made this record. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FName Id;

	/** Copied from the terminal at first arrival, for the menu's travel list. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FText DisplayName;

	/** When the players first stood here. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FDateTime RecordedAtUtc;

	/** One entry per registered pawn, in registration order. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TArray<FL2H_SavedPlayer> Players;

	/**
	 * Every placed interactable that could name itself, as it stood when this record was written.
	 *
	 * The world is part of a checkpoint: a door the players opened is progress in exactly the sense a
	 * position is, and returning them to a place whose doors had shut again would put them somewhere the
	 * checkpoint never actually was.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TArray<FL2H_WorldStateRecord> WorldState;
};

/**
 * The on-disk shape of a TimeCore run.
 *
 * Checkpoints is an ordered stack, oldest first: every place the players have stood, in the order
 * they stood in them. It is a stack rather than a set because travel is deliberately backwards-only
 * -- a rewind to an earlier record truncates everything above it, since those records were forward
 * progress and forward progress is exactly what a rewind erases. The last entry is therefore always
 * the run's live checkpoint, which is what the game returns the players to on death.
 *
 * LevelName is recorded so a run saved in one level is refused in another rather than teleporting
 * players onto geometry that is not there.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Save Game"))
class LASTTWOHOURS_API UL2H_SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bumped when the layout below changes, so an old slot can be rejected instead of misread. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	int32 SaveVersion = 5;

	/** The level the checkpoint was taken in. A mismatch means it must not be applied. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FString LevelName;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	FDateTime SavedAtUtc;

	/** Every place the players have stood, oldest first. The last entry is the live checkpoint. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Save")
	TArray<FL2H_CheckpointRecord> Checkpoints;
};
