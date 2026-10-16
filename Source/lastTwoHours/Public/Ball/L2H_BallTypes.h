// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimMontage;

#include "L2H_BallTypes.generated.h"

/** What turns a throw into a teleport. */
UENUM(BlueprintType)
enum class EL2H_BallTrigger : uint8
{
	/**
	 * Once the ball has stopped moving. The baseline: it is the only trigger that guarantees the ball
	 * is somewhere a person could stand, which is what makes the landing check meaningful.
	 */
	OnSettled UMETA(DisplayName = "On Settled"),

	/** The moment it first touches anything. Faster to fire, but the ball may be flat against a wall. */
	OnFirstImpact UMETA(DisplayName = "On First Impact"),

	/** Only when the player presses again, so a throw can be lined up and then committed. */
	Manual UMETA(DisplayName = "Manual")
};

/** What a second press does while a ball is already out. */
UENUM(BlueprintType)
enum class EL2H_BallRepeatBehavior : uint8
{
	/** Bring the ball back and leave the thrower where he is. The baseline behaviour. */
	Recall UMETA(DisplayName = "Recall"),

	/** Teleport to wherever the ball is right now, if that spot is valid. */
	TeleportNow UMETA(DisplayName = "Teleport Now")
};

/**
 * Every limit of the Chrono Ball, in one editable block.
 *
 * The plan's rule is that no gameplay number is compiled in, so nothing below is a constant: the
 * throwing ability owns the whole struct, the ball is handed a copy at spawn, and tuning happens on
 * the ability's Blueprint without a rebuild.
 */
USTRUCT(BlueprintType)
struct FL2H_BallLimits
{
	GENERATED_BODY()

	// ----- flight -------------------------------------------------------------------------

	/** Launch speed along the aim direction, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float ThrowSpeed = 1800.0f;

	/** Ceiling the projectile's speed is clamped to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float MaxSpeed = 2200.0f;

	/** 0 = a flat throw, 1 = an ordinary arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	/** 0 = it stops dead, 1 = it keeps its speed. Drives how far a low throw rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bounciness = 0.5f;

	/** Speed below which the ball is treated as settled. Also the projectile's stop threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float SettleVelocityThreshold = 120.0f;

	/** How far in front of the thrower the ball is created. Keeps it out of the thrower's capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float ThrowForwardOffset = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	EL2H_BallTrigger Trigger = EL2H_BallTrigger::OnSettled;

	// ----- limits -------------------------------------------------------------------------

	/** Distance from the throw point past which the ball resolves as a refusal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float MaxRange = 2500.0f;

	/** Seconds in the air past which the ball resolves as a refusal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float MaxFlightTime = 3.0f;

	/** Seconds the ball exists at all, settled or not, before it gives up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float BallLifetime = 8.0f;

	// ----- cost and cadence -----------------------------------------------------------------

	/** Paid the moment the ball leaves the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float CostFlat = 2.0f;

	/** Paid per second the ball is in the air, settled when the ball resolves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float CostPerSecondAirborne = 0.5f;

	/** Seconds before another throw may be made. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float Cooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	EL2H_BallRepeatBehavior RepeatPressBehavior = EL2H_BallRepeatBehavior::Recall;

	/** A refused throw costs nothing. Off means a wide throw still burns the flat cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	bool bRefundCostOnFailure = true;

	/** The ball is consumed by a successful teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	bool bDestroyBallAfterSuccess = true;

	// ----- landing validation ---------------------------------------------------------------

	/** Refuse the teleport when the space the thrower's capsule would occupy is blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	bool bRequireClearLandingSpot = true;

	/** How far above the ball the capsule test is centred, so the check is not done inside the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float LandingClearanceAboveFeet = 5.0f;

	/**
	 * Refuse while a parkour component reports a busy or non-idle state. It only ever applies to a pawn
	 * that actually has one -- a pawn without the component skips the check entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	bool bBlockOnParkourState = true;

	// ----- recall ---------------------------------------------------------------------------

	/** cm/s the ball travels back to the thrower when recalled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float RecallSpeed = 2400.0f;

	/** Distance at which a returning ball counts as back in the hand and disappears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.0"))
	float RecallAcceptRadius = 90.0f;

	// ----- hold and aim ---------------------------------------------------------------------

	/**
	 * Socket the orb waits in while it is held. The orb is a real actor from the moment it is taken
	 * out -- which is what lets the throw be aimed -- so this is where it hangs until it is released.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	FName HoldSocket = TEXT("hand_r");

	/**
	 * Seconds of flight the dotted preview arc covers. Independent of MaxFlightTime: the arc is a view
	 * aid, and drawing it for the whole legal flight would clutter the screen for a long throw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.1"))
	float PreviewMaxTime = 2.5f;

	/** Seconds between two dots of the preview arc. Smaller is a smoother curve and more dots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall", meta = (ClampMin = "0.01"))
	float PreviewStepTime = 0.06f;
	
	// ----- animation ------------------------------------------------------------------------
	
	/**
	 * Looping clip played on the thrower's mesh while the orb waits in his hand. This is the pose the
	 * player reads as the spell being held in front of him: the orb is already real and aimable, so the
	 * hand carrying it should not be the ordinary idle. Empty is a valid look and plays nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	TObjectPtr<UAnimMontage> HoldMontage;
	
	/** One-shot clip played on the thrower the frame the orb is released. Empty plays nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|ChronoBall")
	TObjectPtr<UAnimMontage> LaunchMontage;
};
