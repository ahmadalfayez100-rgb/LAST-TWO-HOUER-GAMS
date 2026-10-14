// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_CheckpointTerminal.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Save/L2H_SaveSubsystem.h"
#include "Time/L2H_TimeDirector.h"
#include "World/L2H_StationMarker.h"
#include "Time/L2HTimeLog.h"

AL2H_CheckpointTerminal::AL2H_CheckpointTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replicated so a hub spawned at runtime (the test stations) exists on clients too. Everything it
	// decides is still the server's: the director and the save check the net mode.
	bReplicates = true;
	bAlwaysRelevant = true;

	Range = CreateDefaultSubobject<UBoxComponent>(TEXT("Range"));
	SetRootComponent(Range);
	// InitBoxExtent, not SetBoxExtent: at CDO construction time the shape's physics and render state
	// does not exist yet, and the runtime setter would touch it anyway and crash the editor.
	Range->InitBoxExtent(FVector(160.0f, 160.0f, 120.0f));
	Range->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Range->SetCollisionObjectType(ECC_WorldDynamic);
	Range->SetCollisionResponseToAllChannels(ECR_Ignore);
	Range->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Range->SetGenerateOverlapEvents(true);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Range);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Marker = CreateDefaultSubobject<UL2H_StationMarker>(TEXT("Marker"));
	Marker->SetupAttachment(Range);
	Marker->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
}

void AL2H_CheckpointTerminal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const bool bHub = SavesOnArrival();
	Marker->Color = bHub ? FLinearColor(0.1f, 0.95f, 0.3f) : FLinearColor(0.15f, 0.45f, 1.0f);
	Marker->Label = bHub
		? NSLOCTEXT("L2HStation", "Hub", "HUB: clock stops, both players in = stage ends, shop")
		: NSLOCTEXT("L2HStation", "Spot", "SAVE SPOT: paid save (time menu)");
	Marker->PadRadius = 160.0f;
	Marker->Refresh();
}

void AL2H_CheckpointTerminal::BeginPlay()
{
	Super::BeginPlay();

	// Bound in BeginPlay rather than the constructor so the delegate is never registered twice by a
	// Blueprint child that also touches the box.
	Range->OnComponentBeginOverlap.AddDynamic(this, &AL2H_CheckpointTerminal::HandleBeginOverlap);
	Range->OnComponentEndOverlap.AddDynamic(this, &AL2H_CheckpointTerminal::HandleEndOverlap);
}

bool AL2H_CheckpointTerminal::IsPawnInRange(const AL2H_BaseCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Pawn : PawnsInRange)
	{
		if (Pawn.Get() == Character)
		{
			return true;
		}
	}

	return false;
}

void AL2H_CheckpointTerminal::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	PawnsInRange.AddUnique(Character);

	// The pawn remembers the terminal instead of the terminal being searched for on a key press, so
	// the menu key is a pointer read and never a world query.
	Character->SetCurrentTerminal(this);
	OnPawnEntered(Character);

	// A hub is a rest: the clock stops inside, and the stage ends (bank, new clock, save) once every
	// player is in. The terminal is a non-replicated level actor, so it counts as authority on a client
	// too; the net mode is what says whether this machine owns the run.
	if (SavesOnArrival() && GetNetMode() != NM_Client)
	{
		if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
		{
			Director->NotifyHubEntered(Character, this);
		}
	}
}

void AL2H_CheckpointTerminal::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	PawnsInRange.Remove(Character);

	// Only the terminal the pawn is actually standing in may clear the pointer: with overlapping
	// ranges, the older exit must not wipe a newer entry.
	if (Character->GetCurrentTerminal() == this)
	{
		Character->SetCurrentTerminal(nullptr);
	}

	if (SavesOnArrival() && GetNetMode() != NM_Client)
	{
		if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
		{
			Director->NotifyHubExited(Character, this);
		}
	}

	OnPawnExited(Character);
}
