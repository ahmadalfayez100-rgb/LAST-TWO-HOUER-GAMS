// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/L2H_Breakable.h"

#include "L2H_ExplosiveBarrel.generated.h"

class UNiagaraSystem;
class UCameraShakeBase;

/**
 * A barrel that explodes when it breaks (MSHF's barrel and its pieces). Everything within ExplosionRadius
 * is hurt by distance: players through the time director (Health, and the time it is worth), enemies through
 * their damage and a stun, other breakables (so barrels set each other off). Loose physics bodies are
 * pushed, and players near it feel the blast on their camera.
 *
 * The shot that saves the rounds: one bullet into a barrel beside a robot does the work of a magazine.
 */
UCLASS(meta = (DisplayName = "L2H Explosive Barrel"))
class LASTTWOHOURS_API AL2H_ExplosiveBarrel : public AL2H_Breakable
{
	GENERATED_BODY()

public:
	AL2H_ExplosiveBarrel();

protected:
	virtual void Break(AActor* Breaker) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "50.0"))
	float ExplosionRadius = 450.0f;

	/** At the centre; falls off linearly to nothing at the radius. Health points for players. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "0.0"))
	float PlayerDamage = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "0.0"))
	float EnemyDamage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "0.0"))
	float EnemyStunSeconds = 3.0f;

	/** Seconds before a barrel caught in the blast goes too, so a row of them goes off as a chain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "0.0"))
	float ChainDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion")
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	/** Players within this distance feel the blast on their camera (scaled by distance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Explosion", meta = (ClampMin = "0.0"))
	float ShakeRadius = 1500.0f;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosionFX();
};
