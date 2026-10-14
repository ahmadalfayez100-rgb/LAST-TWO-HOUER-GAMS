// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/L2H_InventoryComponent.h"

#include "Character/L2H_BaseCharacter.h"
#include "Time/L2H_TimeDirector.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Item/L2H_ItemPickup.h"
#include "Item/L2H_StorageActor.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "UI/L2H_InventoryWidget.h"

namespace
{
	/** An empty cell is one with no data asset -- there is no separate empty-slot type to keep in step. */
	FORCEINLINE bool IsEmptySlot(const FL2H_ItemInstance& Item)
	{
		return Item.Data == nullptr;
	}
}

UL2H_InventoryComponent::UL2H_InventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// The grid is replicated state, so the component opts in rather than relying on a Blueprint checkbox
	// -- a pawn that forgot it would show a full inventory on the host and an empty one on a client.
	SetIsReplicatedByDefault(true);
}

void UL2H_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// The grid is sized once, on every machine, from the authored capacity. A replicated array cannot
	// carry its own length meaningfully, because "12 cells with 3 items" and "3 cells" are the same
	// three entries on the wire.
	Slots.SetNum(FMath::Max(Capacity, 1));
	Shortcuts.SetNum(FMath::Max(ShortcutCount, 1));
}

void UL2H_InventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseInventoryUI();
	Super::EndPlay(EndPlayReason);
}

void UL2H_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UL2H_InventoryComponent, Slots);
	DOREPLIFETIME(UL2H_InventoryComponent, Shortcuts);
}

void UL2H_InventoryComponent::OnRep_Slots()
{
	L2HTime::LogVerbose(FString::Printf(TEXT("%s inventory replicated (%d cells)"), *GetNameSafe(GetOwner()), Slots.Num()));
	BroadcastChanged();
}

void UL2H_InventoryComponent::OnRep_Shortcuts()
{
	BroadcastChanged();
}

void UL2H_InventoryComponent::RestoreContents(const TArray<FL2H_ItemInstance>& InSlots, const TArray<FL2H_ItemInstance>& InShortcuts)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// The grid keeps its configured size whatever the record holds: a checkpoint written before the
	// bag grew (or shrank) still loads, with the extra cells empty or the overflow dropped.
	Slots = InSlots;
	Slots.SetNum(FMath::Max(Capacity, 1));
	Shortcuts = InShortcuts;
	Shortcuts.SetNum(FMath::Max(ShortcutCount, 1));
	BroadcastChanged();
}

void UL2H_InventoryComponent::BroadcastChanged()
{
	OnInventoryChanged.Broadcast();
}

FL2H_ItemInstance UL2H_InventoryComponent::GetSlotItem(int32 Index) const
{
	return Slots.IsValidIndex(Index) ? Slots[Index] : FL2H_ItemInstance();
}

FL2H_ItemInstance UL2H_InventoryComponent::GetShortcutItem(int32 Index) const
{
	return Shortcuts.IsValidIndex(Index) ? Shortcuts[Index] : FL2H_ItemInstance();
}

bool UL2H_InventoryComponent::HasItemWithTag(FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}

	for (const FL2H_ItemInstance& Item : Slots)
	{
		if (Item.IsSet() && Item.KeyTag.MatchesTag(Tag))
		{
			return true;
		}
	}

	return false;
}

int32 UL2H_InventoryComponent::CountItemsOfType(EL2H_PickupType Type) const
{
	int32 Total = 0;

	for (const FL2H_ItemInstance& Item : Slots)
	{
		if (Item.IsSet() && Item.Data->Data.Type == Type)
		{
			Total += Item.Quantity;
		}
	}

	return Total;
}

