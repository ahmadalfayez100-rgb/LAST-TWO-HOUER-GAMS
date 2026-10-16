// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/L2H_ItemPickup.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/L2H_WeaponComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Item/L2H_InventoryComponent.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"

#include "Interaction/L2H_WorldStateId.h"

AL2H_ItemPickup::AL2H_ItemPickup()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(SceneRoot);
	// Query-only on the Interact channel: a dropped item must be pressable but must not become an
	// obstacle the player walks into, and it must not block a bullet trace either.
	ItemMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ItemMesh->SetSimulatePhysics(false);
}

void AL2H_ItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_ItemPickup, Instance);
	DOREPLIFETIME(AL2H_ItemPickup, bTaken);
}

void AL2H_ItemPickup::InitialiseFromInstance(const FL2H_ItemInstance& InInstance)
{
	if (!HasAuthority())
	{
		return;
	}

	Instance = InInstance;
	OnRep_Instance();
}

void AL2H_ItemPickup::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	// The instance id IS the world identity here, which is what lets the save layer remember that THIS
	// dropped crowbar was already taken without remembering anything about the actor that held it.
	OutId = Instance.InstanceId.IsValid() ? Instance.InstanceId : L2H_MakeWorldStateId(this);
}

void AL2H_ItemPickup::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	bCanInteract = Instance.IsSet() && !bTaken;
	OutVerb = NSLOCTEXT("L2H|Item", "PickUp", "Pick up");

	OutTargetName = Instance.IsSet()
		? Instance.Data->Data.DisplayName
		: NSLOCTEXT("L2H|Item", "UnknownItem", "Unknown item");
}

void AL2H_ItemPickup::L2H_Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !Instance.IsSet() || !Interactor || bTaken)
	{
		return;
	}

	// Asked for by class, not cast to a pawn: the pickup does not care what picked it up, only whether
	// that thing has somewhere to put it.
	UL2H_InventoryComponent* Inventory = Interactor->FindComponentByClass<UL2H_InventoryComponent>();

	if (!Inventory)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Pickup refused: %s has no L2H inventory component"),
			*GetNameSafe(Interactor)));
		return;
	}

	if (!Inventory->CanAccept(Instance.Data, Instance.Quantity))
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Pickup refused: %s inventory has no room for %s"),
			*GetNameSafe(Interactor),
			*GetNameSafe(Instance.Data)));
		return;
	}

	if (Inventory->ServerAddItemInstance(Instance))
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Pickup taken: %s by %s (instance %s)"),
			*GetNameSafe(Instance.Data),
			*GetNameSafe(Interactor),
			*Instance.InstanceId.ToString()));

		// The hands go to where the item lay: from the floor, a table, or a shelf.
		if (AL2H_BaseCharacter* Taker = Cast<AL2H_BaseCharacter>(Interactor))
		{
			if (GrantsWeapon)
			{
				if (UL2H_WeaponComponent* Weapons = Taker->FindComponentByClass<UL2H_WeaponComponent>())
				{
					Weapons->ServerAddWeapon(GrantsWeapon);
				}
			}
			const float FeetZ = Taker->GetActorLocation().Z - Taker->GetSimpleCollisionHalfHeight();
			Taker->PlayInteractAnim(Taker->GetPickupAnim(GetActorLocation().Z - FeetZ));
		}

		// A placed pickup is only hidden: a rewind to an earlier checkpoint puts it back where it was. One
		// dropped at runtime has no place in any checkpoint's world, so it simply goes.
		if (IsNetStartupActor())
		{
			SetTaken(true);
		}
		else
		{
			Destroy();
		}
	}
}

void AL2H_ItemPickup::OnRep_Instance()
{
	if (ItemMesh)
	{
		ItemMesh->SetStaticMesh(Instance.IsSet() ? Instance.Data->Data.WorldMesh : nullptr);
	}

	OnPickupRefreshed();
}

void AL2H_ItemPickup::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	OutFlags.Add(TEXT("Taken"), bTaken);
}

void AL2H_ItemPickup::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	if (const bool* Taken = InFlags.Find(TEXT("Taken")))
	{
		SetTaken(*Taken);
	}
}

void AL2H_ItemPickup::SetTaken(bool bNewTaken)
{
	if (!HasAuthority())
	{
		return;
	}

	bTaken = bNewTaken;
	OnRep_Taken();
}

void AL2H_ItemPickup::OnRep_Taken()
{
	SetActorHiddenInGame(bTaken);
	if (ItemMesh)
	{
		ItemMesh->SetCollisionEnabled(bTaken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
}

void AL2H_ItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// A pickup placed in the level carries its item from the Details panel, so the mesh is shown as soon
	// as the item is chosen, and the same on every machine at load (the instance is part of the level).
	if (ItemMesh)
	{
		ItemMesh->SetStaticMesh(Instance.IsSet() ? Instance.Data->Data.WorldMesh : nullptr);
	}
}
