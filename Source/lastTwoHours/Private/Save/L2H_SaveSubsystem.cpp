// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/L2H_SaveSubsystem.h"

#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/L2H_InteractableInterface.h"
#include "Item/L2H_InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"
#include "World/L2H_CheckpointTerminal.h"

namespace
{
	/** Used when the level's config leaves CheckpointSlotName empty. */
	const TCHAR* const DefaultCheckpointSlot = TEXT("L2H_Time_Checkpoint");

	/**
	 * The layout this build writes. A slot from another layout is refused rather than misread, which
	 * matters here because version 1 stored one flat checkpoint, version 2 stores a stack, and version 3
	 * records the world alongside the players: reading one as the other would hand the players a
	 * checkpoint they never made, or restore them into a world that does not match it.
	 */
	const int32 CurrentSaveVersion = 5;
}

FString UL2H_SaveSubsystem::ResolveSlotName(const UObject* WorldContextObject)
{
	FString Base(DefaultCheckpointSlot);
	if (const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(WorldContextObject))
	{
		if (!Config->CheckpointSlotName.IsEmpty())
		{
			Base = Config->CheckpointSlotName;
		}
	}

	// One slot per level. Levels share the config asset (and so its slot name), and a shared slot meant
	// the first level to save locked every other level out of saving at all.
	const FString LevelName = WorldContextObject ? UGameplayStatics::GetCurrentLevelName(WorldContextObject, /*bRemovePrefixString=*/true) : FString();
	return LevelName.IsEmpty() ? Base : FString::Printf(TEXT("%s_%s"), *Base, *LevelName);
}

UL2H_SaveGame* UL2H_SaveSubsystem::ResolveCheckpoint(const UObject* WorldContextObject)
{
	if (CachedCheckpoint)
	{
		return CachedCheckpoint;
	}

	const FString SlotName = ResolveSlotName(WorldContextObject);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return nullptr;
	}

	UL2H_SaveGame* Loaded = Cast<UL2H_SaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Loaded)
	{
		return nullptr;
	}

	if (Loaded->SaveVersion != CurrentSaveVersion)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: slot %s is layout v%d and this build writes v%d -- ignored"),
			*SlotName, Loaded->SaveVersion, CurrentSaveVersion));
		return nullptr;
	}

	CachedCheckpoint = Loaded;
	return CachedCheckpoint;
}

UL2H_SaveGame* UL2H_SaveSubsystem::EnsureRun(const UObject* WorldContextObject)
{
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);

	if (UL2H_SaveGame* Existing = ResolveCheckpoint(WorldContextObject))
	{
		if (Existing->LevelName.IsEmpty() || Existing->LevelName == LevelName)
		{
			return Existing;
		}

		// Refused rather than reused: writing this level's first arrival into another level's run
		// would move that level's checkpoints without anybody ever visiting it. A level's slot name
		// is meant to be its own -- the config is per level -- so reaching here means two levels are
		// sharing a slot, and the safe answer is to write nothing at all.
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: the slot holds a run for %s, not %s -- refused"),
			*Existing->LevelName, *LevelName));
		return nullptr;
	}

	UL2H_SaveGame* Run = Cast<UL2H_SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UL2H_SaveGame::StaticClass()));
	if (!Run)
	{
		L2HTime::LogImportant(TEXT("Save: could not create the run object"));
		return nullptr;
	}

	Run->LevelName = LevelName;
	Run->SavedAtUtc = FDateTime::UtcNow();
	CachedCheckpoint = Run;

	return Run;
}

bool UL2H_SaveSubsystem::PersistRun()
{
	if (!CachedCheckpoint)
	{
		return false;
	}

	CachedCheckpoint->SavedAtUtc = FDateTime::UtcNow();

	const FString SlotName = ResolveSlotName(this);
	if (!UGameplayStatics::SaveGameToSlot(CachedCheckpoint, SlotName, 0))
	{
		L2HTime::LogImportant(TEXT("Save: the slot could not be written"));
		return false;
	}

	return true;
}

FL2H_CheckpointRecord* UL2H_SaveSubsystem::FindRecord(UL2H_SaveGame* Run, FName Id) const
{
	if (!Run || Id.IsNone())
	{
		return nullptr;
	}

	return Run->Checkpoints.FindByPredicate(
		[Id](const FL2H_CheckpointRecord& Record) { return Record.Id == Id; });
}