int32 UL2H_InventoryComponent::FindToolCell(EL2H_ItemUseEffect Effect) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].IsSet() && Slots[Index].Data->Data.UseEffect == Effect)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UL2H_InventoryComponent::FindStackSlot(const UL2H_ItemDataAsset* Data) const
{
	if (Data == nullptr || !Data->Data.bStackable)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		// The second half of a two-cell item is skipped: it is a placeholder for its first half and
		// growing its quantity would double-count the stack.
		if (Slots[Index].IsSet()
			&& !Slots[Index].bSecondaryHalf
			&& Slots[Index].Data == Data
			&& Slots[Index].Quantity < MaxStackSize)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 UL2H_InventoryComponent::FindFreeSlot(const UL2H_ItemDataAsset* Data) const
{
	const bool bNeedsSecondCell = Data != nullptr && Data->Data.bOccupiesTwoSlots;

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!IsEmptySlot(Slots[Index]))
		{
			continue;
		}

		if (!bNeedsSecondCell)
		{
			return Index;
		}

		const int32 Row = Index / FMath::Max(SlotsPerRow, 1);
		const int32 Column = Index % FMath::Max(SlotsPerRow, 1);

		// A two-cell item may not straddle a row break, so the last column of every row is not a valid
		// anchor. Without this the second half would appear at the start of the next row, which looks
		// like two unrelated items.
		if (Column >= (SlotsPerRow - 1))
		{
			continue;
		}

		if (Slots.IsValidIndex(Index + 1) && IsEmptySlot(Slots[Index + 1]))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UL2H_InventoryComponent::CanAccept(const UL2H_ItemDataAsset* Data, int32 Quantity) const
{
	if (Data == nullptr || Quantity <= 0)
	{
		return false;
	}

	// A stack that can still grow counts as room, otherwise a single ammo box would fill a grid the
	// player could no longer add another shell to.
	if (FindStackSlot(Data) != INDEX_NONE)
	{
		return true;
	}

	return FindFreeSlot(Data) != INDEX_NONE;
}

bool UL2H_InventoryComponent::ServerAddItemInstance(const FL2H_ItemInstance& InInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InInstance.IsSet())
	{
		return false;
	}

	UL2H_ItemDataAsset* Data = InInstance.Data;

	// Stack first: a second copy of something stackable belongs in the stack, not in the next free cell.
	const int32 StackIndex = FindStackSlot(Data);
	if (StackIndex != INDEX_NONE)
	{
		Slots[StackIndex].Quantity = FMath::Min(Slots[StackIndex].Quantity + InInstance.Quantity, MaxStackSize);
		L2HTime::LogImportant(FString::Printf(
			TEXT("Inventory %s: stacked %s to %d"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Data),
			Slots[StackIndex].Quantity));

		BroadcastChanged();
		return true;
	}

	const int32 FreeIndex = FindFreeSlot(Data);
	if (FreeIndex == INDEX_NONE)
	{
		return false;
	}

	Slots[FreeIndex] = InInstance;
	Slots[FreeIndex].bSecondaryHalf = false;

	if (Data->Data.bOccupiesTwoSlots && Slots.IsValidIndex(FreeIndex + 1))
	{
		// The second cell is a marker, not an item: it holds no data of its own and exists so the grid
		// cannot place anything on top of the second half of something already stored.
		FL2H_ItemInstance Half;
		Half.Data = Data;
		Half.Quantity = 0;
		Half.bSecondaryHalf = true;
		Half.InstanceId = InInstance.InstanceId;
		Slots[FreeIndex + 1] = Half;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Inventory %s: added %s x%d into cell %d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Data),
		InInstance.Quantity,
		FreeIndex));

	BroadcastChanged();
	return true;
}

bool UL2H_InventoryComponent::ServerAddItem(const UL2H_ItemDataAsset* Data, int32 Quantity)
{
	if (Data == nullptr || Quantity <= 0)
	{
		return false;
	}

	FL2H_ItemInstance Item;
	Item.Data = const_cast<UL2H_ItemDataAsset*>(Data);
	Item.Quantity = Quantity;
	Item.UsesLeft = FMath::Max(Data->Data.MaxUses, 1);
	Item.InstanceId = FGuid::NewGuid();
	Item.KeyTag = FGameplayTag();

	return ServerAddItemInstance(Item);
}

bool UL2H_InventoryComponent::TryAddItem(const UL2H_ItemDataAsset* Data, int32 Quantity)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ServerAddItem(Data, Quantity);
	}

	// A client cannot reach the replicated grid to write it, so the request goes to the server and the
	// server decides whether it fits. True here means "asked", not "stored" -- the grid is the answer.
	ServerAddItemByClass(const_cast<UL2H_ItemDataAsset*>(Data), Quantity);
	return true;
}

