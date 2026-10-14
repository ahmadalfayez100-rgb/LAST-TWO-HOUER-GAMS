// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "L2H_CoverComponent.generated.h"

class AL2H_BaseCharacter;
class UAnimMontage;
class UAnimSequenceBase;

/** Where the body is in taking cover: the transitions in and out are phases of their own. */
UENUM(BlueprintType)
enum class EL2H_CoverPhase : uint8
{
	None,
	/** Facing the wall, turning the back to it (StandToCover). */
	Entering,
	/** Back to the wall: idle, moving, peeking. */
	Holding,
	/** Standing up and turning away from the wall (ToStand). */
	Exiting,
	/** Crossing from the end of this wall to the next one (WalkToCover / CrouchToCover). */
	Transferring
};

/**
 * Taking cover against a wall. L3 (the crouch key) next to a wall in front of the character puts its back
 * to it: it then moves only along the wall, stops at its end, and plays the cover clips. Aiming lifts it
 * out of the pose (standing up over a low wall) for as long as the aim is held. L3 again, a jump, or the
 * wall running out leaves cover.
 *
 * The idea is GASPALS' AC_CoverSystem (trace, stick, slide along); the clips are the project's own: the
 * Mixamo set under animation/L2H/Cover for a high wall, the low CoverDown set for a low one.
 *
 * State is replicated so every machine plays the same clips; entry and exit are asked for by the owning
 * client and checked again on the server.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent))
class LASTTWOHOURS_API UL2H_CoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_CoverComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The crouch key was pressed this frame. Acted on in the tick, after the Blueprint's own handling of the key. */
	void NotifyCoverKeyPressed();

	/** The cross key (Circle / Left Alt) was pressed: at a wall's end, the move to the next cover. */
	void NotifyJumpKeyPressed();

	UFUNCTION(BlueprintPure, Category = "L2H|Cover")
	bool IsInCover() const { return bInCover; }

	UFUNCTION(BlueprintPure, Category = "L2H|Cover")
	bool IsHighCover() const { return bHighCover; }

	/** In cover and not exposed by aiming: what the damage reduction asks. */
	bool IsProtected() const;

	/** Keeps a movement request on the wall: only its part along the wall, nothing past the wall's end. */
	FVector ConstrainMovementInput(const FVector& WorldDirection, float& InOutScale) const;

	/** Leaves cover at once (jump, fall, going down). Safe to call when not in cover. */
	void ForceExitCover();

	/** How far ahead (cm, from the capsule's surface) a wall counts as cover. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float EnterDistance = 90.0f;

	/** A wall must reach at least this high (cm above the feet) to hide behind. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float LowCoverMinHeight = 70.0f;

	/** A wall still there at this height (cm above the feet) is high cover: the character stands. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float HighCoverHeight = 140.0f;

	/** Gap kept between the wall and the capsule's surface. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float WallGap = 6.0f;

	/** Speed along the wall (the clips move about 100-115 cm/s). */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float MoveSpeed = 105.0f;

	/** Damage taken while protected is multiplied by this. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float DamageScale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighIdleLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighIdleRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighMoveLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighMoveRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowIdleLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowIdleRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowMoveLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowMoveRight;

	/** Looking out past the wall's end, played while standing still at an edge. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowPeekLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> LowPeekRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighPeekLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim")
	TObjectPtr<UAnimSequenceBase> HighPeekRight;

	/**
	 * Going in: played facing the wall, they end on the matching idle turned 180 degrees (measured 2-4 cm
	 * off), so the capsule turns its back to the wall on the clip's last frame. Left/Right = the side the
	 * body ends up leaning toward.
	 */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighEnterLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighEnterRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowEnterLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowEnterRight;

	/** Going out: they start on the matching idle (0-2 cm off). None = leave with a plain blend. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighExitLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighExitRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowExitLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowExitRight;

	/**
	 * Cover to cover, named by the way they TRAVEL (character's own right/left). Measured on the clips: the
	 * Mixamo "_L" ones travel to the character's right, starting on the right-leaning idle and ending on the
	 * left-leaning one, a SWAT turn on the way. Their L2H_CoverMove curve (0..1) is the RM root's progress.
	 */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighTransferRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> HighTransferLeft;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowTransferRight;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	TObjectPtr<UAnimSequenceBase> LowTransferLeft;

	/**
	 * Added over the Blueprint's input while in cover: Circle / Left Alt go to IA_L2H_CoverCross (dodge and
	 * sprint there are swallowed), and Cross / Space to IA_L2H_CoverBlock, which nothing binds: it only
	 * consumes them, so the DPTS jump and GASP traversal on that key cannot launch the body off the wall.
	 */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Input")
	TObjectPtr<class UInputMappingContext> CoverInputContext;

	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Input")
	int32 CoverInputPriority = 50;

	/** Farthest the next cover may be (cm, from this wall's end, along the line of the wall). */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float MaxTransferDistance = 400.0f;

	/** A gap narrower than this is not a crossing: the wall simply goes on, and walking along it does. */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover")
	float MinTransferGap = 30.0f;

	/** Play rate of the transitions (the Mixamo clips run 1.1-1.5 s). */
	UPROPERTY(EditAnywhere, Category = "L2H|Cover|Anim|Transitions")
	float TransitionRate = 1.25f;

	UFUNCTION(BlueprintPure, Category = "L2H|Cover")
	EL2H_CoverPhase GetPhase() const { return Phase; }

