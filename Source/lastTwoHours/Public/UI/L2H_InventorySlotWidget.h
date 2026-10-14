// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_InventorySlotWidget.generated.h"

class UL2H_InventoryWidget;
class UTexture2D;

/**
 * One cell of the grid.
 *
 * It knows its own index and nothing else. Where a click goes is the grid's decision, because only the
 * grid can see two cells at once -- and a cell that decided for itself could not tell a move from a
 * combine, which is the whole reason the grid is two-click rather than one.
 *
 * The item arrives as a copy. The grid swaps its array whole on every change, so anything this widget
 * held by reference would be pointing at a cell that no longer holds what it held.
 */
UCLASS(Abstract, meta = (DisplayName = "L2H Inventory Slot Widget"))
class LASTTWOHOURS_API UL2H_InventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Points this cell at one index of one grid. Called by the grid on every rebuild. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void SetSlotData(UL2H_InventoryWidget* InOwnerGrid, int32 InSlotIndex, bool bInShortcut, const FL2H_ItemInstance& InItem);

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	FL2H_ItemInstance GetItem() const { return Item; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** True when this cell mirrors the shortcut row rather than the main grid. */
	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool IsShortcut() const { return bShortcut; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool IsEmpty() const { return !Item.IsSet(); }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool IsSelected() const;

	/** Icon to draw, or null for an empty cell and for an item authored without one. */
	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	UTexture2D* GetIcon() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetQuantity() const { return Item.Quantity; }

	/**
	 * The click entry point. A Blueprint Button's OnClicked binds straight to this.
	 *
	 * Public and BlueprintCallable because the layout is authored in the Blueprint child, and the only
	 * thing that child should have to know about the grid is which cell was clicked.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void HandleClicked();

	/** Fired after every SetSlotData, for the Blueprint child to paint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Inventory")
	void OnSlotRefreshed(const FL2H_ItemInstance& InItem, bool bInEmpty, bool bInSelected);

protected:
	/** The grid this cell belongs to. Never owned by the cell; the component owns the grid. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<UL2H_InventoryWidget> OwnerGrid;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	FL2H_ItemInstance Item;

	/** INDEX_NONE until the grid has pointed this cell at something. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	bool bShortcut = false;
};
