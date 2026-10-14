// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_ContainerActor.generated.h"

class UStaticMeshComponent;

/**
 * A drawer or locker that holds items.
 *
 * The contents are the actor's own replicated array rather than a pointer to an inventory, so a
 * container is a thing in the world that happens to hold things -- it never needs to know what an
 * inventory is, and the transfer direction is decided by whoever is doing the transferring.
 *
 * The array is swapped whole rather than diffed. A drawer holds a handful of items, and a whole-array
 * OnRep is impossible to get subtly wrong, which is worth more here than the bytes.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Container"))
class LASTTWOHOURS_API AL2H_ContainerActor : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_ContainerActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	/** A copy, so a caller cannot hold a reference into a replicated array that is about to be swapped. */
	UFUNCTION(BlueprintPure, Category = "L2H|Container")
	TArray<FL2H_ItemInstance> GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "L2H|Container")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "L2H|Container")
	bool IsLocked() const { return bLocked; }

	UFUNCTION(BlueprintPure, Category = "L2H|Container")
	FText GetContainerName() const { return DisplayName; }

	/** Server-only. False when the drawer is full. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Container")
	bool AddItem(const FL2H_ItemInstance& Item);

	/** Server-only. Hands the item at Index to the caller and removes it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Container")
	bool TakeItemAt(int32 Index, FL2H_ItemInstance& OutItem);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Container")
	void SetLocked(bool bNewLocked);

	/** Fired on every machine when the contents change, so an open drawer UI can repaint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Container")
	void OnContainerChanged();

protected:
	UFUNCTION()
	void OnRep_Items();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Container")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Container")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Container")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Container", meta = (ClampMin = "1"))
	int32 Capacity = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Container")
	bool bStartLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Container")
	FGuid StateId;

	UPROPERTY(ReplicatedUsing = OnRep_Items, BlueprintReadOnly, Category = "L2H|Container")
	TArray<FL2H_ItemInstance> Items;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Container")
	bool bLocked = false;
};
