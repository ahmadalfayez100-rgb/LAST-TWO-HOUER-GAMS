// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"

#include "Time/L2H_TimeRequest.h"

#include "L2H_BaseCharacter.generated.h"

class AL2H_CheckpointTerminal;
class UL2H_TimeMenuWidget;
class UL2H_TimeRequestPromptWidget;
class UL2H_TimeQuickMenuWidget;
class UL2H_CoverComponent;

class UCameraComponent;
class UCameraShakeBase;
class UInputAction;
class UInputMappingContext;
class UGameplayAbility;
class UL2H_AbilitySet;
class UL2H_AbilitySystemComponent;
class UL2H_TimeAttributeSet;
class UL2H_TimeClockWidget;
class USpringArmComponent;
class UAbilitySystemComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UNiagaraComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
struct FInputActionValue;

/**
 * One input-to-ability pairing, read by AL2H_BaseCharacter::SetupPlayerInputComponent.
 *
 * The action and the tag are the whole row: pressing the action activates whatever ability the pawn
 * holds whose asset tags contain the tag. A character ability is therefore added by adding a row and
 * granting the ability -- no new C++ handler, and no place where an input and an ability can drift
 * apart, because the tag is written once.
 */
USTRUCT(BlueprintType)
struct FL2H_AbilityInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TObjectPtr<UInputAction> Action;

	/** Matched against the granted ability's asset tags by TryActivateAbilitiesByTag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	FGameplayTag AbilityTag;
};

/**
 * The TimeCore player pawn.
 *
 * Deliberately standalone. It shares no C++ base with BP_L2H_Base or BP_L2H_Character and never
 * touches the GASP/DPTS movement layer, so the TimeCore prototype cannot regress traversal.
 *
 * Input is Enhanced Input pattern B -- the bindings live in SetupPlayerInputComponent -- so this
 * pawn does not depend on the Blueprint EnhancedInputAction nodes the existing pawns use, and it
 * never reads IMC_Sandbox.
 *
 * One rule shapes the movement members below: a GameplayEffect may modify only attributes, so
 * MaxWalkSpeed is never touched by an effect. Anything that needs to slow this pawn (carry,
 * downed) goes through the cache -> change -> restore helpers instead.
 */