void UL2H_InventoryComponent::ServerMoveItem_Implementation(int32 FromIndex, int32 ToIndex)
{
	if (!Slots.IsValidIndex(FromIndex) || !Slots.IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return;
	}

	if (IsEmptySlot(Slots[FromIndex]) || Slots[FromIndex].bSecondaryHalf)
	{
		return;
	}

	// Swapping rather than overwriting: a move onto an occupied cell is a swap, which is what every grid
	// inventory does and what a player expects when they drag one item onto another.
	// Swapped by hand rather than through a helper: SWAP is not a name this engine version defines.
	FL2H_ItemInstance Temp = Slots[FromIndex];
	Slots[FromIndex] = Slots[ToIndex];
	Slots[ToIndex] = Temp;

	L2HTime::LogVerbose(FString::Printf(TEXT("Inventory %s: moved cell %d <-> %d"), *GetNameSafe(GetOwner()), FromIndex, ToIndex));

	BroadcastChanged();
}

void UL2H_InventoryComponent::RequestMove(int32 FromIndex, int32 ToIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerMoveItem_Implementation(FromIndex, ToIndex);
		return;
	}

	ServerMoveItem(FromIndex, ToIndex);
}

void UL2H_InventoryComponent::ServerDropItem_Implementation(int32 Index)
{
	ServerDropItemAt(Index);
}

void UL2H_InventoryComponent::RequestDrop(int32 Index)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerDropItemAt(Index);
		return;
	}

	ServerDropItem(Index);
}

AL2H_ItemPickup* UL2H_InventoryComponent::ServerDropItemAt(int32 Index)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World || !Owner->HasAuthority() || !Slots.IsValidIndex(Index))
	{
		return nullptr;
	}

	const FL2H_ItemInstance Item = Slots[Index];
	if (!Item.IsSet() || Item.bSecondaryHalf)
	{
		return nullptr;
	}

	if (!Item.Data->Data.bDiscardable)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Drop refused: %s is not discardable"), *GetNameSafe(Item.Data)));
		return nullptr;
	}

	if (!PickupClass)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Drop failed: %s has no PickupClass set"), *GetName()));
		return nullptr;
	}

	// Cleared before the spawn so a failed spawn cannot leave the item in two places.
	Slots[Index] = FL2H_ItemInstance();

	const FVector Forward = Owner->GetActorForwardVector();
	const FVector SpawnLocation = Owner->GetActorLocation() + Forward * DropDistance + FVector(0.0f, 0.0f, -Owner->GetSimpleCollisionHalfHeight() + 10.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AL2H_ItemPickup* Dropped = World->SpawnActor<AL2H_ItemPickup>(
		PickupClass,
		SpawnLocation,
		Owner->GetActorRotation(),
		SpawnParams);

	if (!Dropped)
	{
		// Put it back rather than losing it: a drop that silently deletes an item is the worst outcome
		// available here, and the log has to say the spawn failed.
		Slots[Index] = Item;
		L2HTime::LogImportant(FString::Printf(TEXT("Drop failed: could not spawn %s"), *GetNameSafe(PickupClass)));
		return nullptr;
	}

	Dropped->InitialiseFromInstance(Item);

	L2HTime::LogImportant(FString::Printf(
		TEXT("Inventory %s: dropped %s (instance %s)"),
		*GetNameSafe(Owner),
		*GetNameSafe(Item.Data),
		*Item.InstanceId.ToString()));

	BroadcastChanged();
	return Dropped;
}

void UL2H_InventoryComponent::ServerUseItem_Implementation(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(Index))
	{
		return;
	}

	FL2H_ItemInstance& Item = Slots[Index];
	if (!Item.IsSet() || !Item.Data->Data.bUsable)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Use refused: %s is not usable"), *GetNameSafe(Item.Data)));
		return;
	}

	// Captured before the cell is emptied. Once the last use is spent the cell is cleared, and reading the
	// data asset through the reference afterwards would log "None" for the very use that consumed it.
	const FString ItemName = GetNameSafe(Item.Data);

	// A tool (time stop, elixir, injector) takes effect through the time director, and one that cannot
	// take effect right now (no partner in reach, time already bent) is refused and kept, not wasted.
	if (Item.Data->Data.UseEffect != EL2H_ItemUseEffect::None)
	{
		AL2H_BaseCharacter* User = Cast<AL2H_BaseCharacter>(GetOwner());
		AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
		if (!User || !Director || !Director->ApplyItemEffect(User, Item))
		{
			return;
		}
	}

	// A usable item does not change the grid itself. It announces a use so the gameplay layer can turn
	// it into whatever the item means -- a bandage is time through the director, a battery is something
	// not built yet -- and only the grid is this component's business.
	OnItemUsed.Broadcast(Index, Item);

	if (Item.UsesLeft > 0)
	{
		--Item.UsesLeft;
	}

	if (Item.Data->Data.bStackable && Item.Quantity > 1)
	{
		--Item.Quantity;
	}
	else if (Item.UsesLeft <= 0)
	{
		Slots[Index] = FL2H_ItemInstance();
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Inventory %s: used %s from cell %d"), *GetNameSafe(GetOwner()), *ItemName, Index));

	BroadcastChanged();
}

