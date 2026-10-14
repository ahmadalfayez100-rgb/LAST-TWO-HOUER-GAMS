// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_InventoryComponent.generated.h"

class AL2H_StorageActor;
class UL2H_InventoryWidget;
class UL2H_ItemDataAsset;

/**
 * One combining rule: A plus B becomes Result.
 *
 * An authored array on the component rather than a DataTable asset, because a recipe is a relationship
 * between two item assets and reading it as rows in a spreadsheet hides the two inputs it depends on.
 * The two consume flags exist so a rule can leave a tool in place while the rag it was wrapped around
 * turns into something else.
 */
USTRUCT(BlueprintType)
struct FL2H_CombineRecipe
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<UL2H_ItemDataAsset> A;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<UL2H_ItemDataAsset> B;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Inventory")
	TObjectPtr<UL2H_ItemDataAsset> Result;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Inventory")
	bool bConsumeA = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Inventory")
	bool bConsumeB = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FL2HOnInventoryChanged);

/**
 * Fired on the AUTHORITY when a usable item is consumed.
 *
 * The grid deliberately stops here: an item that healed would have to know about time, and time has
 * exactly one entry point (AL2H_TimeDirector). So the grid announces the use and the gameplay layer
 * decides what it meant.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FL2HOnItemUsed, int32, SlotIndex, FL2H_ItemInstance, Item);

/**
 * The player's carry slot, as a grid.
 *
 * A fixed grid of cells rather than a list: the slot index IS the position on screen, so moving an item
 * is moving an index, a two-cell item is a placement rule over two adjacent indices, and the widget
 * never has to work out where anything goes. An empty cell is a FL2H_ItemInstance with no data asset,
 * which means there is no separate "slot" type to keep in step with the items.
 *
 * Everything that changes the contents goes through a server RPC. The array is replicated whole with
 * one OnRep, because a grid is small enough that a whole-array copy is cheaper to get right than a diff,
 * and a diff is where a two-player session ends up with two different inventories.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Inventory"))
class LASTTWOHOURS_API UL2H_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_InventoryComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fired on every machine whenever the contents change or the UI opens. Rebuild the grid here. */
	UPROPERTY(BlueprintAssignable, Category = "L2H|Inventory")
	FL2HOnInventoryChanged OnInventoryChanged;

	/** Fired on the authority when a usable item is used, for the gameplay layer to interpret. */
	UPROPERTY(BlueprintAssignable, Category = "L2H|Inventory")
	FL2HOnItemUsed OnItemUsed;

	// -------------------------------------------------------------------------------------------
	// Queries
	// -------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> GetShortcuts() const { return Shortcuts; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 GetSlotsPerRow() const { return SlotsPerRow; }

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	FL2H_ItemInstance GetSlotItem(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	FL2H_ItemInstance GetShortcutItem(int32 Index) const;

	/** True when any held item carries Tag. This is how a door asks for a key without knowing its name. */
	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool HasItemWithTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	int32 CountItemsOfType(EL2H_PickupType Type) const;

	/** The first bag cell holding a tool with this use effect, or INDEX_NONE. */
	int32 FindToolCell(EL2H_ItemUseEffect Effect) const;

	/**
	 * Whether Quantity of Data would fit right now.
	 *
	 * A caller asks before acting rather than being told afterwards, because "the pickup refused" and "the
	 * pickup was taken and then thrown away" look identical from the world's side.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	bool CanAccept(const UL2H_ItemDataAsset* Data, int32 Quantity) const;

	// -------------------------------------------------------------------------------------------
	// Mutations. All route to the server.
	// -------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	bool TryAddItem(const UL2H_ItemDataAsset* Data, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestMove(int32 FromIndex, int32 ToIndex);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestDrop(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestUse(int32 Index);

	/** Server only: uses the cell as if its owner had asked (a reload spending a prepaid magazine). */
	void UseOnServer(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestCombine(int32 IndexA, int32 IndexB);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestAssignShortcut(int32 SlotIndex, int32 ShortcutIndex);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestTransferToStorage(AL2H_StorageActor* Storage, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void RequestTransferFromStorage(AL2H_StorageActor* Storage, int32 StorageIndex);

	// -------------------------------------------------------------------------------------------
	// Server-side API. Pickups and drops call these directly, because they already run on the authority.
	// -------------------------------------------------------------------------------------------

	/** Adds an exact instance, preserving its id and remaining uses. The pickup path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Inventory")
	bool ServerAddItemInstance(const FL2H_ItemInstance& InInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Inventory")
	bool ServerAddItem(const UL2H_ItemDataAsset* Data, int32 Quantity);

	/** Server only. Replaces the whole bag with a checkpoint's copy (slot count kept as configured). */
	void RestoreContents(const TArray<FL2H_ItemInstance>& InSlots, const TArray<FL2H_ItemInstance>& InShortcuts);

	/** Spawns the item back into the world in front of the owner. Returns what was dropped, or null. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Inventory")
	AL2H_ItemPickup* ServerDropItemAt(int32 Index);

	// -------------------------------------------------------------------------------------------
	// UI
	// -------------------------------------------------------------------------------------------

	/** Opens or closes the grid for THIS player. Local: the widget belongs to one screen. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void ToggleInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "L2H|Inventory")
	void CloseInventoryUI();

	UFUNCTION(BlueprintPure, Category = "L2H|Inventory")
	UL2H_InventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

protected:
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_Shortcuts();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerMoveItem(int32 FromIndex, int32 ToIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerDropItem(int32 Index);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerUseItem(int32 Index);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerCombineItems(int32 IndexA, int32 IndexB);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerAssignShortcut(int32 SlotIndex, int32 ShortcutIndex);

	/**
	 * The client's "I added something" request.
	 *
	 * Non-const pointer rather than const on purpose: an RPC parameter is copied through the reflection
	 * system, and a const-qualified object pointer is one more thing for that path to get wrong for no
	 * benefit -- the callee is going to treat it as read-only anyway.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerAddItemByClass(UL2H_ItemDataAsset* Data, int32 Quantity);

	/** Puts a fresh item into one named cell. Used by the combine path so a result needs no free cell. */
	void ServerAddItemIntoCell(const UL2H_ItemDataAsset* Data, int32 Index);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerTransferToStorage(AL2H_StorageActor* Storage, int32 SlotIndex);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Inventory")
	void ServerTransferFromStorage(AL2H_StorageActor* Storage, int32 StorageIndex);

	/** First free cell that fits Data, or INDEX_NONE. Understands two-cell items. */
	int32 FindFreeSlot(const UL2H_ItemDataAsset* Data) const;

	/** A cell a stack of Data already lives in and can still grow, or INDEX_NONE. */
	int32 FindStackSlot(const UL2H_ItemDataAsset* Data) const;

	const FL2H_CombineRecipe* FindRecipe(const UL2H_ItemDataAsset* A, const UL2H_ItemDataAsset* B) const;

	void BroadcastChanged();

	/** Cells the grid has. Authored per inventory, so a character can start with fewer pockets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 12;

	/** Cells per row, used only to stop a two-cell item straddling a row break. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory", meta = (ClampMin = "1"))
	int32 SlotsPerRow = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory", meta = (ClampMin = "1"))
	int32 ShortcutCount = 4;

	/** Ceiling for one stack. A cap is what makes any stack finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory", meta = (ClampMin = "1"))
	int32 MaxStackSize = 20;

	/** What a dropped item becomes. Authored so the inventory never names a Blueprint in C++. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory")
	TSubclassOf<AL2H_ItemPickup> PickupClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory")
	TSubclassOf<UL2H_InventoryWidget> InventoryWidgetClass;

	/** How far in front of the owner a dropped item lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory", meta = (ClampMin = "10.0"))
	float DropDistance = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Inventory")
	TArray<FL2H_CombineRecipe> Recipes;

	UPROPERTY(ReplicatedUsing = OnRep_Slots, BlueprintReadOnly, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_Shortcuts, BlueprintReadOnly, Category = "L2H|Inventory")
	TArray<FL2H_ItemInstance> Shortcuts;

	/** Local to this player's screen; never replicated. */
	UPROPERTY(Transient)
	TObjectPtr<UL2H_InventoryWidget> InventoryWidget;
};