private:
	/** A wall in front: its surface point and outward normal, and whether it is tall enough to stand behind. */
	bool FindCover(FVector& OutPoint, FVector& OutNormal, bool& bOutHigh) const;

	/** Is there still wall behind the body at Offset (cm) along the wall's right-hand direction? */
	bool HasWallAt(float Offset) const;

	/** How far along the wall the edge check looks before a step is allowed. */
	float EdgeLookAhead(float Radius) const;

	void EnterCover(const FVector& Normal, bool bHigh, bool bRight);
	void ExitCover();

	/** L3 while holding: the exit clip, then out. */
	void BeginExit();

	/** Every machine, when the phase changes: starts the transition clip that belongs to it. */
	void StartPhaseVisuals();

	/** Authority or owner: the entering clip is done (or was cut short by aiming). */
	void FinishEnter();

	/** Authority or owner: the exit clip is done; the capsule takes the facing the hips ended on. */
	void FinishExit();

	/**
	 * The next cover past this wall's end on one side: same facing, within MaxTransferDistance, past a gap
	 * of at least MinTransferGap, with a clear path. Out: where the capsule stands there, its normal, height.
	 */
	bool FindTransferTarget(bool bRight, FVector& OutLocation, FVector& OutNormal, bool& bOutHigh) const;

	void BeginTransfer(const FVector& Target, const FVector& Normal, bool bHigh, bool bRight);
	void FinishTransfer();

	UFUNCTION(Server, Reliable)
	void ServerBeginTransfer(FVector_NetQuantize Target, FVector_NetQuantizeNormal Normal, bool bHigh, bool bRight);

	UFUNCTION(Server, Reliable)
	void ServerEnterCover(FVector_NetQuantizeNormal Normal, bool bHigh, bool bRight);

	UFUNCTION(Server, Reliable)
	void ServerExitCover();

	UFUNCTION(Server, Reliable)
	void ServerBeginExit();

	UFUNCTION()
	void OnRep_InCover();

	UFUNCTION()
	void OnRep_Phase();

	/** Owner-side: stands up over a low wall while aiming, crouches back behind it after. */
	void UpdateStance(bool bAiming);

	/** Every machine: holds the back to the wall and the body against it. */
	void HoldToWall(float DeltaTime);

	/** Every machine: the cover clip that fits what the body is doing, or none while aiming. */
	void UpdateAnimation(bool bAiming);
	void StopCoverMontage();

	AL2H_BaseCharacter* GetCharacter() const;
	bool IsOwnerAiming() const;

	UPROPERTY(ReplicatedUsing = OnRep_InCover)
	bool bInCover = false;

	UPROPERTY(Replicated)
	bool bHighCover = false;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal CoverNormal = FVector::ForwardVector;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EL2H_CoverPhase Phase = EL2H_CoverPhase::None;

	/** Side of the transition in progress, so every machine plays the same clip. */
	UPROPERTY(Replicated)
	bool bTransitionRight = false;

	/** The crossing in progress: from, to, and the wall waiting there. */
	UPROPERTY(Replicated)
	FVector_NetQuantize TransferStart;

	UPROPERTY(Replicated)
	FVector_NetQuantize TransferTarget;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal TransferNormal = FVector::ForwardVector;

	UPROPERTY(Replicated)
	bool bTransferHigh = false;

	bool bJumpPressed = false;

	/** Whether this machine's local player currently has CoverInputContext added. */
	bool bCoverInputAdded = false;

	/** Owner only: adds or removes CoverInputContext to match bInCover. */
	void SyncCoverInput();

	/** When this machine's transition clip ends (world seconds). */
	float TransitionEndTime = 0.0f;

	/** The next cover clip starts without a blend: the entering clip ended on exactly its first pose. */
	bool bSnapNextClip = false;

	/** Capsule yaw when the exit began, and how far the exit clip's turn curve has carried it since. */
	float ExitStartYaw = 0.0f;
	float ExitTurn = 0.0f;

	/** Capsule yaw when the entry began, and the scale from the clip's 180 deg curve to the turn needed. */
	float EnterStartYaw = 0.0f;
	float EnterTurnScale = 1.0f;

	bool bKeyPressed = false;

	/** Side the stick last asked for along the wall (+1 right, -1 left) and the frame it did. Locally controlled only. */
	mutable int32 IntentRight = 0;
	mutable uint64 IntentFrame = 0;
	bool bFaceRight = false;
	bool bStoodUpToAim = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayingAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;
};
