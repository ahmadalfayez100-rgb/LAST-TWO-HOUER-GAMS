// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_ReadableActor.generated.h"

class UStaticMeshComponent;

/**
 * A sheet of paper the player can read.
 *
 * One key does all three things -- read it, turn it over, put it down -- because a document has no
 * second verb to offer and a separate close key would be a key the player has to remember for no
 * reason. Which of the three happens is derived from bOpen and bFrontSide, both replicated, so both
 * players in a co-op session are looking at the same face of the same sheet.
 *
 * The text lives in the actor rather than in a widget: a note is a thing in the world whose content is
 * authored where it is placed, and the read widget only has to ask the focused readable what it says.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Readable"))
class LASTTWOHOURS_API AL2H_ReadableActor : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_ReadableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IL2H_InteractableInterface. Bodies live here, never in the interface's own translation unit.
	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	/** The face currently showing, which is what the read widget prints. */
	UFUNCTION(BlueprintPure, Category = "L2H|Readable")
	FText GetVisibleText() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Readable")
	FText GetReadableTitle() const { return Title; }

	UFUNCTION(BlueprintPure, Category = "L2H|Readable")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "L2H|Readable")
	bool IsFrontSide() const { return bFrontSide; }

	/** Turn the sheet over. Server-only, and refused when there is nothing on the back. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Readable")
	bool Flip();

	/** Fired on every machine whenever the open state or the side changes. Repaint the read UI here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Readable")
	void OnReadableStateChanged(bool bNewOpen, bool bNewFrontSide);

protected:
	UFUNCTION()
	void OnRep_ReadableState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Readable")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Readable")
	TObjectPtr<UStaticMeshComponent> PaperMesh;

	/** Shown as the prompt's target, and as the read panel's header. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Readable")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Readable", meta = (MultiLine = "true"))
	FText FrontText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Readable", meta = (MultiLine = "true"))
	FText BackText;

	/** Off for a one-sided notice. With it off, a second press puts the sheet down instead of turning it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Readable")
	bool bHasBackSide = false;

	/** Authored identity. Left invalid, a stable one is derived from the actor's path instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Readable")
	FGuid StateId;

	/** True while a player has the sheet open. Replicated: it is a shared world state, not a local one. */
	UPROPERTY(ReplicatedUsing = OnRep_ReadableState, BlueprintReadOnly, Category = "L2H|Readable")
	bool bOpen = false;

	/** Which face is showing. Replicated so both players read the same side. */
	UPROPERTY(ReplicatedUsing = OnRep_ReadableState, BlueprintReadOnly, Category = "L2H|Readable")
	bool bFrontSide = true;
};