AL2H_CheckpointTerminal* UL2H_SaveSubsystem::ResolvePayerTerminal(AActor* Payer) const
{
	const AL2H_BaseCharacter* Character = Cast<const AL2H_BaseCharacter>(Payer);
	return Character ? Character->GetCurrentTerminal() : nullptr;
}

bool UL2H_SaveSubsystem::CapturePlayers(FL2H_CheckpointRecord& Record)
{
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		return false;
	}

	Record.Players.Reset();

	for (AL2H_BaseCharacter* Pawn : Director->GetRegisteredPawns())
	{
		if (!Pawn)
		{
			continue;
		}

		FL2H_SavedPlayer Saved;
		Saved.Transform = Pawn->GetActorTransform();
		Saved.Time = AL2H_TimeDirector::GetTimeFor(Pawn);
		Saved.bDowned = Pawn->IsDowned();
		Saved.Chrons = Pawn->GetChrons();
		Saved.Health = AL2H_TimeDirector::GetHealthFor(Pawn);
		if (const UL2H_InventoryComponent* Bag = Pawn->FindComponentByClass<UL2H_InventoryComponent>())
		{
			Saved.InventorySlots = Bag->GetSlots();
			Saved.InventoryShortcuts = Bag->GetShortcuts();
		}
		Record.Players.Add(Saved);
	}

	return Record.Players.Num() > 0;
}

int32 UL2H_SaveSubsystem::CaptureWorldState(FL2H_CheckpointRecord& Record)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	Record.WorldState.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UL2H_InteractableInterface>())
		{
			continue;
		}

		FGuid Id;
		IL2H_InteractableInterface::Execute_L2H_GetWorldStateId(Actor, Id);

		// An actor that cannot name itself cannot be found again on a load. Left out rather than recorded
		// under a guess: a door that reopens is a far smaller failure than a record that lands on the
		// wrong door.
		if (!Id.IsValid())
		{
			continue;
		}

		FL2H_WorldStateRecord State;
		State.Id = Id;
		State.ActorPath = Actor->GetPathName();

		// The actor reports its own state. This layer never inspects an actor's fields, which is what keeps
		// a new interactable type from needing anything changed here.
		IL2H_InteractableInterface::Execute_L2H_CaptureWorldState(Actor, State.Flags);

		Record.WorldState.Add(State);
	}

	return Record.WorldState.Num();
}

int32 UL2H_SaveSubsystem::ApplyWorldState(const FL2H_CheckpointRecord& Record)
{
	UWorld* World = GetWorld();
	if (!World || Record.WorldState.Num() == 0)
	{
		return 0;
	}

	// Indexed in a pass of its own rather than looked up as each record is read: one pass over the level
	// beats a scan per record, and it is what makes falling back from the id to the actor path possible.
	TMap<FGuid, AActor*> ById;
	TMap<FString, AActor*> ByPath;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UL2H_InteractableInterface>())
		{
			continue;
		}

		FGuid Id;
		IL2H_InteractableInterface::Execute_L2H_GetWorldStateId(Actor, Id);
		if (Id.IsValid())
		{
			ById.Add(Id, Actor);
		}

		ByPath.Add(Actor->GetPathName(), Actor);
	}

	int32 Applied = 0;

	for (const FL2H_WorldStateRecord& State : Record.WorldState)
	{
		// Id first, because it is the identity that survives a level reload.
		AActor** Found = ById.Find(State.Id);
		if (!Found && !State.ActorPath.IsEmpty())
		{
			Found = ByPath.Find(State.ActorPath);
		}

		if (!Found || !*Found)
		{
			// The actor this record described is not in this level. Quiet, because a checkpoint outlives
			// the level it was taken in and a missing actor is a legitimate outcome rather than a fault.
			continue;
		}

		IL2H_InteractableInterface::Execute_L2H_ApplyWorldState(*Found, State.Flags);
		++Applied;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: world state applied to %d of %d recorded actor(s)"),
		Applied,
		Record.WorldState.Num()));

	return Applied;
}

