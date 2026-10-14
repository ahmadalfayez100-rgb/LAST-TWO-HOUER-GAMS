// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/L2H_ContainerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "UObject/ConstructorHelpers.h"

#include "Interaction/L2H_WorldStateId.h"

AL2H_ContainerActor::AL2H_ContainerActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.5f, 0.6f, 0.25f));
	}
}

void AL2H_ContainerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_ContainerActor, Items);
	DOREPLIFETIME(AL2H_ContainerActor, bLocked);
}

void AL2H_ContainerActor::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	OutFlags.Add(FName(TEXT("Locked")), bLocked);
}

void AL2H_ContainerActor::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	// Only the lock travels. The contents do not: this record carries name/flag pairs, and a container is
	// the one interactable whose state is not a set of flags -- restoring what was taken out of a drawer
	// needs an item payload on the record, which is a separate change to the save format.
	if (const bool* Found = InFlags.Find(FName(TEXT("Locked"))))
	{
		SetLocked(*Found);
	}
}

void AL2H_ContainerActor::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	OutId = StateId.IsValid() ? StateId : L2H_MakeWorldStateId(this);
}

void AL2H_ContainerActor::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	OutTargetName = DisplayName;
	bCanInteract = !bLocked;

	OutVerb = bLocked
		? NSLOCTEXT("L2H|Container", "Locked", "Locked")
		: NSLOCTEXT("L2H|Container", "Search", "Search");
}

void AL2H_ContainerActor::L2H_Interact_Implementation(AActor* Interactor)
{
	if (bLocked)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Container refused: %s is locked (%s tried)"), *GetName(), *GetNameSafe(Interactor)));
		return;
	}

	// Opening the drawer is a UI decision made by the HUD of the player who searched it, so this does not
	// move any items -- it only reports that the drawer was opened.
	L2HTime::LogImportant(FString::Printf(
		TEXT("Container searched: %s by %s (%d/%d full)"),
		*GetName(),
		*GetNameSafe(Interactor),
		Items.Num(),
		Capacity));

	OnContainerChanged();
}

bool AL2H_ContainerActor::AddItem(const FL2H_ItemInstance& Item)
{
	if (!HasAuthority() || !Item.IsSet())
	{
		return false;
	}

	if (Items.Num() >= Capacity)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Container full: %s refused an item (capacity %d)"), *GetName(), Capacity));
		return false;
	}

	Items.Add(Item);

	L2HTime::LogImportant(FString::Printf(TEXT("Container %s accepted %s (%d/%d)"), *GetName(), *GetNameSafe(Item.Data), Items.Num(), Capacity));

	OnContainerChanged();
	return true;
}

bool AL2H_ContainerActor::TakeItemAt(int32 Index, FL2H_ItemInstance& OutItem)
{
	if (!HasAuthority() || !Items.IsValidIndex(Index))
	{
		return false;
	}

	OutItem = Items[Index];
	Items.RemoveAt(Index);

	L2HTime::LogImportant(FString::Printf(TEXT("Container %s handed out %s (%d/%d)"), *GetName(), *GetNameSafe(OutItem.Data), Items.Num(), Capacity));

	OnContainerChanged();
	return true;
}

void AL2H_ContainerActor::SetLocked(bool bNewLocked)
{
	if (!HasAuthority() || bLocked == bNewLocked)
	{
		return;
	}

	bLocked = bNewLocked;
	L2HTime::LogImportant(FString::Printf(TEXT("Container %s: %s"), *GetName(), bLocked ? TEXT("locked") : TEXT("unlocked")));
}

void AL2H_ContainerActor::OnRep_Items()
{
	L2HTime::LogVerbose(FString::Printf(TEXT("Container %s contents replicated: %d items"), *GetName(), Items.Num()));
	OnContainerChanged();
}