UCLASS()
class LASTTWOHOURS_API AL2H_BaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AL2H_BaseCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Drives the aiming laser while the sights are up. The beam is cosmetic and purely local, so it is
	 * refreshed on the machine that owns the camera and never replicated.
	 */
	virtual void Tick(float DeltaSeconds) override;

	/** In cover, movement is kept along the wall (see UL2H_CoverComponent::ConstrainMovementInput). */
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

	/** Fall damage: a landing faster than FallDamageMinSpeed costs Health (and so time). */
	virtual void Landed(const FHitResult& Hit) override;

	// ---------------------------------------------------------------------------------------
	// Ability system
	// ---------------------------------------------------------------------------------------

	/** IAbilitySystemInterface. The ASC lives on this pawn, never on the PlayerState. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "L2H|Abilities")
	UL2H_AbilitySystemComponent* GetL2HAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "L2H|Abilities")
	UL2H_TimeAttributeSet* GetTimeAttributeSet() const { return TimeAttributeSet; }

	// ---------------------------------------------------------------------------------------
	// State
	// ---------------------------------------------------------------------------------------

	/** True while this pawn is at zero time: ragdolled, not dead, always recoverable. */
	UFUNCTION(BlueprintPure, Category = "L2H|State")
	bool IsDowned() const { return bDowned; }

	/** True while another pawn is carrying this one. */
	UFUNCTION(BlueprintPure, Category = "L2H|State")
	bool IsBeingCarried() const { return bBeingCarried; }

	/**
	 * True once Health has run out. Death, as opposed to Downed: the body is just as limp, but nothing
	 * in the game picks it back up except a revive, and AreAllPlayersDowned must not count it -- which
	 * is exactly why this is a separate flag rather than a second pose.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	bool IsDead() const { return bDead; }

	/** True while this pawn is dead and still inside its free revive window (both screens agree). */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	bool IsFreeReviveOpen() const;

	/** Seconds left in the free revive window, 0 when there is none. For the HUD. */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	float GetFreeReviveSecondsLeft() const;

	/** 0..1 while this player is holding Interact over a partner, -1 otherwise. For the HUD. */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	float GetReviveHoldProgress() const;

	// ---------------------------------------------------------------------------------------
	// Bank (Chrons): leftover seconds banked at the end of a stage, spent in the shop. Server writes,
	// both screens read.
	// ---------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "L2H|Bank")
	float GetChrons() const { return Chrons; }

	/** Server only. Adds (or with a negative value, removes) banked seconds; never below zero. */
	void AddChrons(float Delta);

	/** Server only. A checkpoint restore puts the bank back to what the record holds. */
	void SetChrons(float Value);

	/** True while this player stands in a hub whose stage they just finished: carry or bank. */
	UFUNCTION(BlueprintPure, Category = "L2H|Bank")
	bool IsHubBankOpen() const { return bHubBankOpen; }

	float GetHubCarryAllowance() const { return HubCarryAllowance; }
	float GetHubCarried() const { return HubCarried; }
	void SetHubCarried(float Value) { HubCarried = Value; }
	void OpenHubBank(float Allowance);
	void CloseHubBank();

	/** Hits the time shield still takes before Health does. Replicated for the clock. */
	UFUNCTION(BlueprintPure, Category = "L2H|Tools")
	int32 GetShieldHits() const { return ShieldHits; }

	/** Server only. */
	void SetShieldHits(int32 Hits) { ShieldHits = FMath::Max(Hits, 0); }

	/** Where this player stood, and with how much Health, a moment ago. Recorded on the server for the rewind watch. */
	struct FRewindSample
	{
		float Time = 0.0f;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float Health = 0.0f;
	};

	/**
	 * Server only. The sample Seconds before now -- or before the fall, for a player on the floor -- taken
	 * standing on the ground, so the watch never puts anyone back in mid-air. False when there is none.
	 */
	bool FindRewindSample(float Seconds, FRewindSample& OutSample) const;

	void ClearRewindHistory() { RewindHistory.Reset(); }

	/** The hub whose stage this player last finished (None before the first). The clock points at the nearest other hub. */
	FName GetLastHubId() const { return LastHubId; }
	void SetLastHubId(FName Id) { LastHubId = Id; }

	/** Server world time of this player's last transfer given (GameState clock), for the free window. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Transfer")
	float LastTransferGiveTime = -1000.0f;

	/** Seconds received while on the floor, waiting to reach the remote revive threshold. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Transfer")
	float PendingReviveSeconds = 0.0f;

	/** +1 carries a step of the bank onto the clock for the next stage, -1 banks it back. */
	UFUNCTION(Server, Reliable)
	void ServerAdjustCarry(int32 Direction);

	/** Server-authoritative. 0 time means Downed, never death, so the pawn always survives this. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|State")
	void SetDowned(bool bNewDowned);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|State")
	void SetBeingCarried(bool bNewBeingCarried);

	/** Routed to the server from a locally predicted caller (an ability or the HUD). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|State")
	void ServerSetBeingCarried(bool bNewBeingCarried);

	// ---------------------------------------------------------------------------------------
	// Health
	// ---------------------------------------------------------------------------------------

	/** Server-authoritative. Death. Only a revive clears it -- recovering time never will. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void SetDead(bool bNewDead);

	/**
	 * Server-authoritative. Brings a dead pawn back with this much Health and this much Time.
	 *
	 * A wrapper rather than two writes here: the director owns every number that moves, Health and
	 * Time alike. This exists so a revive station, a companion pawn and the terminal can each say
	 * "bring this player back" without holding a director reference of their own.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void ReviveTo(float HealthAmount, float TimeSeconds);

	/** Called by the director once a hit has landed, to restart the regeneration delay. */
	void NotifyHealthDamaged();

	// ---------------------------------------------------------------------------------------
	// Carry
	// ---------------------------------------------------------------------------------------

	/** True while this pawn is holding another actor. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	bool IsCarrying() const { return CarriedActor != nullptr; }

	/** Climbing down off a ledge or hanging from one: the crouch key belonged to the climb this time. */
	bool IsBusyTraversing() const { return bClimbDownMoving || IsParkourHanging(); }

	UL2H_CoverComponent* GetCoverComponent() const { return CoverComponent; }

	/** What this pawn is holding, or null. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	AActor* GetCarriedActor() const { return CarriedActor; }

	/**
	 * The ability the fire input runs while something is carried. Read through a getter rather than made
	 * public so the fire ability does not need to know how the pawn stores its kit.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	TSubclassOf<UGameplayAbility> GetThrowAbilityClass() const { return ThrowAbilityClass; }

	/**
	 * Server-authoritative. Stops the target's physics, attaches it to SocketName and slows this
	 * pawn by SpeedScale.
	 *
	 * Attachment is a transform relationship, not a replicated property, so every machine redoes it
	 * in OnRep_CarriedActor. That is the whole reason carry uses AttachToComponent rather than a
	 * physics handle: a handle would have to be simulated identically on every machine to look right,
	 * and it will not be.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Carry")
	bool BeginCarry(AActor* Target, float SpeedScale, FName SocketName);

	/**
	 * Server-authoritative. Detaches, restores the target's physics and releases the speed scale.
	 * bPlayPutDown false skips the put-down clip -- a throw has its own motion.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Carry")
	void EndCarry(bool bPlayPutDown = true);

	/**
	 * Movement actions (roll, climb, wall run, bar, ladder, vault) are done with empty hands. The
	 * Blueprint calls this every frame with whether one of them is running; while one is, whatever is
	 * held is let go where it is. Safe on any machine: a client asks the server once per carry.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Carry")
	void DropCarryForAction(bool bActionRunning);

	// ---------------------------------------------------------------------------------------
	// Carry animation. The hold is an upper-body layer in the AnimBP that reads GetCarryPose /
	// GetCarryLayerWeight; the legs stay on the movement graph. Pick-up and put-down are one-shot
	// slot clips. A carried partner rides piggyback on its own single-node clip, which bypasses
	// the AnimBP's Offset Root Bone (that node rights any rotated body).
	// ---------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------
	// Throw. One formula for the throw and for its preview, so the dotted arc the player sees is
	// exactly where the object goes.
	// ---------------------------------------------------------------------------------------

	/** Where the carried object leaves from and the velocity it leaves with. False when nothing throwable is held. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	bool GetThrowLaunch(FVector& OutStart, FVector& OutVelocity) const;

	/** Release speed for something weightless, and for the heaviest thing one player may lift. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Throw", meta = (ClampMin = "50.0"))
	float ThrowLightSpeed = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Throw", meta = (ClampMin = "50.0"))
	float ThrowHeavySpeed = 650.0f;

	/** Added to the camera direction before normalising, so a level aim still arcs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Throw", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ThrowUpwardBias = 0.3f;

	// ---------------------------------------------------------------------------------------
	// Carry posture. The upper body turns toward the camera while holding something, and the hands
	// close on the object's real sides. The AnimBP reads these; the legs stay on the movement graph,
	// so running and jumping with a load work as usual.
	// ---------------------------------------------------------------------------------------

	/** Yaw (degrees) the spine turns toward the camera while carrying. 0 when not carrying. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	float GetCarryTwist() const { return CarryTwist; }

	/** Spine yaw (degrees) that turns the held gun onto the aim point while aiming; 0 otherwise. */
	float GetAimTwistCorrection() const;

	/** Most the torso turns to line the barrel up with the aim point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	float AimMaxTwist = 35.0f;

	/** Most the aim sweep's pitch is nudged to line the barrel up with the aim point vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	float AimMaxPitchCorrection = 20.0f;

	/** Base aim plus the closed-loop pitch nudge while aiming, so the aim sweep lays the barrel on the shot line. */
	virtual FRotator GetBaseAimRotation() const override;

	/** 0..1 weight for the hand IK on the carried object. 0 for a partner and before the grab. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	float GetCarryIKAlpha() const { return CarryIKAlpha; }

	/**
	 * Point the left / right hand reaches for, in the MESH COMPONENT's space (the AnimBP's IK reads it as
	 * component space). Kept relative to the body so the hands move with it in the same frame; a world
	 * target lagged one movement step behind the walking body and shook the arms.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	FVector GetCarryHandTarget(bool bLeft) const { return bLeft ? CarryHandTargetL : CarryHandTargetR; }

	/** Component-space rotation of hand_l / hand_r: open palm toward the load, fingers up. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	FRotator GetCarryHandRotation(bool bLeft) const { return bLeft ? CarryHandRotL : CarryHandRotR; }

	/** Component-space point each elbow bends toward, so the arm never folds the wrong way. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	FVector GetCarryElbowTarget(bool bLeft) const { return bLeft ? CarryElbowL : CarryElbowR; }

	/**
	 * Starts DPTS's in-air searches (ledge catch and wall run) for this fall. DPTS only starts them
	 * itself when the character walks off a ledge, so a hook launch or a jump pressed in the air never
	 * searched and nothing was ever caught. Safe to call on the ground: it does nothing there.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hook")
	void StartAirParkourSearch();

	// ---------------------------------------------------------------------------------------
	// Hanging: tap jump = the normal climb jump (Blueprint), hold jump = a free jump toward the stick.
	// ---------------------------------------------------------------------------------------

	/** The jump button went down while hanging. Starts the hold timer. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hang")
	void BeginHangJumpPress();

	/** The jump button came up. True when it was a tap (the Blueprint then runs the climb jump). */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hang")
	bool EndHangJumpPress();

	/** Let go of the ledge and jump toward the stick: up, left, right or down. Owner only; mirrored on the server. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hang")
	void HangDirectionalJump();

	/** How long the jump button must be held while hanging before it becomes a free jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hang", meta = (ClampMin = "0.05"))
	float HangHoldTime = 0.3f;

	/** Free-jump strength, matched to the back leap (385 out, 335 up). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hang")
	float HangJumpSide = 385.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hang")
	float HangJumpUp = 335.0f;

	/** Added to HangJumpUp when the stick points up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hang")
	float HangJumpUpExtra = 265.0f;

	/** Push away from the wall so the body clears it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hang")
	float HangJumpOut = 80.0f;

	/**
	 * Standing near an edge: hop down and catch it. Finds the edge in front, eases the body over it (turned
	 * to face the wall) and lets it fall with DPTS's in-air search running, so the catch is DPTS's own and
	 * the hang afterwards is a normal one (climb up, shimmy and jump all work). False when there is no edge.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hang")
	bool TryClimbDownToLedge();

	/** Told by the server when the interact press found nothing to carry; the Blueprint may then climb down a ledge. */
	UFUNCTION(Client, Reliable)
	void ClientInteractFoundNothing();

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Interact")
	void OnInteractFoundNothing();

	/** How far the hands stay off the load; the gap is bridged by the carry beams. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0"))
	float CarryHandGap = 14.0f;

	/** How far the upper body may turn from the hips while carrying. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float CarryMaxTwist = 60.0f;

	/**
	 * True while a standing carrier's camera is past the upper body's limit. The Blueprint's rotation
	 * update treats it like aiming (strafe rotation), so the whole body turns to the camera with GASP's
	 * turn-in-place steps, then lets go once it has caught up.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	bool WantsCarryTurn() const { return bCarryTurning; }

	/** 0 nothing, 1 a box one player lifts, 2 a heavy (two-player) load, 3 a partner on the back. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	int32 GetCarryPose() const;

	/** 0..1, eased toward 1 while GetCarryPose() is not 0. The AnimBP's carry layer weight. */
	UFUNCTION(BlueprintPure, Category = "L2H|Carry")
	float GetCarryLayerWeight() const { return CarryLayerWeight; }

	/** 0..1 weight of the injured upper-body pose (GASPALS): eased in at or below InjuredHealthFraction. */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	float GetInjuredAlpha() const { return InjuredAlpha; }

	/** Health fraction at or below which the body hunches over (a revive's 50% counts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health")
	float InjuredHealthFraction = 0.5f;

	/** Strongest the injured pose gets over the locomotion's upper body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health")
	float InjuredMaxAlpha = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPickupLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPutDownLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPickupHeavy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPutDownHeavy;

	/** The carrier lifting a partner onto the back, and setting them down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPersonStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> CarryPersonEnd;

	/** The partner on the back, still and moving. Authored around the carrier's root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> RiderIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim")
	TObjectPtr<UAnimSequenceBase> RiderMove;

	/**
	 * Standing up when a revive ends the ragdoll (GASPALS get-up clips). Back = the body lay face up,
	 * Front = face down. Both start lying with the pelvis over the root, so the capsule is put there.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Revive|Anim")
	TObjectPtr<UAnimMontage> GetUpBackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Revive|Anim")
	TObjectPtr<UAnimMontage> GetUpFrontMontage;

	// ---------------------------------------------------------------------------------------
	// Shards
	// ---------------------------------------------------------------------------------------

	/** How many shards this pawn is currently absorbing. Capped by the level's MaxCarriedShards. */
	UFUNCTION(BlueprintPure, Category = "L2H|Shards")
	int32 GetCarriedShardCount() const { return CarriedShardCount; }

	/** True while the pawn still has room for another shard under the given cap. */
	UFUNCTION(BlueprintPure, Category = "L2H|Shards")
	bool HasShardRoom(int32 MaxShards) const { return CarriedShardCount < MaxShards; }

	/**
	 * Server-authoritative. Counts the shard and opens the absorb window.
	 *
	 * The window is the whole reason a cap of one is a rule rather than a formality: a pawn that is
	 * still taking one shard in cannot take a second, so a pile of shards has to be walked off one at
	 * a time instead of being hoovered up in a single pass.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Shards")
	void BeginShardAbsorb(float WindowSeconds);

	// ---------------------------------------------------------------------------------------
	// Ability activation
	// ---------------------------------------------------------------------------------------

	/**
	 * Ability activation has to be decided on the server, and the base abilities are ServerOnly, so a
	 * client cannot start them from its own component. This routes the request instead, which makes
	 * the client path identical to the one the host already takes.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Abilities")
	void ServerTryActivateAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass);

	/** The same route for an ability identified by tag rather than by class. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Abilities")
	void ServerTryActivateAbilityByTag(FGameplayTag AbilityTag);

	// ---------------------------------------------------------------------------------------
	// Checkpoint terminals
	// ---------------------------------------------------------------------------------------

	/** The terminal this pawn is standing in, or null. Set on overlap, never searched for. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	AL2H_CheckpointTerminal* GetCurrentTerminal() const { return CurrentTerminal; }

	/** Called by AL2H_CheckpointTerminal on entry and on exit. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Checkpoint")
	void SetCurrentTerminal(AL2H_CheckpointTerminal* InTerminal);

	/** Names the place this player just walked into, on their own screen (no-op elsewhere). */
	UFUNCTION(BlueprintCallable, Category = "L2H|UI")
	void ShowAreaName(const FText& Name);

	/** Opens the terminal menu on this player's own screen. False when there is no hub to open. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Checkpoint")
	bool TryOpenTerminalMenu();

	/** Tears the menu down and hands input back to the game. Safe when nothing is open. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Checkpoint")
	void CloseTerminalMenu();

	/** True while this pawn's menu is up. */
	UFUNCTION(BlueprintPure, Category = "L2H|Checkpoint")
	bool IsTerminalMenuOpen() const { return TerminalMenu != nullptr; }

	/** Spawned per local player the first time a hub's menu opens. Set on the Blueprint child. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Checkpoint")
	TSubclassOf<UL2H_TimeMenuWidget> TerminalMenuClass;

	/** Spawned per local player at BeginPlay; shows a partner's request at the screen edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Checkpoint")
	TSubclassOf<UL2H_TimeRequestPromptWidget> RequestPromptClass;

	// ---------------------------------------------------------------------------------------
	// Time request
	// ---------------------------------------------------------------------------------------

	/** The tag the menu's transfer row fires, so it and the dedicated key cannot drift apart. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	FGameplayTag GetTransferAbilityTag() const;

	/** Asks the partner to hand over a transfer's worth of seconds. Server-authoritative. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Time")
	void ServerRequestTimeFromPartner();

	/** Answers the request posted on this pawn. Server-authoritative. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Time")
	void ServerRespondToTimeRequest(bool bAccept);

	/**
	 * Re-records the times at the checkpoint this pawn is standing in, paid out of its own seconds.
	 * Server-authoritative.
	 *
	 * This is the only thing that may replace a first arrival. It is a choice rather than a
	 * convenience: the recorded number is what the players are handed back on death, so overwriting it
	 * has to be asked for, and it costs -- otherwise a run could keep re-recording a falling time and
	 * slowly rewrite its own checkpoint downwards.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Time")
	void ServerRequestOptionalSave();

	/**
	 * Rewinds the whole run to an earlier checkpoint. Server-authoritative.
	 *
	 * The destination must be one the run has already stood in, and everything after it is forgotten:
	 * travel is backwards-only, so naming an earlier place is the same act as giving up everything
	 * that came after it.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Checkpoint")
	void ServerTravelToCheckpoint(FName CheckpointId);

	/** Replaces this pawn's time with the level's designer time, if that is an improvement. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Checkpoint")
	void ServerAdoptDesignerTime();

	/** The live request this pawn has been asked to answer. */
	UPROPERTY(ReplicatedUsing = OnRep_PendingRequest, BlueprintReadOnly, Category = "L2H|Request")
	FL2H_TimeRequest PendingRequest;

	UFUNCTION()
	void OnRep_PendingRequest();

	/** Fires on server and clients, so the local screen can show or hide its prompt. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Request")
	void OnTimeRequestChanged(bool bActive);

	/** Server side, used by the director. */
	void SetPendingRequest(const FL2H_TimeRequest& InRequest);
	void ClearPendingRequest();

	// ---------------------------------------------------------------------------------------
	// Movement speed -- cache -> change -> restore. Never driven by a GameplayEffect.
	// ---------------------------------------------------------------------------------------

	/** Hold-to-sprint. Composes with the carry scale instead of fighting it. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Movement")
	void SetSprinting(bool bNewSprinting);

	/** Multiplies the designer-authored walk speed. Used by carry, e.g. 0.6 for a heavy object. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Movement")
	void SetCarrySpeedScale(float NewScale);

	/**
	 * Multiplies the designer-authored walk speed while aiming down sights.
	 *
	 * Composed with sprint and carry rather than written over them: aiming while sprinting is a real
	 * thing a player does, and having it cancel the sprint would leave the two scales fighting over
	 * MaxWalkSpeed instead of stacking.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Movement")
	void SetAimSpeedScale(float NewScale);

	/** Narrows the follow camera while aiming. Pass 0 to restore the FOV the designer authored. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Camera")
	void SetAimFieldOfView(float NewFieldOfView);

	// ---------------------------------------------------------------------------------------
	// Aiming and shot feedback, shared by the movement character and the test pawn.
	// ---------------------------------------------------------------------------------------

	/**
	 * Where the player is really looking from: the camera when a player controls this pawn, the eyes
	 * otherwise. On the server a remote player's camera arrives through ServerUpdateCamera, so a shot
	 * traced from here lands under the crosshair instead of parallel to it.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Camera")
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Hold-to-aim from the movement layer: the owning client asks, the server sets the weapon's aim flag. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Weapon")
	void ServerSetWeaponAiming(bool bNewAiming);

	/**
	 * A shot or a hook line, drawn on every machine. Reliable although cosmetic: a hook press fires one
	 * line, and a dropped one reads to the partner as the hook doing nothing. The fire rate is low enough.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShotFX(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHook);

	// ---------------------------------------------------------------------------------------
	// Camera feedback. Shakes play on this player's own camera only; the server reaches a remote
	// player's camera through the controller's client RPC.
	// ---------------------------------------------------------------------------------------

	/** Plays Shake on this player's camera, from the server or from the owning machine. */
	void PlayCameraShakeOnOwner(TSubclassOf<UCameraShakeBase> Shake, float Scale = 1.0f);

	/** On every hit that costs Health, scaled by the damage (HitShakeDamage = scale 1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Feedback")
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Feedback", meta = (ClampMin = "1.0"))
	float HitShakeDamage = 25.0f;

	/** On a landing faster than HardLandingSpeed (cm/s, downward), scaled up to twice by the speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Feedback")
	TSubclassOf<UCameraShakeBase> LandCameraShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Feedback", meta = (ClampMin = "0.0"))
	float HardLandingSpeed = 750.0f;

	/** The hook's reel, mirrored on the owning client so its prediction moves with the server. */
	UFUNCTION(Client, Reliable)
	void ClientHookLaunch(FVector_NetQuantize LaunchVelocity);

	/** The Blueprint draws the line (tracer for bullets, laser for the hook). */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Weapon")
	void OnShotFX(FVector Start, FVector End, bool bHook);

	/**
	 * The aiming laser, refreshed every frame the sights are up. Start is the muzzle when a weapon is out,
	 * End is whatever the crosshair is on. bActive false arrives exactly once, on the frame the laser is
	 * put away, so the Blueprint can hide the beam and then be left alone.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Weapon")
	void OnAimBeamUpdate(FVector Start, FVector End, bool bActive);

	/** Traces the aim line and pushes it to the Blueprint. Owning machine only. */
	void UpdateAimBeam();

	/** Mirrors the weapon's aim flag onto the aim speed and FOV halves, so both come off with it. */
	void UpdateAimState();

	// ---------------------------------------------------------------------------------------
	// Hook travel. The way a traversal system moves a body to a target: the landing point is chosen
	// first -- just off a wall, or standing on a ledge -- and the body is launched on the ballistic
	// arc that reaches it, then stopped there. Gravity shapes the flight; nothing keeps pulling after
	// arrival, which is what used to fling the player past the point.
	// ---------------------------------------------------------------------------------------

	/**
	 * Server only. Throws the pawn on an arc to the point the hook caught. SurfaceNormal is the normal of
	 * what was hit; zero means "unknown" and the landing is taken just short of the point.
	 * HitActor tagged `climb` (DPTS geometry) changes the landing: the body arrives facing it with the capsule
	 * just above its top edge, where the DPTS in-air search catches the ledge.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Hook")
	void StartHookSwing(FVector Anchor, FVector SurfaceNormal = FVector::ZeroVector, AActor* HitActor = nullptr);

	/** Lets go of the rope. Callable on the owning client; it forwards to the server. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Hook")
	void ReleaseHookSwing();

	UFUNCTION(Server, Reliable)
	void ServerReleaseHookSwing();

	/** True while the hook is carrying the player to its point (the rope is drawn for exactly this). */
	UFUNCTION(BlueprintPure, Category = "L2H|Hook")
	bool IsHookSwinging() const { return bHookSwinging; }

	/** Kept for Blueprints that asked for it; the same window as IsHookSwinging now. */
	UFUNCTION(BlueprintPure, Category = "L2H|Hook")
	bool IsHookBeamHeld() const { return bHookSwinging; }

	/** 0 = a flat, fast line; 1 = a high lob. The arc the travel takes to the landing point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float HookArc = 0.4f;

	/** Within this of the landing point the travel is over and the body is stopped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "10.0"))
	float HookArriveRadius = 40.0f;

	/** How far off a wall the body lands, so it arrives in front of the surface, not inside it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float HookWallStandOff = 45.0f;

	/** Share of the flight speed kept on arrival. Small, so the player stops at the point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HookArrivalKeep = 0.1f;

	/** Longest flight before the travel gives up, so a blocked path cannot pin a player mid-air. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.3"))
	float HookMaxFlightTime = 3.0f;

	/**
	 * Hooking climbable geometry: the capsule centre arrives this far above the top edge. The DPTS catch only
	 * finds a ledge it faces with the centre a little above the edge (caught at +13 cm, missed at -42 cm).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook")
	float HookLedgeAboveEdge = 13.0f;

	/** Seconds after arriving at a ledge during which the catch is asked for again every frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float HookLedgeCatchWindow = 0.5f;

	/** Unused since the travel became an arc; kept so Blueprint overrides still load. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Hook")
	TSoftObjectPtr<UNiagaraSystem> HookRopeFX;


	// ---------------------------------------------------------------------------------------
	// Carried body. A downed or dead partner is carried seated on the carrier's forearms: the
	// ragdoll is switched off while carried and comes back when the body is put down.
	// ---------------------------------------------------------------------------------------

	/**
	 * Where a carried partner's capsule sits, relative to the carrier's. The rider clip seats the body on
	 * the carrier's forearms, in front: measured from the pack, the carrier's hands end the lift 24 cm ahead
	 * at 115 cm and the rider's pelvis sits at 114 cm above its own root, so the rider's root goes ~28 cm
	 * forward. At zero the two bodies occupied the same space.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry")
	FVector CarriedBodyOffset = FVector(28.0f, 0.0f, 0.0f);

	/** Extra rotation of a carried partner's capsule on the carrier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry")
	FRotator CarriedBodyRotation = FRotator::ZeroRotator;

	/**
	 * True while the laser is being drawn, so hiding it happens once rather than every frame. Starts
	 * true so the first frame sends that one hide: the beam component is visible as authored.
	 */
	UPROPERTY(Transient)
	bool bAimBeamActive = true;

	/** True while the aim speed and FOV halves are on, so lowering them happens once, not every frame. */
	UPROPERTY(Transient)
	bool bAimHalvesApplied = false;

	/**
	 * How far the aiming laser reaches. Cosmetic only -- the shot's own range still comes from the
	 * weapon's data asset -- so this is set long enough to read as a laser rather than as a stub.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "100.0"))
	float AimBeamRange = 8000.0f;

	/** Restores the exact MaxWalkSpeed the designer authored and releases all three overrides. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Movement")
	void RestoreWalkSpeed();

	UFUNCTION(BlueprintPure, Category = "L2H|Movement")
	float GetAuthoredWalkSpeed() const { return bWalkSpeedCached ? CachedWalkSpeed : 0.0f; }

	// ---------------------------------------------------------------------------------------
	// Test commands (console). Routed through the level's L2H_TimeDirector, so they obey the
	// same effect-only rule as gameplay. PlayerIndex = local player index (0 = left screen).
	// ---------------------------------------------------------------------------------------

	UFUNCTION(Exec)
	void L2HTimeDamage(int32 PlayerIndex, float Amount);

	/** Console test: takes HEALTH off a local player, which is the path a weapon now uses. */
	UFUNCTION(Exec)
	void L2HHealthDamage(int32 PlayerIndex, float Amount);

	/** Console test: gives HEALTH back, without touching the clock. */
	UFUNCTION(Exec)
	void L2HHealthRestore(int32 PlayerIndex, float Amount);

	UFUNCTION(Exec)
	void L2HTimeRestore(int32 PlayerIndex, float Amount);

	UFUNCTION(Exec)
	void L2HTimeReset();

	UFUNCTION(Exec)
	void L2HTimeSave(int32 PlayerIndex);

	/** Logs the run's checkpoint stack, marking which entries a rewind may name from where you stand. */
	UFUNCTION(Exec)
	void L2HTimeCheckpoints();

	/** Rewinds to the checkpoint at this index of the run, erasing everything after it. */
	UFUNCTION(Exec)
	void L2HTimeTravel(int32 Index);

	/** Runs the designer-time offer for one local player, as the menu button would. */
	UFUNCTION(Exec)
	void L2HTimeDesigner(int32 PlayerIndex);

