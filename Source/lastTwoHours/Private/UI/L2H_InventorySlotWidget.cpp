// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_InventorySlotWidget.h"

#include "Engine/Texture2D.h"
#include "Item/L2H_ItemDataAsset.h"
#include "UI/L2H_InventoryWidget.h"

void UL2H_InventorySlotWidget::SetSlotData(UL2H_InventoryWidget* InOwnerGrid, int32 InSlotIndex, bool bInShortcut, const FL2H_ItemInstance& InItem)
{
	OwnerGrid = InOwnerGrid;
	SlotIndex = InSlotIndex;
	bShortcut = bInShortcut;
	Item = InItem;

	OnSlotRefreshed(Item, IsEmpty(), IsSelected());
}

bool UL2H_InventorySlotWidget::IsSelected() const
{
	// Both halves are compared. The grid and the shortcut row are two different index spaces, so index 2
	// of one is not index 2 of the other, and a cell that ignored the row would light up wrong.
	return OwnerGrid != nullptr
		&& OwnerGrid->GetSelectedIndex() == SlotIndex
		&& OwnerGrid->IsSelectedShortcut() == bShortcut;
}

UTexture2D* UL2H_InventorySlotWidget::GetIcon() const
{
	return Item.Data ? Item.Data->Data.Icon : nullptr;
}

FText UL2H_InventorySlotWidget::GetDisplayName() const
{
	return Item.Data ? Item.Data->Data.DisplayName : FText::GetEmpty();
}

void UL2H_InventorySlotWidget::HandleClicked()
{
	if (OwnerGrid)
	{
		OwnerGrid->HandleSlotClicked(SlotIndex, bShortcut);
	}
}
