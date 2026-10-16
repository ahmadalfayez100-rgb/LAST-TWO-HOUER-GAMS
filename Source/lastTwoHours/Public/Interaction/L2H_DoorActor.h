// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_DoorActor.generated.h"

class UStaticMeshComponent;

/**
 * A hinged door that opens, closes, and can be locked.
 *
 * The actor's origin IS the hinge: the leaf mesh is rotated about it, so a designer places the actor on
 * the hinge line and never has to author a pivot offset. What replicates is only the two facts that
 * matter -- open/closed and locked -- and each machine interpolates the swing locally from those. That
 * is deliberate: replicating the leaf angle itself would spend bandwidth on an animation, and two
 * machines that started interpolating at slightly different times would still agree once both settled.
 *
 * Locked is a state the door owns, not a check the door performs: whoever holds the key or solves the
 * lock calls Unlock(). A door that asked the inventory itself would have to know what an inventory is.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Door"))
class LASTTWOHOURS_API AL2H_DoorActor : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_DoorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Door")
	bool IsLocked() const { return bLocked; }

	UFUNCTION(BlueprintPure, Category = "L2H|Door")
	bool IsOpen() const { return bOpen; }

	/** Clears the lock. Called by a lock actor, a key check, or a scripted unlock. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Door")
	void Unlock(AActor* UnlockedBy);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Door")
	void SetLocked(bool bNewLocked);

	/** Server-only. Opens or closes without any lock check, for a scripted door. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Door")
	void SetOpen(bool bNewOpen);

	/** Fired on every machine whenever the door's state changes. Foley and lights hook up here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Door")
	void OnDoorStateChanged(bool bNewOpen, bool bNewLocked);

protected:
	UFUNCTION()
	void OnRep_DoorState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The leaf and the frame are separate meshes so one door Blueprint covers every doorway. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	FText DisplayName;

	/** Degrees the leaf swings to. A negative value swings the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	float OpenAngle = 95.0f;

	/** Seconds from shut to fully open. This is the whole animation -- there is no curve asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door", meta = (ClampMin = "0.05"))
	float OpenSeconds = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	bool bStartLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	bool bStartOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Door")
	FGuid StateId;

	UPROPERTY(ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "L2H|Door")
	bool bOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "L2H|Door")
	bool bLocked = false;

private:
	/** 0 shut, 1 fully open. Local to each machine, driven towards bOpen in Tick. */
	float OpenAlpha = 0.0f;

	/** Server: a push clip is on its way to the door; the door opens when it fires. */
	FTimerHandle PushTimer;
};
