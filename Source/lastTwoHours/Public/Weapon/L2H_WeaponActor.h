// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_WeaponActor.generated.h"

class UL2H_WeaponDataAsset;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * A weapon in the world: its mesh, and whether it is up or on the back.
 *
 * The two socket names travel with the weapon rather than being resolved on one machine, because
 * attachment is not a replicated property -- every machine has to perform it, so every machine has to
 * know which socket the current state names. The component re-attaches in the OnRep for exactly that
 * reason.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Weapon"))
class LASTTWOHOURS_API AL2H_WeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AL2H_WeaponActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only. Points the actor at its authored data and re-applies mesh and attachment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	void SetWeaponData(UL2H_WeaponDataAsset* InData);

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	UL2H_WeaponDataAsset* GetWeaponData() const { return WeaponData; }

	/**
	 * The pawn holding this weapon. Called on every machine, which is why it carries no authority gate:
	 * the actor has to attach itself wherever it is drawn, not only where it was spawned.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Weapon")
	void SetWeaponOwner(AActor* InOwner);

	/** Server only. Raised in hand (true) or holstered on the body (false). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	void SetEquipped(bool bNewEquipped);

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	bool IsEquipped() const { return bEquipped; }

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	/** The static-mesh body, for a weapon whose pack shipped no rigged mesh. Hidden while the other draws. */
	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	UStaticMeshComponent* GetStaticMeshView() const { return StaticMeshView; }

	/**
	 * World transform of the socket the shot leaves from.
	 *
	 * Falls back to the actor's own transform rather than failing: a weapon with no authored fire socket
	 * should still shoot, because the alternative is a gun that silently stops working when a name is
	 * spelled differently.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	FTransform GetFireSocketTransform() const;

protected:
	UFUNCTION()
	void OnRep_WeaponData();

	UFUNCTION()
	void OnRep_Equipped();

	/** Applies the mesh and attaches to whichever socket the current state names. Idempotent. */
	void ApplyVisualState();

	/** True when the data names a static mesh and no rigged one. One predicate, so both callers agree. */
	bool UsesStaticMesh() const;

	/** Whichever of the two mesh components currently draws this weapon. Never null. */
public:
	USceneComponent* GetActiveMeshView() const;
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UStaticMeshComponent> StaticMeshView;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponData, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UL2H_WeaponDataAsset> WeaponData;

	UPROPERTY(ReplicatedUsing = OnRep_Equipped, BlueprintReadOnly, Category = "L2H|Weapon")
	bool bEquipped = false;

	/**
	 * The pawn this weapon hangs off. Deliberately NOT replicated: it is a local attachment fact, and
	 * every machine derives it from the same replicated actor reference the component already sends.
	 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> WeaponOwner;
};