void UL2H_InventoryComponent::RequestUse(int32 Index)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerUseItem_Implementation(Index);
		return;
	}

	ServerUseItem(Index);
}

void UL2H_InventoryComponent::ServerAddItemByClass_Implementation(UL2H_ItemDataAsset* Data, int32 Quantity)
{
	ServerAddItem(Data, Quantity);
}

void UL2H_InventoryComponent::ServerAddItemIntoCell(const UL2H_ItemDataAsset* Data, int32 Index)
{
	if (Data == nullptr || !Slots.IsValidIndex(Index))
	{
		return;
	}

	FL2H_ItemInstance Item;
	Item.Data = const_cast<UL2H_ItemDataAsset*>(Data);
	Item.Quantity = 1;
	Item.UsesLeft = FMath::Max(Data->Data.MaxUses, 1);
	Item.InstanceId = FGuid::NewGuid();

	Slots[Index] = Item;
}

const FL2H_CombineRecipe* UL2H_InventoryComponent::FindRecipe(const UL2H_ItemDataAsset* A, const UL2H_ItemDataAsset* B) const
{
	for (const FL2H_CombineRecipe& Recipe : Recipes)
	{
		// Both orderings are accepted. A recipe is authored as an unordered pair because a player who
		// tries the rag on the bottle must not be told the combination is wrong.
		const bool bDirect = (Recipe.A == A && Recipe.B == B);
		const bool bSwapped = (Recipe.A == B && Recipe.B == A);

		if ((bDirect || bSwapped) && Recipe.Result != nullptr)
		{
			return &Recipe;
		}
	}

	return nullptr;
}

void UL2H_InventoryComponent::ServerCombineItems_Implementation(int32 IndexA, int32 IndexB)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(IndexA) || !Slots.IsValidIndex(IndexB) || IndexA == IndexB)
	{
		return;
	}

	if (!Slots[IndexA].IsSet() || !Slots[IndexB].IsSet())
	{
		return;
	}

	const FL2H_CombineRecipe* Recipe = FindRecipe(Slots[IndexA].Data, Slots[IndexB].Data);
	if (!Recipe)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Combine refused: %s + %s is not a recipe"),
			*GetNameSafe(Slots[IndexA].Data),
			*GetNameSafe(Slots[IndexB].Data)));
		return;
	}

	if (Recipe->bConsumeA)
	{
		Slots[IndexA] = FL2H_ItemInstance();
	}

	if (Recipe->bConsumeB)
	{
		Slots[IndexB] = FL2H_ItemInstance();
	}

	// The result lands in whichever input was consumed, so a combine never needs a free cell and can
	// therefore always be completed once its rule is satisfied.
	const int32 ResultIndex = Recipe->bConsumeA ? IndexA : IndexB;
	if (IsEmptySlot(Slots[ResultIndex]))
	{
		ServerAddItemIntoCell(Recipe->Result, ResultIndex);
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Inventory %s: combined into %s in cell %d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Recipe->Result),
		ResultIndex));

	BroadcastChanged();
}

void UL2H_InventoryComponent::RequestCombine(int32 IndexA, int32 IndexB)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerCombineItems_Implementation(IndexA, IndexB);
		return;
	}

	ServerCombineItems(IndexA, IndexB);
}

