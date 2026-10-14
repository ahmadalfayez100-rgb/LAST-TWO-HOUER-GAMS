// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Save/L2H_SaveGame.h"

#include "L2H_SaveSubsystem.generated.h"

class AL2H_CheckpointTerminal;

/**
 * "Save for time": the run's checkpoint store.
 *
 * A run is a stack of checkpoints -- every place the players have stood, in the order they stood in
 * them -- and this subsystem owns that stack. Time is never written directly on the load path: every
 * restore works out the delta between the recorded value and the current one and routes it through
 * the director's cost or restore effect, so "time only ever changes through a GameplayEffect" holds on
 * a load just as it does in play.
 *
 * Two rules from the level design shape the whole API:
 *
 *   First arrival is what is kept. Walking into a checkpoint records the players' time once; standing
 *   in it again does not quietly overwrite that with a worse number, because the recorded time is
 *   what the game hands back on death. Only RecordCurrentTime, which a player has to ask for, may
 *   replace it.
 *
 *   Travel is backwards-only. TravelToCheckpoint restores an earlier record and then truncates the
 *   stack above it. Without the truncation the players could name an earlier record and then simply
 *   travel forward again, which is the freedom this design deliberately does not grant.
 *
 * A GameInstance subsystem rather than a widget, because the stack has to outlive the level.
 */
UCLASS(meta = (DisplayName = "L2H Save Subsystem"))
class LASTTWOHOURS_API UL2H_SaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Charges Payer the level's SaveCostSeconds and re-records the times at the checkpoint the payer
	 * is standing in. False when refused -- no terminal, or the payer cannot afford it.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	bool SaveForTime(AActor* Payer);

	/**
	 * Records a first arrival at the checkpoint the payer is standing in, without charging.
	 *
	 * The mandatory save: a hub records because the players walked in, not because they bought
	 * anything, so there is no affordability check to fail. An existing record is left alone -- first
	 * arrival stands.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Save")
	bool SaveCheckpointFree(AActor* Payer);

	/** Records a first arrival at CheckpointId. An existing record for that id is left untouched. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Save")
	bool RegisterCheckpoint(AActor* Payer, FName CheckpointId, const FText& DisplayName);

	/**
	 * Replaces the times already recorded at CheckpointId with the players' current ones.
	 *
	 * This is the player's choice -- "record where I am now" -- and the only thing that may overwrite
	 * a first arrival.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Save")
	bool RecordCurrentTime(AActor* Payer, FName CheckpointId);

	/** Every checkpoint the players have stood in, oldest first. The last one is the live one. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	TArray<FL2H_CheckpointRecord> GetCheckpoints(const UObject* WorldContextObject);

	/** The checkpoints strictly older than FromId -- every destination a rewind may legally name. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	TArray<FL2H_CheckpointRecord> GetTravelOptions(const UObject* WorldContextObject, FName FromId);

	/**
	 * Rewinds every registered player to Id and forgets every record after it. Id must be an earlier
	 * record than the one the players are standing in, which is what makes this a rewind rather than
	 * teleportation.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	bool TravelToCheckpoint(const UObject* WorldContextObject, FName Id);

	/** True when a checkpoint exists for this level and this build's save layout. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	bool HasCheckpoint(const UObject* WorldContextObject);

	/**
	 * Returns every registered pawn to the newest checkpoint and restores its recorded time. This is
	 * both the level-start resume and the death return.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	bool RestoreCheckpoint(const UObject* WorldContextObject);

	/** Deletes the slot on disk and forgets the cached copy. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Save")
	bool ClearCheckpoint(const UObject* WorldContextObject);

protected:
	/**
	 * Moves every registered pawn to the record and gives it the recorded time. Shared by the rewind,
	 * the level-start resume and the death return, so all three land the players identically.
	 */
	bool ApplyRecord(const UObject* WorldContextObject, const FL2H_CheckpointRecord& Record);

	/** Fills Record's player array from the director's registered pawns, as they are right now. */
	bool CapturePlayers(FL2H_CheckpointRecord& Record);

	/**
	 * Fills Record's world array from every placed interactable that can name itself.
	 *
	 * Returns how many were recorded. Zero is not a failure -- it is a level with nothing in it worth
	 * recording, and refusing a save over that would make an empty test level unsaveable.
	 */
	int32 CaptureWorldState(FL2H_CheckpointRecord& Record);

	/** Hands each recorded state back to the actor it names. Returns how many were found and applied. */
	int32 ApplyWorldState(const FL2H_CheckpointRecord& Record);

	/** The cached run, or a fresh empty one. Never null while the level has a time director. */
	UL2H_SaveGame* EnsureRun(const UObject* WorldContextObject);

	/** Writes the cached run to the slot. */
	bool PersistRun();

	/** The checkpoint the payer is standing in, or null. Null is what most refusals read as. */
	AL2H_CheckpointTerminal* ResolvePayerTerminal(AActor* Payer) const;

	/** The record for Id, or null. */
	FL2H_CheckpointRecord* FindRecord(UL2H_SaveGame* Run, FName Id) const;

	/** The checkpoint as last written or read. Transient: the slot on disk is the durable copy. */
	UPROPERTY(Transient)
	TObjectPtr<UL2H_SaveGame> CachedCheckpoint;

	/** Slot name from the level config, falling back to the built-in default. */
	static FString ResolveSlotName(const UObject* WorldContextObject);

	/**
	 * Loads the slot into CachedCheckpoint when it is not already loaded. A slot written by an older
	 * save layout is refused rather than misread.
	 */
	UL2H_SaveGame* ResolveCheckpoint(const UObject* WorldContextObject);
};