bool UL2H_SaveSubsystem::RegisterCheckpoint(AActor* Payer, FName CheckpointId, const FText& DisplayName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- checkpoints are recorded by the server"));
		return false;
	}

	if (CheckpointId.IsNone())
	{
		L2HTime::LogImportant(TEXT("Save: refused -- the terminal has no checkpoint id"));
		return false;
	}

	UL2H_SaveGame* Run = EnsureRun(this);
	if (!Run)
	{
		return false;
	}

	if (FindRecord(Run, CheckpointId))
	{
		// First arrival stands. Re-walking into a checkpoint the players already visited must not
		// lower the time the game hands back to them on death, so this is a deliberate no-op rather
		// than an overwrite. RecordCurrentTime is the only path that may replace the number, and it
		// exists precisely because a player has to ask for it.
		return true;
	}

	FL2H_CheckpointRecord Record;
	Record.Id = CheckpointId;
	Record.DisplayName = DisplayName;
	Record.RecordedAtUtc = FDateTime::UtcNow();

	if (!CapturePlayers(Record))
	{
		L2HTime::LogImportant(TEXT("Save: refused -- no registered players to record"));
		return false;
	}

	// The world is part of the checkpoint too: a door the players opened is progress, and a load that
	// forgot it would put them back in a level that disagrees with the position they were returned to.
	CaptureWorldState(Record);

	Run->Checkpoints.Add(Record);

	if (!PersistRun())
	{
		return false;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: first arrival at %s -- recorded %d player(s), %d checkpoint(s) in the run"),
		*CheckpointId.ToString(), Record.Players.Num(), Run->Checkpoints.Num()));

	return true;
}

bool UL2H_SaveSubsystem::RecordCurrentTime(AActor* Payer, FName CheckpointId)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	UL2H_SaveGame* Run = ResolveCheckpoint(this);
	FL2H_CheckpointRecord* Record = FindRecord(Run, CheckpointId);
	if (!Record)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: refused -- %s has not been visited, so there is nothing to re-record"),
			*CheckpointId.ToString()));
		return false;
	}

	if (!CapturePlayers(*Record))
	{
		L2HTime::LogImportant(TEXT("Save: refused -- no registered players to record"));
		return false;
	}

	CaptureWorldState(*Record);

	Record->RecordedAtUtc = FDateTime::UtcNow();

	if (!PersistRun())
	{
		return false;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: %s re-recorded the times at %s"), *GetNameSafe(Payer), *CheckpointId.ToString()));

	return true;
}

bool UL2H_SaveSubsystem::SaveForTime(AActor* Payer)
{
	UWorld* World = GetWorld();
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);

	if (!World || !Director || !Config)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- the level has no time director"));
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- saves are written by the server"));
		return false;
	}

	// Resolved before the charge, not after. Charging for a re-record that then cannot happen would
	// take a player's seconds and hand back nothing for them.
	const AL2H_CheckpointTerminal* Terminal = ResolvePayerTerminal(Payer);
	if (!Terminal)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: refused -- %s is not standing in a checkpoint"), *GetNameSafe(Payer)));
		return false;
	}

	const float Cost = FMath::Max(0.0f, Config->SaveCostSeconds);
	const float Available = AL2H_TimeDirector::GetTimeFor(Payer);

	if (Available < 0.0f)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: refused -- %s has no time system"), *GetNameSafe(Payer)));
		return false;
	}

	if (Available < Cost)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: refused -- %s has %.1fs and the save costs %.1fs"),
			*GetNameSafe(Payer), Available, Cost));
		return false;
	}

	Director->ApplyTimeCost(Payer, Cost);

	// A place the players have never stood in has no record yet, so there is nothing to re-record:
	// the first paid save anywhere is that place's registration, and every one after it is a
	// re-record. Without this a spot -- which never records on arrival -- would take the seconds and
	// then refuse, which is the exact trade this function resolves its terminal early to avoid.
	const FName TerminalId = Terminal->GetCheckpointId();
	if (FindRecord(ResolveCheckpoint(this), TerminalId))
	{
		return RecordCurrentTime(Payer, TerminalId);
	}

	return RegisterCheckpoint(Payer, TerminalId, Terminal->DisplayName);
}

