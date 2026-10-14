// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_InventoryWidget.h"

#include "Item/L2H_InventoryComponent.h"
#include "Item/L2H_StorageActor.h"
#include "Time/L2HTimeLog.h"

void UL2H_InventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bound here rather than in SetInventorySource: the delegate is a per-instance multicast, and a
	// widget that bound in both places would repaint twice on every change.
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UL2H_InventoryWidget::HandleInventoryChanged);
	}

	RefreshInventory();
}

void UL2H_InventoryWidget::NativeDestruct()
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UL2H_InventoryWidget::HandleInventoryChanged);
	}

	Super::NativeDestruct();
}

void UL2H_InventoryWidget::SetInventorySource(UL2H_InventoryComponent* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}

	// Unbound from the old source before the new one is adopted, so a widget handed between two
	// components cannot end up listening to both.
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UL2H_InventoryWidget::HandleInventoryChanged);
	}

	Inventory = InInventory;

	// Only rebind when this widget is already on screen. Otherwise NativeConstruct does it, and doing
	// both would add the same handler twice.
	if (Inventory && IsInViewport())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UL2H_InventoryWidget::HandleInventoryChanged);
	}

	ClearSelection();
	RefreshInventory();
}

void UL2H_InventoryWidget::SetStorageSource(AL2H_StorageActor* InStorage)
{
	Storage = InStorage;

	// The selection is dropped because it referred to a transfer destination that has just changed, and
	// acting on a stale destination is how an item ends up in the wrong box.
	ClearSelection();
	RefreshInventory();
}

TArray<FL2H_ItemInstance> UL2H_InventoryWidget::GetSlots() const
{
	return Inventory ? Inventory->GetSlots() : TArray<FL2H_ItemInstance>();
}

TArray<FL2H_ItemInstance> UL2H_InventoryWidget::GetShortcuts() const
{
	return Inventory ? Inventory->GetShortcuts() : TArray<FL2H_ItemInstance>();
}

TArray<FL2H_ItemInstance> UL2H_InventoryWidget::GetStorageItems() const
{
	return Storage ? Storage->GetItems() : TArray<FL2H_ItemInstance>();
}

int32 UL2H_InventoryWidget::GetSlotsPerRow() const
{
	return Inventory ? Inventory->GetSlotsPerRow() : 1;
}

void UL2H_InventoryWidget::ClearSelection()
{
	SelectedIndex = INDEX_NONE;
	bSelectedShortcut = false;
}

void UL2H_InventoryWidget::RefreshInventory()
{
	OnInventoryRefreshed();
}

void UL2H_InventoryWidget::HandleInventoryChanged()
{
	// A replicated array can shrink under a live selection, so the selection is dropped whenever the
	// contents change. Keeping an index across a change is how a grid ends up acting on the wrong item.
	ClearSelection();
	RefreshInventory();
}

void UL2H_InventoryWidget::SetResultText(const FText& InText)
{
	LastResultText = InText;

	if (!InText.IsEmpty())
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Inventory UI: %s"), *InText.ToString()));
	}
}

FL2H_ItemInstance UL2H_InventoryWidget::GetItemAt(int32 Index, bool bShortcut) const
{
	const TArray<FL2H_ItemInstance> Source = bShortcut ? GetShortcuts() : GetSlots();
	return Source.IsValidIndex(Index) ? Source[Index] : FL2H_ItemInstance();
}

