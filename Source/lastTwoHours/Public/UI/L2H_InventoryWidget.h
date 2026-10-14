// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_InventoryWidget.generated.h"

class AL2H_StorageActor;
class UL2H_InventoryComponent;

/**
 * The grid, drawn.
 *
 * Owns no items and no authority. Every action leaves through a server RPC on the inventory component,
 * and the widget only ever draws what comes back -- which is what makes the same widget correct on a
 * client, where a click is a request, and on a listen server, where it is a call that already happened.
 *
 * The first click selects a cell; the second decides what the pair means. That decision lives here and
 * not in the cell, because only the grid can see both ends of it, and a cell that decided alone could
 * not tell a move from a combine from a store.
 */
UCLASS(Abstract, meta = (DisplayName = "L2H Inventory Widget"))
class LASTTWOHOURS_API UL2H_InventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Points the grid at the component it draws. Called by the component as the widget is created. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void SetInventorySource(UL2H_InventoryComponent* InInventory);

	/** Binds a locker as the transfer partner. Null means the grid is not in a storage session. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void SetStorageSource(AL2H_StorageActor* InStorage);

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	UL2H_InventoryComponent* GetInventorySource() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	AL2H_StorageActor* GetStorageSource() const { return Storage; }

	// -------------------------------------------------------------------------------------------------
	// What the Blueprint child reads when it rebuilds. Copies, because the grid swaps whole arrays.
	// -------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> GetSlots() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> GetShortcuts() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> GetStorageItems() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetSlotsPerRow() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool HasStorage() const { return Storage != nullptr; }

	// -------------------------------------------------------------------------------------------------
	// Selection. Two index spaces, kept apart on purpose.
	// -------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool IsSelectedShortcut() const { return bSelectedShortcut; }

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void ClearSelection();

	/**
	 * The one entry point for a click on a cell. A slot widget reaches it through HandleClicked.
	 *
	 * The second argument is what keeps the shortcut strip and the main grid from acting on each other's
	 * indices: the component's move and combine both take main-grid indices, so the row a click came from
	 * is not decoration, it is which index space was meant.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void HandleSlotClicked(int32 Index, bool bShortcut);

	// -------------------------------------------------------------------------------------------------
	// Actions. Each leaves through the component, so the authority decides.
	// -------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestUseSelected();

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestDropSelected();

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestStoreSelected();

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestTakeFromStorage(int32 StorageIndex);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestAssignSelectedToShortcut(int32 ShortcutIndex);

	/**
	 * Explicit swap, for a Blueprint child that wants to bind it to its own gesture.
	 *
	 * A click on an occupied cell is read as "these two go together", so without this there would be no
	 * way to ask for the swap the component supports but a plain click cannot distinguish from a combine.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestSwapSelectedWith(int32 Index);

	/** Closes the grid through the component, so the component's pointer and the screen agree. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestClose();

	/** Re-reads the component and repaints. Safe to call before a source is bound. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RefreshInventory();

	/** Fired after every rebuild, for the Blueprint child to lay the grid out. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Inventory")
	void OnInventoryRefreshed();

	/** The last thing that happened, for a status line. A silent no-op reads as a broken button. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Inventory")
	FText LastResultText;

protected:
	/** The component this grid draws. Bound by SetInventorySource; the component owns this widget. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<UL2H_InventoryComponent> Inventory;

	/** The locker currently open beside the grid, or null. Bound by SetStorageSource. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<AL2H_StorageActor> Storage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|Inventory")
	bool bSelectedShortcut = false;

	UFUNCTION()
	void HandleInventoryChanged();

	/** The item in one cell of one row, or an empty instance when the index is out of range. */
	FL2H_ItemInstance GetItemAt(int32 Index, bool bShortcut) const;

	void SetResultText(const FText& InText);
};