bool UL2H_SaveSubsystem::SaveCheckpointFree(AActor* Payer)
{
	UWorld* World = GetWorld();
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);

	if (!World || !Director)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- the level has no time director"));
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- saves are written by the server"));
		return false;
	}

	// Deliberately no affordability check and no charge. This is the mandatory save: a hub records
	// one because the players arrived, and a promise the game made cannot be refused for lack of
	// funds -- that would turn a guarantee into a second, hidden cost.
	//
	// The identity comes from the terminal the payer is standing in, which the terminal set before it
	// called in, so this never has to be told which checkpoint it is recording.
	const AL2H_CheckpointTerminal* Terminal = ResolvePayerTerminal(Payer);
	if (!Terminal)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: refused -- %s is not standing in a checkpoint"), *GetNameSafe(Payer)));
		return false;
	}

	return RegisterCheckpoint(Payer, Terminal->GetCheckpointId(), Terminal->DisplayName);
}

TArray<FL2H_CheckpointRecord> UL2H_SaveSubsystem::GetCheckpoints(const UObject* WorldContextObject)
{
	if (const UL2H_SaveGame* Run = ResolveCheckpoint(WorldContextObject))
	{
		return Run->Checkpoints;
	}

	return TArray<FL2H_CheckpointRecord>();
}

TArray<FL2H_CheckpointRecord> UL2H_SaveSubsystem::GetTravelOptions(
	const UObject* WorldContextObject, FName FromId)
{
	TArray<FL2H_CheckpointRecord> Options;

	UL2H_SaveGame* Run = ResolveCheckpoint(WorldContextObject);
	if (!Run)
	{
		return Options;
	}

	// Everything from the checkpoint the players are standing in onward is progress they would be
	// rewinding through, so it is not a destination. A FromId nobody has visited offers the whole
	// run, which is the honest answer: there is no position to be backwards of.
	const FL2H_CheckpointRecord* From = FindRecord(Run, FromId);
	const int32 FromIndex = From
		? Run->Checkpoints.IndexOfByPredicate(
			[FromId](const FL2H_CheckpointRecord& Record) { return Record.Id == FromId; })
		: Run->Checkpoints.Num();

	if (FromIndex <= 0)
	{
		return Options;
	}

	Options.Reserve(FromIndex);
	for (int32 Index = 0; Index < FromIndex; ++Index)
	{
		Options.Add(Run->Checkpoints[Index]);
	}

	return Options;
}

bool UL2H_SaveSubsystem::TravelToCheckpoint(const UObject* WorldContextObject, FName Id)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	UL2H_SaveGame* Run = ResolveCheckpoint(WorldContextObject);
	if (!Run)
	{
		L2HTime::LogImportant(TEXT("Save: travel refused -- this level has no run"));
		return false;
	}

	const int32 Index = Run->Checkpoints.IndexOfByPredicate(
		[Id](const FL2H_CheckpointRecord& Record) { return Record.Id == Id; });

	if (Index == INDEX_NONE)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: travel refused -- %s was never visited"), *Id.ToString()));
		return false;
	}

	// Copied out before the restore: the restore runs gameplay code, and the array it would
	// otherwise be reading from gets truncated further down.
	const FL2H_CheckpointRecord Target = Run->Checkpoints[Index];

	if (!ApplyRecord(WorldContextObject, Target))
	{
		// A refused rewind has to be a complete no-op. Forgetting the later checkpoints and then
		// failing to move anybody would cost the players their progress and give them nothing.
		return false;
	}

	// Backwards-only is enforced here. The records above the destination were forward progress, and a
	// rewind is exactly the erasure of forward progress -- without this the players could name an
	// earlier checkpoint and then simply travel forward again.
	const int32 Forgotten = Run->Checkpoints.Num() - (Index + 1);
	if (Forgotten > 0)
	{
		Run->Checkpoints.RemoveAt(Index + 1, Forgotten);
	}

	PersistRun();

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: travelled back to %s -- %d later checkpoint(s) forgotten, %d left in the run"),
		*Id.ToString(), Forgotten, Run->Checkpoints.Num()));

	return true;
}