void UL2H_InventoryWidget::HandleSlotClicked(int32 Index, bool bShortcut)
{
	if (!Inventory)
	{
		return;
	}

	// Nothing selected means this click is the first half of a pair, which is what makes the grid
	// two-click without a mode switch to learn.
	if (SelectedIndex == INDEX_NONE)
	{
		if (!GetItemAt(Index, bShortcut).IsSet())
		{
			SetResultText(NSLOCTEXT("L2H|Inventory", "EmptyCell", "That cell is empty."));
			return;
		}

		SelectedIndex = Index;
		bSelectedShortcut = bShortcut;
		SetResultText(FText::GetEmpty());
		RefreshInventory();
		return;
	}

	// Clicking the selected cell again puts it down. Without this there is no way to change your mind
	// except by acting, which is exactly the state a player should not be trapped in.
	if (SelectedIndex == Index && bSelectedShortcut == bShortcut)
	{
		ClearSelection();
		RefreshInventory();
		return;
	}

	// A shortcut row is a copy of a main-grid cell, not a second grid: the component's move and combine
	// both take main-grid indices, so a shortcut can only ever be the destination of an assignment.
	if (bSelectedShortcut)
	{
		SetResultText(bShortcut
			? NSLOCTEXT("L2H|Inventory", "ShortcutToShortcut", "Assign shortcuts from the grid.")
			: NSLOCTEXT("L2H|Inventory", "ShortcutToGrid", "A shortcut copies from the grid; it does not move back."));

		ClearSelection();
		RefreshInventory();
		return;
	}

	if (bShortcut)
	{
		Inventory->RequestAssignShortcut(SelectedIndex, Index);
		ClearSelection();
		return;
	}

	if (GetItemAt(Index, false).IsSet())
	{
		// An occupied target is read as "these two go together". The component refuses and logs when no
		// recipe covers the pair, so a player who meant a swap uses RequestSwapSelectedWith instead.
		Inventory->RequestCombine(SelectedIndex, Index);
	}
	else
	{
		Inventory->RequestMove(SelectedIndex, Index);
	}

	ClearSelection();
}

void UL2H_InventoryWidget::RequestUseSelected()
{
	if (!Inventory || SelectedIndex == INDEX_NONE || bSelectedShortcut)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "UseNeedsGridSelection", "Select an item in the grid first."));
		return;
	}

	// Captured and cleared before the call: using an item can empty the cell, and an index read after
	// that would be an index into a grid that has already changed.
	const int32 Index = SelectedIndex;
	ClearSelection();
	Inventory->RequestUse(Index);
}

void UL2H_InventoryWidget::RequestDropSelected()
{
	if (!Inventory || SelectedIndex == INDEX_NONE || bSelectedShortcut)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "DropNeedsGridSelection", "Select an item in the grid first."));
		return;
	}

	const int32 Index = SelectedIndex;
	ClearSelection();
	Inventory->RequestDrop(Index);
}

void UL2H_InventoryWidget::RequestStoreSelected()
{
	if (!Inventory || !Storage)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "NoStorage", "No storage is open."));
		return;
	}

	if (SelectedIndex == INDEX_NONE || bSelectedShortcut)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "StoreNeedsGridSelection", "Select an item in the grid first."));
		return;
	}

	const int32 Index = SelectedIndex;
	ClearSelection();
	Inventory->RequestTransferToStorage(Storage, Index);
}

void UL2H_InventoryWidget::RequestTakeFromStorage(int32 StorageIndex)
{
	if (!Inventory || !Storage)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "NoStorage", "No storage is open."));
		return;
	}

	ClearSelection();
	Inventory->RequestTransferFromStorage(Storage, StorageIndex);
}

void UL2H_InventoryWidget::RequestAssignSelectedToShortcut(int32 ShortcutIndex)
{
	if (!Inventory || SelectedIndex == INDEX_NONE || bSelectedShortcut)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "ShortcutNeedsGridSelection", "Select an item in the grid first."));
		return;
	}

	const int32 Index = SelectedIndex;
	ClearSelection();
	Inventory->RequestAssignShortcut(Index, ShortcutIndex);
}

void UL2H_InventoryWidget::RequestSwapSelectedWith(int32 Index)
{
	if (!Inventory || SelectedIndex == INDEX_NONE || bSelectedShortcut)
	{
		SetResultText(NSLOCTEXT("L2H|Inventory", "SwapNeedsGridSelection", "Select an item in the grid first."));
		return;
	}

	const int32 From = SelectedIndex;
	ClearSelection();
	Inventory->RequestMove(From, Index);
}

void UL2H_InventoryWidget::RequestClose()
{
	// Routed through the component so its cached pointer and the screen are cleared together. Nothing is
	// touched on this widget afterwards: removing it from the viewport destroys it on the way out.
	if (Inventory)
	{
		Inventory->CloseInventoryUI();
	}
}
