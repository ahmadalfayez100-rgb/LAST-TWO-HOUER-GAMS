// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_Breakable.generated.h"

class UL2H_ItemDataAsset;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Something that breaks: a crate by default (MSHF's wooden crate and its broken pieces). Bullets and melee
 * hits wear its Health down; at zero it breaks on every machine -- the whole mesh goes, its pieces scatter
 * (local physics, gone after a few seconds) -- and the server drops what it held: time shards, and an item
 * if one is set.
 *
 * Placed in a level it is part of the saved world: a checkpoint remembers it broken, a rewind puts it back.
 */
UCLASS(meta = (DisplayName = "L2H Breakable"))
class LASTTWOHOURS_API AL2H_Breakable : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_Breakable();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Breakable")
	bool IsBroken() const { return bBroken; }

	// Saved world state (placed ones only); never an Interact target.
	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override {}
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

protected:
	virtual void BeginPlay() override;

	/** Server: the moment it gives way. Subclasses add to it (the barrel explodes). */
	virtual void Break(AActor* Breaker);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Breakable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What it scatters into. Each piece is spawned where the whole was, with physics, then removed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable")
	TArray<TObjectPtr<UStaticMesh>> PieceMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable", meta = (ClampMin = "1.0"))
	float MaxHealth = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable")
	TObjectPtr<USoundBase> BreakSound;

	/** Time shards it drops, and the seconds each one holds. 0 shards = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable", meta = (ClampMin = "0"))
	int32 LootShards = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable", meta = (ClampMin = "1.0"))
	float LootShardSeconds = 8.0f;

	/** Optional: an item lying in it, dropped as a pickup when it breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable")
	TObjectPtr<UL2H_ItemDataAsset> LootItem;

	/** How hard the pieces fly apart, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Breakable", meta = (ClampMin = "0.0"))
	float PieceSpeed = 350.0f;

	UPROPERTY(Replicated)
	float Health = 20.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	UFUNCTION()
	void OnRep_Broken();

	/** Every machine: whole mesh off, pieces out, the sound. */
	void ShowBroken();
	void ShowWhole();

	bool bPlaced = false;
	bool bShownBroken = false;
};