bool UL2H_SaveSubsystem::ApplyRecord(const UObject* WorldContextObject, const FL2H_CheckpointRecord& Record)
{
	UWorld* World = GetWorld();
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(WorldContextObject);

	if (!World || !Director)
	{
		L2HTime::LogImportant(TEXT("Save: restore refused -- no time director"));
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return false;
	}

	const TArray<AL2H_BaseCharacter*> Pawns = Director->GetRegisteredPawns();

	// Refused rather than padded: matching players to slots by index is only safe when the counts
	// agree, and a mismatched restore would silently hand one player another player's time.
	if (Pawns.Num() != Record.Players.Num())
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: restore refused -- %d player(s) now, %d in the checkpoint"),
			Pawns.Num(), Record.Players.Num()));
		return false;
	}

	// Every carry ends before anyone is touched. Whatever was in hand stays where it was, and a partner on
	// someone's shoulders has to be off them before the restore stands that partner back up.
	for (AL2H_BaseCharacter* Pawn : Pawns)
	{
		if (Pawn)
		{
			Pawn->EndCarry();
		}
	}

	for (int32 Index = 0; Index < Pawns.Num(); ++Index)
	{
		AL2H_BaseCharacter* Pawn = Pawns[Index];
		const FL2H_SavedPlayer& Saved = Record.Players[Index];
		if (!Pawn)
		{
			continue;
		}

		// Death is undone before the clock moves: the director ignores time changes on a dead pawn, so
		// a restore that left the flag up would hand back a run with a body that never gets up again.
		if (Pawn->IsDead())
		{
			Pawn->SetDead(false);
		}
		Director->ResetHealth(Pawn);
		if (Saved.Health > 0.0f)
		{
			Director->SetHealthTo(Pawn, Saved.Health);
			Pawn->NotifyHealthDamaged();
		}
		if (UL2H_InventoryComponent* Bag = Pawn->FindComponentByClass<UL2H_InventoryComponent>())
		{
			Bag->RestoreContents(Saved.InventorySlots, Saved.InventoryShortcuts);
		}

		Pawn->TeleportTo(Saved.Transform.GetLocation(), Saved.Transform.Rotator(), false, true);

		// Through the director, never a raw attribute write: "time only ever changes through a
		// GameplayEffect" is the rule the whole system is built on, and a load is not an exception.
		// This is also what lifts a downed pawn back up, because the restore crosses zero.
		Director->ApplyTimeTo(Pawn, Saved.Time);
		Pawn->SetChrons(Saved.Chrons);
		Pawn->CloseHubBank();

		if (Saved.bDowned != Pawn->IsDowned())
		{
			Pawn->SetDowned(Saved.bDowned);
		}
	}

	// The world is restored after the players, never before: an interactable that reacts to a player
	// arriving should see the player already in place rather than a level caught mid-move.
	const int32 RestoredActors = ApplyWorldState(Record);

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: restored %s for %d player(s), %d world actor(s)"),
		*Record.Id.ToString(),
		Pawns.Num(),
		RestoredActors));

	return true;
}

bool UL2H_SaveSubsystem::HasCheckpoint(const UObject* WorldContextObject)
{
	const UL2H_SaveGame* Run = ResolveCheckpoint(WorldContextObject);
	if (!Run || Run->Checkpoints.Num() == 0)
	{
		return false;
	}

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(WorldContextObject, /*bRemovePrefixString=*/true);
	if (!Run->LevelName.IsEmpty() && Run->LevelName != LevelName)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Save: run belongs to %s, not %s -- ignored"),
			*Run->LevelName, *LevelName));
		return false;
	}

	return true;
}

bool UL2H_SaveSubsystem::RestoreCheckpoint(const UObject* WorldContextObject)
{
	UL2H_SaveGame* Run = ResolveCheckpoint(WorldContextObject);
	if (!Run || Run->Checkpoints.Num() == 0)
	{
		return false;
	}

	// The newest record, not a chosen one: this is the level-start resume and the death return, and
	// in both the players did not choose where they were going back to.
	return ApplyRecord(WorldContextObject, Run->Checkpoints.Last());
}

bool UL2H_SaveSubsystem::ClearCheckpoint(const UObject* WorldContextObject)
{
	const FString SlotName = ResolveSlotName(WorldContextObject);
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	CachedCheckpoint = nullptr;

	L2HTime::LogImportant(FString::Printf(
		TEXT("Save: checkpoint %s"), bDeleted ? TEXT("cleared") : TEXT("not found")));

	return bDeleted;
}
