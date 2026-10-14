// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_LockActor.generated.h"

class AL2H_DoorActor;
class UStaticMeshComponent;

/**
 * A combination lock: the pad the player presses, the code they have to know, and the door it opens.
 *
 * The code itself never replicates. It is authored on the server and compared there, so a client can
 * only ever learn that an attempt failed -- which matters in a co-op game where the second player's
 * machine is not a trusted place to keep a puzzle answer.
 *
 * KeypadUser is the one piece of per-player state here, and it exists so the keypad opens on the HUD of
 * the player who pressed the pad and on nobody else's. Without it the second player's screen would grow
 * a keypad they never asked for.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Lock"))
class LASTTWOHOURS_API AL2H_LockActor : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_LockActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	/**
	 * Server-only. Compares Code against the authored one and opens the linked door on a match.
	 *
	 * The comparison is deliberately the ONLY thing that can clear the lock, so the keypad widget and any
	 * future scripted path go through the same check rather than each deciding for itself.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Lock")
	bool SubmitCode(AActor* Interactor, const FString& Code);

	/** Server-only. Dismisses the keypad for whoever has it open. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Lock")
	void CloseKeypad(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category = "L2H|Lock")
	bool IsUnlocked() const { return bUnlocked; }

	/** How many digits the keypad should draw. Derived from the authored code, never authored twice. */
	UFUNCTION(BlueprintPure, Category = "L2H|Lock")
	int32 GetCodeLength() const { return CorrectCode.Len(); }

	/** The player who currently has the keypad open, or null. Replicated. */
	UFUNCTION(BlueprintPure, Category = "L2H|Lock")
	AActor* GetKeypadUser() const { return KeypadUser; }

	UFUNCTION(BlueprintPure, Category = "L2H|Lock")
	FText GetLockName() const { return DisplayName; }

	/** Fired on every machine when an attempt is rejected, so the keypad can flash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Lock")
	void OnCodeRejected();

	/** Fired on every machine once, when the lock gives way. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Lock")
	void OnLockOpened(AActor* OpenedBy);

	/** Fired when the keypad opens or closes, so the HUD knows to show or hide it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Lock")
	void OnKeypadStateChanged(AActor* User, bool bShown);

protected:
	UFUNCTION()
	void OnRep_LockState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Lock")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Lock")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Lock")
	FText DisplayName;

	/** Server-side answer. Never replicated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Lock")
	FString CorrectCode = TEXT("0000");

	/** The door this lock opens. Set per placed instance. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "L2H|Lock")
	TObjectPtr<AL2H_DoorActor> LinkedDoor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Lock")
	FGuid StateId;

	UPROPERTY(ReplicatedUsing = OnRep_LockState, BlueprintReadOnly, Category = "L2H|Lock")
	bool bUnlocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_LockState, BlueprintReadOnly, Category = "L2H|Lock")
	TObjectPtr<AActor> KeypadUser;
};
