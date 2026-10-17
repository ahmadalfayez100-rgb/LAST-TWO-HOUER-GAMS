// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_RecallAnchor.generated.h"

class APawn;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * The mark Samer leaves on the floor so he can come back to it.
 *
 * An actor rather than a saved transform because the mark has to be visible to both players and has to
 * survive its owner walking away. A transform stored on the pawn would let a respawn or a level reset
 * quietly move the return point without the player ever seeing it change.
 */
UCLASS()
class LASTTWOHOURS_API AL2H_RecallAnchor : public AActor
{
	GENERATED_BODY()

public:
	AL2H_RecallAnchor();

	UFUNCTION(BlueprintPure, Category = "L2H|Recall")
	APawn* GetAnchorOwner() const { return AnchorOwner; }

	/**
	 * A spot above the mark where the owner can actually stand. It reads his current capsule rather than a
	 * fixed number, so a shrunk Samer returns at the right height.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Recall")
	FVector GetReturnLocation() const;

	void SetAnchorOwner(APawn* InOwner);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Recall")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Recall")
	TObjectPtr<UNiagaraComponent> MarkerFX;

	/**
	 * The looping chevron and rings. Authored on the Blueprint so every machine already has the asset when
	 * the actor replicates, instead of depending on a replicated object reference to carry it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Recall")
	TObjectPtr<UNiagaraSystem> MarkerSystem;

	UPROPERTY(Replicated)
	TObjectPtr<APawn> AnchorOwner;
};
