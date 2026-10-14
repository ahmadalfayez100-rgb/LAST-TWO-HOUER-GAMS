// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/L2H_StorageActor.h"

#include "Components/StaticMeshComponent.h"
#include "Item/L2H_InventoryComponent.h"
#include "Time/L2HTimeLog.h"
#include "UI/L2H_InventoryWidget.h"

AL2H_StorageActor::AL2H_StorageActor()
{
	// Four times a drawer. A locker is where a run's finds wait while the player goes back out, so it is
	// authored for more than one trip rather than for what a single search turns up.
	Capacity = 24;

	DisplayName = NSLOCTEXT("L2H|Storage", "DefaultStorageName", "Storage");

	// Taller than the drawer it inherits from, so a locker reads as a standing object across a room and a
	// player can tell which box is the one they left things in.
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.9f));
	}
}

void AL2H_StorageActor::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	Super::L2H_GetInteractPrompt_Implementation(bCanInteract, OutVerb, OutTargetName);

	// The inherited verb is the generic "Search", which is wrong here: a locker is not something you
	// rummage through once, it is somewhere you put things back.
	if (bCanInteract)
	{
		OutVerb = NSLOCTEXT("L2H|Storage", "StorageVerb", "Open Storage");
	}
}

void AL2H_StorageActor::L2H_Interact_Implementation(AActor* Interactor)
{
	Super::L2H_Interact_Implementation(Interactor);

	if (IsLocked() || !Interactor)
	{
		return;
	}

	// Found by component and never by cast, so the same locker opens for whoever is carrying a grid. That
	// is what lets storage be verified on the test pawn before it ever meets the hero pawn.
	UL2H_InventoryComponent* Inventory = Interactor->FindComponentByClass<UL2H_InventoryComponent>();
	if (!Inventory)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Storage %s: %s carries no inventory"), *GetName(), *GetNameSafe(Interactor)));
		return;
	}

	Inventory->ToggleInventoryUI();

	// Bound after the toggle, and only if a grid actually exists: a toggle that just closed the grid
	// leaves nothing to bind, and a storage pointer on a widget the player cannot see is a transfer
	// target with no way to reach it.
	if (UL2H_InventoryWidget* Widget = Inventory->GetInventoryWidget())
	{
		Widget->SetStorageSource(this);
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Storage %s opened for %s (%d/%d full)"),
		*GetName(),
		*GetNameSafe(Interactor),
		GetItems().Num(),
		GetCapacity()));
}
