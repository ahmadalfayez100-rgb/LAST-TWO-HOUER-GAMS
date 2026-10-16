// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_ItemPickup.generated.h"

class UL2H_ItemDataAsset;
class UStaticMeshComponent;

/**
 * An item lying in the world, waiting to be taken.
 *
 * It carries a whole FL2H_ItemInstance rather than just a data asset, because dropping an item has to
 * put back the SAME item -- same instance id, same remaining uses, same poisoned state -- and an actor
 * that only knew the asset would hand the player a fresh copy instead.
 *
 * Taking it is deliberate about ownership: the pickup asks the interactor for a UL2H_InventoryComponent
 * by class, and refuses if there is none. It never casts the interactor to a pawn type, which is what
 * lets the same pickup be swept up by a test pawn, the hero, or a future AI.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Item Pickup"))
class LASTTWOHOURS_API AL2H_ItemPickup : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_ItemPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	/** True once a player has taken a placed pickup. A rewind can put it back, so it is hidden, not destroyed. */
	UFUNCTION(BlueprintPure, Category = "L2H|Item")
	bool IsTaken() const { return bTaken; }

	/** Sets what this pickup is, its mesh and its instance id. Server-only, called at spawn. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Item")
	void InitialiseFromInstance(const FL2H_ItemInstance& InInstance);

	UFUNCTION(BlueprintPure, Category = "L2H|Item")
	FL2H_ItemInstance GetItemInstance() const { return Instance; }

	UFUNCTION(BlueprintPure, Category = "L2H|Item")
	UL2H_ItemDataAsset* GetItemData() const { return Instance.Data; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

public:
	/** Optional: taking this pickup also adds the weapon to the taker's loadout (D-pad Down to select it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Item")
	TObjectPtr<class UL2H_WeaponDataAsset> GrantsWeapon;

protected:
	/** The whole instance, replicated, so a client sees what it is about to pick up. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Instance, BlueprintReadOnly, Category = "L2H|Item")
	FL2H_ItemInstance Instance;

	UFUNCTION()
	void OnRep_Instance();

	UPROPERTY(ReplicatedUsing = OnRep_Taken, BlueprintReadOnly, Category = "L2H|Item")
	bool bTaken = false;

	UFUNCTION()
	void OnRep_Taken();

	/** Server: taken, waiting for the hand to reach it before it leaves the world. */
	bool bTakePending = false;

	/** Server: shows or hides the pickup and its interact collision. */
	void SetTaken(bool bNewTaken);

	/** Fired on every machine after the mesh is rebuilt, for glow/interaction feedback. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Item")
	void OnPickupRefreshed();
};