protected:
	// ---------------------------------------------------------------------------------------
	// Components
	// ---------------------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// ---------------------------------------------------------------------------------------
	// Input (pattern B). Assets are assigned on the Blueprint child.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** IMC_L2H_Debug: development keys, added on top of the default in non-shipping builds only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputMappingContext> DebugMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Carry: press to pick up, press again to put down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> InteractAction;

	/** The crouch key (L3 / C). The Blueprint crouches with it; C++ listens too, for cover. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> CoverAction;

	void Input_Cover(const FInputActionValue& Value);

	/** Circle / Left Alt in cover (IA_L2H_CoverCross, from IMC_L2H_Cover): crosses to the next cover. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> CoverJumpAction;

	void Input_CoverJump(const FInputActionValue& Value);

	/** Hook: drag a light object to you, or reel yourself to a heavy one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> HookAction;

	/** Opens the checkpoint terminal's menu. Only a hub offers one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> MenuAction;

	/** Answers a pending time request: accept, and the transfer goes through. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> RequestAcceptAction;

	/** Answers a pending time request: decline, and nothing moves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> RequestDeclineAction;

	/**
	 * The character abilities' input, as data. One row per super ability: the action that fires it and
	 * the tag of the ability it fires. Assigned per character on the Blueprint child, so Yousef and
	 * Samer share this class and differ only in the rows they hold.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TArray<FL2H_AbilityInputBinding> AbilityInputBindings;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted(const FInputActionValue& Value);
	void Input_JumpCompleted(const FInputActionValue& Value);
	void Input_SprintStarted(const FInputActionValue& Value);
	void Input_SprintCompleted(const FInputActionValue& Value);
	void Input_Interact(const FInputActionValue& Value);
	void Input_Hook(const FInputActionValue& Value);

	void Input_Menu(const FInputActionValue& Value);

	// ---------------------------------------------------------------------------------------
	// On the floor. After SpectateDelay the view moves to the partner and orbits them with this
	// player's own stick; Interact (Triangle / E) swaps back to the body and again. D-pad Up (T) pings
	// where the view looks, away from a terminal, standing or down; the pole shows on both screens.
	// ---------------------------------------------------------------------------------------

	/** Owning machine only, from Tick: moves the camera rig to the partner and back. */
	void UpdateDownView();

	/** The nearest other player, the one the downed view follows. */
	AL2H_BaseCharacter* FindPartner() const;

	/** Owning machine: traces the view and asks the server for a ping. */
	void TryPing();

	UFUNCTION(Server, Reliable)
	void ServerPing(FVector_NetQuantize Location, AActor* HitActor);

	/** Set by Interact while down: look at the body instead of the partner. */
	bool bDownViewOwnBody = false;

	/** Local time this player was first seen on the floor; -1 while standing. */
	float DownViewSince = -1.0f;

	/** The camera rig (GASP's GameplayCamera, else the boom) while it is lent to the partner. */
	TWeakObjectPtr<USceneComponent> DownViewRig;
	FVector DownViewRigRelative = FVector::ZeroVector;

	float LastPingTime = -1000.0f;

	/** Server side: one ping per player, the previous goes when a new one is placed. */
	TWeakObjectPtr<AActor> ActivePing;
	float ServerLastPingTime = -1000.0f;

	// ---------------------------------------------------------------------------------------
	// Time quick menu (D-pad Left / X): give time, ask for time, answer the partner's request.
	// While it is open IMC_L2H_TimeMenu sits on top: D-pad Up/Down move, X or Triangle confirm,
	// Circle or D-pad Left close. Those keys are consumed, so X does not also jump.
	// ---------------------------------------------------------------------------------------

	void Input_TimeQuickMenu(const FInputActionValue& Value);

	/** Opens or closes the menu. Owning machine only. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void ToggleTimeQuickMenu();

	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	bool IsTimeQuickMenuOpen() const { return TimeQuickMenu != nullptr; }

	void MoveTimeQuickMenu(int32 Direction);
	void ConfirmTimeQuickMenu();
	void RefreshTimeQuickMenu();

	enum class ETimeQuickOption : uint8 { Give, Request, Accept, Decline, CarryMore, CarryLess, ReviveHere, Buy, Use, Close };

	/** Per row of GetTimeQuickOptions: the shop index (Buy) or bag cell (Use), -1 otherwise. */
	mutable TArray<int32> TimeQuickArgs;

	UFUNCTION(Server, Reliable)
	void ServerBuyShopItem(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerBuyPartnerRevive();

	/** The rows as they stand right now: Accept and Decline only appear while a request is pending. */
	TArray<ETimeQuickOption> GetTimeQuickOptions() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputAction> TimeQuickMenuAction;

	/**
	 * Added above everything while the menu is open, so X / Triangle confirm and Circle closes instead of
	 * jumping or dodging. Its keys are consumed for as long as it is on the stack.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Input")
	TObjectPtr<UInputMappingContext> TimeQuickMenuContext;

	/** Defaults to the code-built widget; a styled Widget Blueprint child can replace it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Menu")
	TSubclassOf<UL2H_TimeQuickMenuWidget> TimeQuickMenuClass;

	UPROPERTY(Transient)
	TObjectPtr<UL2H_TimeQuickMenuWidget> TimeQuickMenu;

	int32 TimeQuickSelection = 0;
	FText TimeQuickStatus;
	void Input_RequestAccept(const FInputActionValue& Value);
	void Input_RequestDecline(const FInputActionValue& Value);

	/**
	 * Fired by one AbilityInputBindings row, with that row's tag handed over as a binding payload.
	 * The value is unused -- the interesting part is which tag arrived.
	 */
	void Input_AbilityByTag(const FInputActionValue& Value, FGameplayTag AbilityTag);

	/** Held input: repeats the fire request for an automatic weapon at its FireRate. */
	void Input_AbilityHeld(const FInputActionValue& Value, FGameplayTag AbilityTag);
	float LastAutoFireRequest = -1000.0f;

	// ---------------------------------------------------------------------------------------
	// Replicated state
	// ---------------------------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_bDowned, BlueprintReadOnly, Category = "L2H|State")
	bool bDowned = false;

	UPROPERTY(ReplicatedUsing = OnRep_bBeingCarried, BlueprintReadOnly, Category = "L2H|State")
	bool bBeingCarried = false;

	/** Replicated so the HUD can tell "waiting to be revived" apart from "waiting for a restart". */
	UPROPERTY(ReplicatedUsing = OnRep_bDead, BlueprintReadOnly, Category = "L2H|Health")
	bool bDead = false;

	/** Server world time (GameState clock) at which the free revive window closes; 0 while none is open. */
	UPROPERTY(Replicated)
	float FreeReviveDeadline = 0.0f;

	UPROPERTY(Replicated)
	float Chrons = 0.0f;

	UPROPERTY(Replicated)
	bool bHubBankOpen = false;

	UPROPERTY(Replicated)
	float HubCarryAllowance = 0.0f;

	UPROPERTY(Replicated)
	float HubCarried = 0.0f;

	UPROPERTY(Replicated)
	FName LastHubId;

	UPROPERTY(Replicated)
	int32 ShieldHits = 0;

	/** Server only, oldest first, about RewindHistorySeconds long. */
	TArray<FRewindSample> RewindHistory;
	float NextRewindSampleTime = 0.0f;
	void RecordRewindSample();

	// The free revive hold, local to the player holding Interact.
	TWeakObjectPtr<AL2H_BaseCharacter> ReviveHoldTarget;
	float ReviveHoldStart = -1.0f;

	/** The partner whose free revive window is open and who is in range, or null. */
	AL2H_BaseCharacter* FindFreeReviveTarget() const;

	/** Other can be stood up by a hold from here: inside the free window, or with an injector or elixir in the bag. */
	bool CanHoldReviveOn(const AL2H_BaseCharacter* Other) const;
	void UpdateReviveHold();
	void Input_InteractReleased(const FInputActionValue& Value);

	UFUNCTION(Server, Reliable)
	void ServerFreeRevive(AL2H_BaseCharacter* Target);

	/** Tells the director whether this pawn's clock should be stopped for being down. */
	void SyncDownDrainPause();

	UFUNCTION()
	void OnRep_bDowned();

	UFUNCTION()
	void OnRep_bBeingCarried();

	UFUNCTION()
	void OnRep_bDead();

	/** Body state for being carried: the rider clip on, capsule collision off; undone on put-down. */
	void ApplyCarriedPose(bool bCarried);

public:
	/**
	 * Server: plays an interaction clip (pick up, push a door, a swing) on every machine, through DefaultSlot.
	 * A non-zero FaceLocation turns the body toward it first. StopAt > 0 cuts the clip there (a clip with a long
	 * tail). Movement input is ignored for LockSeconds (< 0: until the clip ends), so the body does not slide
	 * across the floor while the clip plays.
	 */
	void PlayInteractAnim(UAnimSequenceBase* Anim, FVector FaceLocation = FVector::ZeroVector, float StopAt = 0.0f, float LockSeconds = -1.0f);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastInteractAnim(UAnimSequenceBase* Anim, float FaceYaw, bool bFace, float StopAt, float LockSeconds);

	/** The pick-up clip for an item lying HeightAboveFeet cm above the feet, when the hand reaches it, and where the clip is cut. */
	UAnimSequenceBase* GetPickupAnim(float HeightAboveFeet, float& OutContactTime, float& OutStopAt) const;

	/** Pick-ups (delivery pack, UEFN): one hand to the floor, or to table height (PickupTableHeight and up). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TObjectPtr<UAnimSequenceBase> PickupFloorAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TObjectPtr<UAnimSequenceBase> PickupTableAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float PickupTableHeight = 50.0f;

	/** Seconds into each clip at which the hand reaches the item (it disappears then), and where the floor clip is cut (it goes on to put the box back down). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float PickupFloorContact = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float PickupFloorStopAt = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float PickupTableContact = 0.6f;

	/** Seconds into the door push at which the hand meets the door, and the door starts to swing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float DoorPushContactTime = 0.4f;

	/** Door push clips by the side the door is on (ResidentHorrorV2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TObjectPtr<UAnimSequenceBase> DoorPushLeftAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TObjectPtr<UAnimSequenceBase> DoorPushRightAnim;

	/** The finisher on a stunned enemy (Interact within FinisherRange, in front): a kick, and the enemy goes down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TObjectPtr<UAnimSequenceBase> FinisherAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "50.0"))
	float FinisherRange = 230.0f;

	/** Seconds into the finisher clip at which the enemy is struck. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.0"))
	float FinisherImpactTime = 0.9f;

	/** The finisher clip is a paired move with a long tail; it is cut here (seconds), blending back out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "0.1"))
	float FinisherEndTime = 2.6f;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStopInteractAnim(float BlendOut);

	/** Local time until which movement input is ignored (an interaction clip is playing). */
	float ActionLockUntil = -1.0f;
	FTimerHandle InteractStopTimer;

	/** A stunned enemy within reach and in front, or null. */
	class AL2H_EnemyRobot* FindFinisherTarget() const;

	UFUNCTION(Server, Reliable)
	void ServerFinisher(class AL2H_EnemyRobot* Target);

protected:

	/** Plays a one-shot carry clip on every machine (the AnimBP's DefaultSlot). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCarryAnim(UAnimSequenceBase* Anim, bool bPickup, FVector_NetQuantize RestLocation);

	/** Lets the body move again once a pick-up or put-down clip has done its work. */
	void EndCarryActionLock();

	/** Fraction of a pick-up clip at which the hands reach the object. Before it, the object stays put. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry|Anim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CarryGrabFraction = 0.45f;

	/** Local per machine: when the current pick-up started, when the grab lands, and where the object rested. */
	float CarryActionStartTime = -1.0f;
	float CarryGrabDelay = 0.0f;
	FVector CarryRestLocation = FVector::ZeroVector;
	FTimerHandle CarryActionLockTimer;

	/** Per frame: the layer weight, the rider clip, and a held object between the hands. */
	void UpdateCarryAnimation(float DeltaSeconds);

	float CarryLayerWeight = 0.0f;
	float CarryTwist = 0.0f;

	float InjuredAlpha = 0.0f;

	/** Degrees added to the base aim pitch while aiming (see GetBaseAimRotation). Eased toward its target in Tick. */
	float AimPitchCorrection = 0.0f;

	/**
	 * Measures the held gun against the aim point: the barrel axis (Barrel socket, else Grip, to the fire
	 * socket) and the line from the muzzle to what the camera's centre hits. False when not aiming.
	 */
	bool MeasureBarrelError(float& OutYawError, float& OutPitchError) const;
	float CarryIKAlpha = 0.0f;
	FVector CarryHandTargetL = FVector::ZeroVector;
	FVector CarryHandTargetR = FVector::ZeroVector;

	FRotator CarryHandRotL = FRotator::ZeroRotator;
	FRotator CarryHandRotR = FRotator::ZeroRotator;
	FVector CarryElbowL = FVector::ZeroVector;
	FVector CarryElbowR = FVector::ZeroVector;

	/** Finger direction in mesh space: the beams leave from the palm, a hand's length up from the wrist bone. */
	FVector CarryFingerDir = FVector::UpVector;

	/** Where each beam lands on the load, in mesh component space. */
	FVector CarryGripL = FVector::ZeroVector;
	FVector CarryGripR = FVector::ZeroVector;

	/** Wobbling laser from each hand to the load: short glowing segments on the mesh, so they move with it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Markers")
	TObjectPtr<UInstancedStaticMeshComponent> CarryBeams;

	void UpdateCarryBeams(bool bShow);

	/** Facing (actor yaw + twist) last frame, so the held object turns with the upper body. */
	float LastCarryFacingYaw = 0.0f;
	bool bCarryFacingValid = false;

	// Climb-down move: a short slide to the montage's start spot facing out, then the root-motion climb-down
	// montage carries the body over the edge, then it is released while the head is still above the edge so
	// the DPTS catch search finds the ledge.
	bool bClimbDownMoving = false;
	bool bClimbDownMontage = false;
	float ClimbDownAlpha = 0.0f;
	float ClimbDownTime = 0.0f;
	FVector ClimbDownFrom = FVector::ZeroVector;
	FVector ClimbDownTo = FVector::ZeroVector;
	float ClimbDownFromYaw = 0.0f;
	float ClimbDownToYaw = 0.0f;

	void StartClimbDownMove(const FVector& From, const FVector& To, float FromYaw, float ToYaw);
	void UpdateClimbDownMove(float DeltaSeconds);
	void FinishClimbDownMove();

	UFUNCTION(Server, Reliable)
	void ServerClimbDownToLedge(FVector_NetQuantize From, FVector_NetQuantize To, float FromYaw, float ToYaw);

	bool bHangJumpPending = false;
	float HangJumpPressTime = 0.0f;

	UFUNCTION(Server, Reliable)
	void ServerHangDirectionalJump(FVector_NetQuantize Velocity);

	/** Disengage from the ledge, launch, and let the in-air search catch whatever is on the way. */
	void ApplyHangDirectionalJump(const FVector& Velocity);

	/** The DPTS component (a Blueprint class, found by name), and whether it is in a hanging state. */
	UActorComponent* FindParkourComponent() const;
	bool IsParkourHanging() const;

	bool bCarryTurning = false;
	float LastCarryBodyYaw = 0.0f;
	bool bCarryBodyYawValid = false;

	/** A client asked the server to drop for an action; cleared when the carry changes. */
	bool bCarryDropRequested = false;

	UFUNCTION(Server, Reliable)
	void ServerDropCarryForAction();

	// ---------------------------------------------------------------------------------------
	// Local markers: the laser dot, the throw arc, the hook rope and the grabbed-point marker.
	// The dot and the arc are drawn only for their own player; the rope and its marker for everyone.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Markers")
	TObjectPtr<UStaticMeshComponent> AimDot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Markers")
	TObjectPtr<UInstancedStaticMeshComponent> ThrowArc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Markers")
	TObjectPtr<UStaticMeshComponent> HookRope;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Markers")
	TObjectPtr<UStaticMeshComponent> HookMarker;

	/** Draws the dotted throw path and its landing ring while a throwable object is held. Owner only. */
	void UpdateThrowPreview();

	/** Stretches the rope mesh from the hand to the anchor, and parks the marker on the anchor. */
	void UpdateHookVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentRiderAnim;

	/** One attach rule for the server and every OnRep: a partner rides the capsule, an object the socket. */
	void AttachCarriedActor(AActor* Target);

	/** The travel on the server and the owning client: arrival, give-up and the stop. */
	void UpdateHookSwing(float DeltaSeconds);

	/** Server side of the end of a travel. bArrived keeps a little of the speed; a cancel keeps it all. */
	void EndHookSwing(bool bArrived);

	UFUNCTION()
	void OnRep_HookSwing();

	/** True while the hook is carrying the player to its point. */
	UPROPERTY(ReplicatedUsing = OnRep_HookSwing)
	bool bHookSwinging = false;

	/** What the hook caught, and where the body is headed. Replicated so every machine draws the rope. */
	UPROPERTY(Replicated)
	FVector_NetQuantize HookAnchor = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector_NetQuantize HookLandPoint = FVector::ZeroVector;

	/** The travel ends at a ledge (climbable geometry was hooked): face HookFaceYaw and hand the body to DPTS. */
	UPROPERTY(Replicated)
	bool bHookToLedge = false;

	UPROPERTY(Replicated)
	float HookFaceYaw = 0.0f;

	/** Local, per machine: until this time the ledge catch is asked for every frame. */
	float HookCatchUntil = -1.0f;

	/** Turns the body to the ledge and asks DPTS to catch, on the machines that run the DPTS search. */
	void UpdateHookLedgeCatch();

	/** Local, per machine: when this machine saw the travel start. */
	float HookSwingStartTime = 0.0f;

	/** Fires on server and clients alike, so the Blueprint graph can react without an OnRep. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|State")
	void OnDownedStateChanged(bool bNewDowned);

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|State")
	void OnCarriedStateChanged(bool bNewBeingCarried);

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Health")
	void OnDeadStateChanged(bool bNewDead);

	// ---------------------------------------------------------------------------------------
	// Carry state. Replicated because the server decides the attach and every machine has to
	// reproduce it -- attachment itself is not a replicated property.
	// ---------------------------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_CarriedActor, BlueprintReadOnly, Category = "L2H|Carry")
	TObjectPtr<AActor> CarriedActor;

	/**
	 * The socket the active carry attached to. Replicated, and listed BEFORE CarriedActor in
	 * GetLifetimeReplicatedProps so a client always holds the socket by the time it attaches.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Carry")
	FName ActiveCarrySocket = NAME_None;

	/** The previous CarriedActor, so OnRep can detach the right one when the property clears. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviouslyCarriedActor;

	/**
	 * The target's collision state before the carry took it over, captured rather than assumed: a
	 * crate and a downed ragdoll do not go back the same way, so restore what was there.
	 */
	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> CarriedOriginalCollision = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(Transient)
	bool bCarriedOriginalSimulatePhysics = false;

	UFUNCTION()
	void OnRep_CarriedActor();

	/** Socket a carried actor uses when the object's own profile names none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry")
	FName DefaultCarrySocket = NAME_None;

	/**
	 * The lift the carrier plays as a carry begins. Assigned on the Blueprint child whose skeleton
	 * matches it; empty on another pawn is the correct state, not a missing assignment.
	 *
	 * A one-shot rather than a hold pose: the hold is the attachment, and this is only the beat that
	 * sells the hands taking the weight. Stopped with the carry, so an automatic drop -- going down
	 * mid-carry -- releases the pose too.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Carry")
	TObjectPtr<UAnimMontage> CarryLiftMontage;

	/** Pawn-owned ASC. Created here, not on the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Abilities")
	TObjectPtr<UL2H_AbilitySystemComponent> AbilitySystemComponent;

	/** Cover against walls (L3 next to one). Created here so every character has it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Cover")
	TObjectPtr<UL2H_CoverComponent> CoverComponent;

	/**
	 * A UAttributeSet is a UObject, not a component, so it cannot be added on the Blueprint
	 * child. UAbilitySystemComponent::InitializeComponent registers every attribute set whose
	 * outer is the owner actor, which is what picks this one up.
	 */
	UPROPERTY()
	TObjectPtr<UL2H_TimeAttributeSet> TimeAttributeSet;

	/**
	 * Extra abilities a level grants on top of the fixed base kit. Optional.
	 *
	 * The base kit itself (hook and carry) lives on the two class properties below, because those are
	 * what the input handlers activate by class; read on one property, they cannot disagree. This set
	 * is the extension point: a level adds to it without touching a character asset, and the base kit
	 * survives the set being swapped or left empty.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TObjectPtr<UL2H_AbilitySet> DefaultAbilitySet;

	/**
	 * Activated by InteractAction, and the pad's counterpart of the same key. The context decision lives
	 * in this one ability while carry and hook keep their own rules, so the pawn never has to know which
	 * of them a single press meant.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> InteractAbilityClass;

	/** Forwarded to by the interact ability when the target is light enough to lift. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> CarryAbilityClass;

	/** Forwarded to by the interact ability when the target is too heavy to lift. Also on HookAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> HookAbilityClass;

	/**
	 * Activated by the transfer input row. Part of the fixed base kit rather than a character set,
	 * because handing time to a downed partner has to be available to whoever is still standing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> TransferAbilityClass;

	/**
	 * Activated by the fire input while an object is carried: the shot becomes the throw. A carried object
	 * is the one state that changes what the fire button means, which is what lets aim and fire serve
	 * carry, throw and shooting without a second binding or a contested key.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> ThrowAbilityClass;

	/** Sprint multiplier over the authored walk speed. 1.4 turns a 500 run into a 700 sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Movement", meta = (ClampMin = "1.0"))
	float SprintMultiplier = 1.4f;

	/** Collision profile the mesh switches to while ragdolled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|State")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** Collision profile the mesh returns to when the ragdoll ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|State")
	FName StandingCollisionProfile = TEXT("CharacterMesh");

	/** Time clock HUD class. Assigned on BP_L2H_PlayerCharacter (WBP_L2H_TimeClock). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|UI")
	TSubclassOf<UL2H_TimeClockWidget> TimeClockWidgetClass;

	/** Only exists on the machine that locally controls this pawn. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "L2H|UI")
	TObjectPtr<UL2H_TimeClockWidget> TimeClockWidget;

	void CreateTimeClock();
	void DestroyTimeClock();
	void RefreshTimeClock();

	/** Host-only helper behind the test commands. */
	void RunTimeTestCommand(int32 PlayerIndex, float Amount, bool bRestore);

	/** Host-only helper behind the health test commands. */
	void RunHealthTestCommand(int32 PlayerIndex, float Amount, bool bRestore);

	/** Grants DefaultAbilitySet on the authority. Idempotent, so BeginPlay may call it freely. */
	void GrantDefaultAbilities();

	/** Motion-warp-free ragdoll toggle, applied locally on server and clients. */
	void ApplyDownedPhysics(bool bNewDowned);

	/** Reads the ragdoll: where the capsule stands, which way it faces, and the get-up clip (back or front). */
	UAnimMontage* ComputeGetUp(FVector& OutLocation, FRotator& OutRotation) const;

	/** Recomputes MaxWalkSpeed from the cached authored value and the active scales. */
	void RefreshWalkSpeed();

	/** Overrides the jump gate so a downed or carried pawn cannot jump. */
	virtual bool CanJumpInternal_Implementation() const override;

private:
	// ---------------------------------------------------------------------------------------
	// Health
	// ---------------------------------------------------------------------------------------

	/** Fires once the regeneration delay has elapsed, then hands off to the per-second tick. */
	void StartHealthRegen();

	/** Applies one second of regeneration, and re-arms itself while there is still a gap to close. */
	void TickHealthRegen();

	/** Armed by NotifyHealthDamaged. Every hit restarts it, so fire suppresses regeneration. */
	FTimerHandle HealthRegenDelayTimer;

	FTimerHandle HealthRegenTickTimer;

	/** MaxWalkSpeed captured the first time anything takes it over. 0 means "nothing captured yet". */
	UPROPERTY(Transient)
	float CachedWalkSpeed = 0.0f;

	UPROPERTY(Transient)
	bool bWalkSpeedCached = false;

	UPROPERTY(Transient)
	float SprintScale = 1.0f;

	UPROPERTY(Transient)
	float CarryScale = 1.0f;

	UPROPERTY(Transient)
	float AimScale = 1.0f;

	/** Captured the first time anything narrows the camera, so restoring is not a second narrowing. */
	UPROPERTY(Transient)
	float CachedFieldOfView = 90.0f;

	UPROPERTY(Transient)
	bool bFieldOfViewCached = false;

	// ---------------------------------------------------------------------------------------
	// Shards
	// ---------------------------------------------------------------------------------------

	/**
	 * Replicated so the HUD can show a partner's state, and so both screens agree on the cap. Not
	 * exposed: a private member may carry neither BlueprintReadOnly nor a Category, and
	 * GetCarriedShardCount is the read path the HUD and the Blueprints use.
	 */
	UPROPERTY(Replicated)
	int32 CarriedShardCount = 0;

	/** Closes the absorb window, releasing the slot. */
	FTimerHandle ShardAbsorbTimer;

	/** The terminal this pawn is inside. Transient: a place in the level, not saved state. */
	UPROPERTY(Transient)
	TObjectPtr<AL2H_CheckpointTerminal> CurrentTerminal;

	/** The menu, spawned per local player on first open and reused afterwards. */
	UPROPERTY(Transient)
	TObjectPtr<UL2H_TimeMenuWidget> TerminalMenu;

	/** The edge-of-screen strip a partner's request appears on. */
	UPROPERTY(Transient)
	TObjectPtr<UL2H_TimeRequestPromptWidget> RequestPrompt;

	/** Points the local prompt at the live request, or collapses it. */
	void RefreshRequestPrompt();

	/** Mesh relative transform captured at BeginPlay, restored when the ragdoll ends. */
	FVector MeshRelativeLocation = FVector::ZeroVector;
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;
};