void UL2H_InventoryComponent::ServerAssignShortcut_Implementation(int32 SlotIndex, int32 ShortcutIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Shortcuts.IsValidIndex(ShortcutIndex))
	{
		return;
	}

	if (!Slots[SlotIndex].IsSet() || !Slots[SlotIndex].Data->Data.bEquipable)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Shortcut refused: %s is not equipable"), *GetNameSafe(Slots[SlotIndex].Data)));
		return;
	}

	// Copied, not moved: the grid stays the source of truth and the shortcut is a second view of the same
	// item, so a shortcut cannot be the only place an item exists.
	Shortcuts[ShortcutIndex] = Slots[SlotIndex];

	L2HTime::LogVerbose(FString::Printf(
		TEXT("Inventory %s: shortcut %d now holds %s"),
		*GetNameSafe(GetOwner()),
		ShortcutIndex,
		*GetNameSafe(Slots[SlotIndex].Data)));

	BroadcastChanged();
}

void UL2H_InventoryComponent::RequestAssignShortcut(int32 SlotIndex, int32 ShortcutIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerAssignShortcut_Implementation(SlotIndex, ShortcutIndex);
		return;
	}

	ServerAssignShortcut(SlotIndex, ShortcutIndex);
}

void UL2H_InventoryComponent::ServerTransferToStorage_Implementation(AL2H_StorageActor* Storage, int32 SlotIndex)
{
	if (!Storage || !GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FL2H_ItemInstance Item = Slots[SlotIndex];
	if (!Item.IsSet() || Item.bSecondaryHalf)
	{
		return;
	}

	if (Storage->AddItem(Item))
	{
		Slots[SlotIndex] = FL2H_ItemInstance();
		L2HTime::LogImportant(FString::Printf(TEXT("Inventory %s: stored %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Item.Data)));
		BroadcastChanged();
	}
}

void UL2H_InventoryComponent::RequestTransferToStorage(AL2H_StorageActor* Storage, int32 SlotIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerTransferToStorage_Implementation(Storage, SlotIndex);
		return;
	}

	ServerTransferToStorage(Storage, SlotIndex);
}

void UL2H_InventoryComponent::ServerTransferFromStorage_Implementation(AL2H_StorageActor* Storage, int32 StorageIndex)
{
	if (!Storage || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FL2H_ItemInstance Item;
	if (Storage->TakeItemAt(StorageIndex, Item))
	{
		if (!ServerAddItemInstance(Item))
		{
			// Handed back rather than lost: the drawer refused nothing, the player's grid simply had no room,
			// and an item that vanishes at that point is indistinguishable from a bug.
			Storage->AddItem(Item);
			L2HTime::LogImportant(FString::Printf(
				TEXT("Storage transfer refused: %s has no room for %s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Item.Data)));
			return;
		}

		L2HTime::LogImportant(FString::Printf(TEXT("Inventory %s: took %s back out of storage"), *GetNameSafe(GetOwner()), *GetNameSafe(Item.Data)));
	}
}

void UL2H_InventoryComponent::RequestTransferFromStorage(AL2H_StorageActor* Storage, int32 StorageIndex)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerTransferFromStorage_Implementation(Storage, StorageIndex);
		return;
	}

	ServerTransferFromStorage(Storage, StorageIndex);
}

void UL2H_InventoryComponent::ToggleInventoryUI()
{
	if (InventoryWidget)
	{
		CloseInventoryUI();
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	if (!PC)
	{
		// Only a locally controlled pawn has a screen to draw on. The other player's inventory is theirs.
		L2HTime::LogVerbose(FString::Printf(TEXT("Inventory UI skipped: %s has no local controller"), *GetNameSafe(GetOwner())));
		return;
	}

	if (!InventoryWidgetClass)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Inventory UI failed: %s has no InventoryWidgetClass set"), *GetName()));
		return;
	}

	InventoryWidget = CreateWidget<UL2H_InventoryWidget>(PC, InventoryWidgetClass);
	if (!InventoryWidget)
	{
		L2HTime::LogImportant(TEXT("Inventory UI failed: widget could not be created"));
		return;
	}

	InventoryWidget->SetInventorySource(this);
	InventoryWidget->AddToPlayerScreen();

	L2HTime::LogImportant(FString::Printf(TEXT("Inventory UI opened for %s"), *GetNameSafe(GetOwner())));

	BroadcastChanged();
}

void UL2H_InventoryComponent::CloseInventoryUI()
{
	if (!InventoryWidget)
	{
		return;
	}

	InventoryWidget->RemoveFromParent();
	InventoryWidget = nullptr;

	L2HTime::LogImportant(FString::Printf(TEXT("Inventory UI closed for %s"), *GetNameSafe(GetOwner())));

	BroadcastChanged();
}
