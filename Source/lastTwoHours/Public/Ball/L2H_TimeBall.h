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

	/** Registers the hold, which is the one piece of this actor's state a client cannot infer. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server only. Arms the ball and starts the flight. Called immediately after spawn, so the ball
	 * is never ticking with default limits.
	 */
	void InitBall(AL2H_BaseCharacter* InThrower, const FL2H_BallLimits& InLimits, AL2H_TimeDirector* InDirector);

	/** Server only. A second press while this ball is out: recall it, or teleport to it now. */
	void HandleRepeatPress();

	/**
	 * Server only. Takes the orb out of the world and holds it in the thrower's hand, waiting for the
	 * launch button. The flat cost is paid by the ability that calls this, so nothing is charged here:
	 * this is the same throw, it simply has not been released yet.
	 */
	void HoldInHand(AL2H_BaseCharacter* InThrower, const FL2H_BallLimits& InLimits, AL2H_TimeDirector* InDirector);

	/** Server only. Releases a held orb along the thrower's aim, starting the flight the hold deferred. */
	void LaunchFromHold();

	/**
	 * Server only. The launch button. One press always means "commit to where the orb is": it releases
	 * an orb still in the hand, and it travels to an orb that is already out.
	 */
	void HandleLaunchPress();

	/**
	 * The unresolved ball thrown by Thrower, or null. Scans by owner, so two players can both have one out.
	 * Samer can have a travel orb and a planted field out at once, so the one in the hand comes first, then
	 * one still travelling, then a planted field. ExactClass limits it to one kind of orb.
	 */
	static AL2H_TimeBall* FindForOwner(const UObject* WorldContextObject, const AActor* Thrower, const UClass* ExactClass = nullptr);

	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	AL2H_BaseCharacter* GetThrower() const { return Thrower; }

	/** The limits this orb was handed, so the thrower's preview draws the arc THIS orb will take. */
	const FL2H_BallLimits& GetLimits() const { return Limits; }

	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsResolved() const { return bResolved; }

	/** True while the orb is waiting in the thrower's hand rather than flying. */
	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsHeld() const { return bHeld; }

	/** True once the ball has stopped moving and is waiting for a manual trigger. */
	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsWaitingForTrigger() const { return bSettled && !bResolved; }

	// ----- time field -----------------------------------------------------------------------

	/**
	 * The orb is a standing time field rather than a way to travel: it pins itself to whatever it
	 * hit and never moves the thrower.
	 *
	 * True when FieldDiameter is set and FieldDilation is anything but 1. Nothing below needs to know
	 * whether that means slow or haste -- the dilation number says it, and ApplyActorSlow already
	 * accepts anything from 0.01 to 100. That is what lets one actor be both the Slow orb and the
	 * Haste orb instead of two near-identical classes.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsFieldEnabled() const;

	/** True between planting and closing. The orb is standing still and owning a field. */
	UFUNCTION(BlueprintPure, Category = "L2H|ChronoBall")
	bool IsFieldActive() const { return bFieldActive; }

protected:
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void OnRep_Held();

	/** Puts the orb in the holder's hand, or drops it back into the world when it is no longer held. */
	void AttachToHolder();

	/** Puts a held orb away without throwing it, refunding the flat cost the hold paid. */
	void PutAway();

	/** The teleport. Moves nobody when the spot is not usable. */
	void ResolveTeleport();

	/** Brings the ball back to the thrower instead of moving him. */
	void ResolveRecall();

	/** Gives up, refunding what the throw cost when the limits say to. */
	void ResolveFailure(const FString& Reason);

	/** Removes the ball. Every resolution ends here, so there is one place that stops it. */
	void Finish();

	/**
	 * Server only. Pins the orb to the surface it hit and starts the field around it. From here the
	 * orb is scenery with a timer, not a projectile: it charges no airborne cost and never teleports.
	 */
	void PlantField(const FHitResult& Impact);

	/** Re-scans who is inside the field, lets go of whoever left and takes in whoever arrived. */
	void RefreshField();

	/** Puts every actor this field touched back to its own dilation. Safe to call when none is open. */
	void EndField();

	/** The field's own duration ran out: close it, then take the orb out of the world. */
	void HandleFieldExpired();

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

	/**
	 * The orb is in the thrower's hand. Replicated with a RepNotify because attachment is not: every
	 * machine has to reproduce the attach itself, and until it does, its copy of the orb would sit
	 * where it spawned instead of following the hand.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Held)
	bool bHeld = false;

	/** How far above the palm the held orb floats once it has risen, in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Hold")
	float HoldRiseHeight = 28.0f;

	/** Seconds the orb takes to rise from the palm to HoldRiseHeight -- about the length of the summon raise. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Hold", meta = (ClampMin = "0.05"))
	float HoldRiseTime = 0.6f;

	/** The slow bob of the floating orb, in cm either way. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Hold")
	float HoldBobHeight = 2.5f;

	// ----- time field -----------------------------------------------------------------------

	/**
	 * Dilation every actor inside the field is given. 0.25 is the Slow orb, 2.0 is the Haste orb, and
	 * 1.0 disables the field so the orb keeps behaving as the plain travel orb.
	 *
	 * This is the only number that separates the two orbs, which is why there is no "is haste" flag
	 * anywhere: a second flag could disagree with the dilation it was supposed to describe.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Field", meta = (ClampMin = "0.05", ClampMax = "20.0"))
	float FieldDilation = 1.0f;

	/** Diameter of the dilated sphere, in cm. 0 leaves the orb as the plain travel orb. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Field", meta = (ClampMin = "0.0"))
	float FieldDiameter = 0.0f;

	/** Seconds the field stands once the orb has planted. 0 means it never closes on its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Field", meta = (ClampMin = "0.0"))
	float FieldDuration = 6.0f;

	/** Seconds between two re-scans of who is standing in the field. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Field", meta = (ClampMin = "0.05"))
	float FieldRefreshInterval = 0.2f;

	/**
	 * Leave the thrower out of his own field. On for the Slow orb, whose thrower must not be slowed by
	 * the orb he just threw; off for the Haste orb, which is usually thrown to be stood in.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall|Field")
	bool bFieldExemptsThrower = true;

	/** True between planting and closing, on the server. */
	UPROPERTY(Transient)
	bool bFieldActive = false;

	/** The actors this field is currently dilating, so exactly those can be given their speed back. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> FieldActors;

	FTimerHandle FieldRefreshTimer;
	FTimerHandle FieldEndTimer;

private:
	/** Every machine: keeps a held orb floating above the hand it is attached to. Cosmetic. */
	void UpdateHeldFloat(float DeltaSeconds);

	/** Seconds since the orb came into the hand, on this machine. */
	float HeldSeconds = 0.0f;
};
