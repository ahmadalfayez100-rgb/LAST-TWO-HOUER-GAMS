// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_ModeOrb.generated.h"

class UL2H_AbilityModeComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * A small thrown orb for the ability modes.
 *  - Return (Samer): lands and stays where it stopped, glowing, as the point he can come back to.
 *  - Slow (Yousef): sticks to the first thing it hits and slows that thing's time (its tick, its
 *    animation, its movement) for a while, then fades. On the floor it fizzles.
 * Replicated: the server decides what it hit; every machine applies the slow to its own copy of the
 * target so the animation slows too, and puts it back when the orb goes.
 */
UCLASS(meta = (DisplayName = "L2H Mode Orb"))
class LASTTWOHOURS_API AL2H_ModeOrb : public AActor
{
	GENERATED_BODY()

public:
	AL2H_ModeOrb();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server: sets it flying. */
	void Launch(UL2H_AbilityModeComponent* InOwnerModes, bool bInSlow, const FVector& Velocity, float InSlowFactor, float InSlowSeconds, float InLifetime);

	bool IsSlowOrb() const { return bSlow; }

	/** True once it has stopped (the marker is ready to be returned to). */
	bool HasLanded() const { return bLanded; }

	AActor* GetSlowedTarget() const { return SlowedTarget; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "L2H|Orb")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "L2H|Orb")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "L2H|Orb")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "L2H|Orb")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(Replicated)
	bool bSlow = false;

	UPROPERTY(Replicated)
	bool bLanded = false;

	UPROPERTY(ReplicatedUsing = OnRep_SlowedTarget)
	TObjectPtr<AActor> SlowedTarget;

	UPROPERTY(Replicated)
	float SlowFactor = 0.2f;

	UFUNCTION()
	void OnRep_SlowedTarget();

	UFUNCTION()
	void OnStop(const FHitResult& Hit);

private:
	void ApplySlowLocally(AActor* Target);
	void RemoveSlowLocally();

	TWeakObjectPtr<UL2H_AbilityModeComponent> OwnerModes;
	TWeakObjectPtr<AActor> LocallySlowed;
	float SlowSeconds = 6.0f;
};
