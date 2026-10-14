// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ball/L2H_BallTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_TimeBall.generated.h"

class AL2H_BaseCharacter;
class AL2H_TimeDirector;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Samer's thrown orb.
 *
 * The ball owns its own flight and its own resolution. That is the whole reason the throwing ability
 * can end on the frame the ball leaves the hand: a second press then re-activates the ability, finds
 * this actor and asks it what to do, which is what makes Recall the natural baseline rather than a
 * special case.
 *
 * Every decision here is server-side. Clients see only a replicated actor that moves and then
 * disappears, so nothing about the flight has to agree between machines -- and the teleport is
 * replicated the ordinary way, through the character's movement.
 */
UCLASS()
class LASTTWOHOURS_API AL2H_TimeBall : public AActor
{
	GENERATED_BODY()

public:
	AL2H_TimeBall();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Server only. Arms the ball and starts the flight. Called immediately after spawn, so the ball
	 * is never ticking with default limits.
	 */
	void InitBall(AL2H_BaseCharacter* InThrower, const FL2H_BallLimits& InLimits, AL2H_TimeDirector* InDirector);

	/** Server only. A second press while this ball is out: recall it, or teleport to it now. */
	void HandleRepeatPress();

	/** The unresolved ball thrown by Thrower, or null. Scans by owner, so two players can both have one out. */
	static AL2H_TimeBall* FindForOwner(const UObject* WorldContextObject, const AActor* Thrower);

	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	AL2H_BaseCharacter* GetThrower() const { return Thrower; }

	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsResolved() const { return bResolved; }

	/** True once the ball has stopped moving and is waiting for a manual trigger. */
	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsWaitingForTrigger() const { return bSettled && !bResolved; }

protected:
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	/** The teleport. Moves nobody when the spot is not usable. */
	void ResolveTeleport();

	/** Brings the ball back to the thrower instead of moving him. */
	void ResolveRecall();

	/** Gives up, refunding what the throw cost when the limits say to. */
	void ResolveFailure(const FString& Reason);

	/** Removes the ball. Every resolution ends here, so there is one place that stops it. */
	void Finish();

	/** Time changes go through the director, so the ball obeys the same effect-only rule as the abilities. */
	void PayCost(float Seconds);
	void Refund(float Seconds);

	/** Seconds owed for the time already spent in the air. */
	float GetAirborneCost() const;

	/** Capsule-first check that the landing spot is free, using the thrower's own capsule. */
	bool IsDestinationClear(FString& OutReason) const;

	/**
	 * Soft guard for a pawn that also carries a parkour component. The component is a Blueprint type
	 * this module does not compile against, so it is found by class name and read reflectively; a
	 * missing component or a missing property is treated as idle rather than as a refusal.
	 */
	bool IsParkourStateIdle(FString& OutReason) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|ChronoBall")
	TObjectPtr<USphereComponent> BallCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|ChronoBall")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|ChronoBall")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Copied from the throwing ability, so the ability's tuning is the only copy of these numbers. */
	UPROPERTY(Transient)
	FL2H_BallLimits Limits;

	UPROPERTY(Transient)
	TObjectPtr<AL2H_BaseCharacter> Thrower;

	UPROPERTY(Transient)
	TObjectPtr<AL2H_TimeDirector> Director;

	/** Where the throw started. MaxRange is measured from here, not from the moving thrower. */
	FVector ThrowLocation = FVector::ZeroVector;

	/** Seconds in the air. Reset when the ball is handed its limits. */
	float AirborneTime = 0.0f;

	/** True while the ball is flying home: it resolves by arriving, not by teleporting. */
	bool bReturning = false;

	/** Set as soon as a resolution starts, so no second resolution can overlap it. */
	bool bResolved = false;

	/** The ball has stopped moving. Only meaningful before anything resolves. */
	bool bSettled = false;
};
