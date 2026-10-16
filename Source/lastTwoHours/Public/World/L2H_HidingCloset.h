// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_HidingCloset.generated.h"

class AL2H_BaseCharacter;
class UAnimSequenceBase;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A wardrobe to hide in (ResidentHorrorV2's closet: a body and two doors). Triangle in front of it: the
 * doors swing open, the player steps in and turns round, the doors close. While the doors are shut the
 * one inside is hidden: robots do not pick them as a target, and a swing already on its way misses.
 * Triangle again and they step back out.
 *
 * Hiding costs nothing but what it always costs -- time keeps running inside. One person per closet; a
 * player who goes down inside (time ran out) is pushed back out.
 */
UCLASS(meta = (DisplayName = "L2H Hiding Closet"))
class LASTTWOHOURS_API AL2H_HidingCloset : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_HidingCloset();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** True while Who is inside a closet with its doors shut. */
	static bool IsHiding(const AActor* Who);

	/** The closet Who is inside (doors open or shut), or null. */
	static AL2H_HidingCloset* FindClosetOf(const AActor* Who);

	/** Server: an enemy pulls the doors open; the one inside is put out in front. False when nobody is inside. */
	bool ForceOpen();

	/** Where something standing in front of the closet faces it from. */
	FVector GetFrontPoint(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "L2H|Closet")
	AL2H_BaseCharacter* GetOccupant() const { return Occupant; }

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override { OutId = FGuid(); }
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override {}
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override {}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	TObjectPtr<UStaticMeshComponent> DoorLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	TObjectPtr<UStaticMeshComponent> DoorRight;

	/** How far the doors swing open, in degrees. Each turns about its outer edge, outwards; both are found from
	 * the meshes, so the pivots the art came with do not matter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0", ClampMax = "170.0"))
	float DoorOpenAngle = 105.0f;

	/** Degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "10.0"))
	float DoorSpeed = 260.0f;

	/** The way out of the closet, as a yaw relative to the actor. Found from where the doors sit unless set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	bool bFrontFromDoors = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (EditCondition = "!bFrontFromDoors"))
	float FrontYaw = 0.0f;

	/** Nudge from the middle of the closet floor to where the hidden player stands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	FVector InsideOffset = FVector::ZeroVector;

	/** How far in front of the closet a player stands to get in, and is put back out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0"))
	float OutsideDistance = 95.0f;

	/** How far from the doors the player stands to open them: within reach of the hand in the clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0"))
	float ReachDistance = 45.0f;

	/** Seconds from the press until the player is inside: the opening clip up to the end of the pull. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0"))
	float EnterSeconds = 1.4f;

	/** Seconds from the press until the doors start to swing: when the hand in the clip pulls them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0"))
	float DoorPullTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet", meta = (ClampMin = "0.0"))
	float CloseDelay = 0.35f;

	/** Played by the player at the press: reaching for the doors and pulling them (ResidentHorrorV2's, retargeted). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	TObjectPtr<UAnimSequenceBase> OpenAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Closet")
	TObjectPtr<USoundBase> DoorSound;

	UPROPERTY(ReplicatedUsing = OnRep_DoorsOpen)
	bool bDoorsOpen = false;

	UPROPERTY(Replicated)
	TObjectPtr<AL2H_BaseCharacter> Occupant;

	/** Server: true once the occupant is inside (until they start to leave). */
	UPROPERTY(Replicated)
	bool bInside = false;

	UFUNCTION()
	void OnRep_DoorsOpen();

	virtual void BeginPlay() override;

	FVector GetFront() const;
	FVector GetOutsidePoint() const;

	void Enter(AL2H_BaseCharacter* Character);
	void Leave();
	void StepIn();
	void StepOut();
	void SetDoorsOpen(bool bOpen);

	/** Every machine: the body put in place and held (or let go), so the owner's own movement cannot undo it. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlace(AL2H_BaseCharacter* Character, FVector_NetQuantize Location, float Yaw, bool bHold);

private:
	struct FDoorHinge
	{
		FTransform Closed;
		FVector Hinge = FVector::ZeroVector;
		float Sign = 1.0f;
	};
	FDoorHinge Hinges[2];
	void MeasureDoors();
	void PoseDoors();
	float DoorAlpha = 0.0f;
	// Measured from the meshes at BeginPlay, in actor space.
	FVector BodyCentre = FVector::ZeroVector;
	FVector FrontLocal = FVector::ForwardVector;
	float FloorZ = 0.0f;
	float BodyHalfDepth = 50.0f;
	FTimerHandle StepTimer;
	FTimerHandle DoorTimer;
};
