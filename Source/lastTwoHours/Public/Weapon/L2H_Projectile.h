// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Damage/L2H_DamageTypes.h"

#include "L2H_Projectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * A physical round: it leaves the muzzle at the weapon's speed, falls with gravity, and does its damage
 * where it actually lands -- so a long shot drops, a moving target can be missed, and a loose crate gets
 * knocked by the hit.
 *
 * Server-spawned and replicated. Only the server resolves the hit; clients just draw the flight. The damage
 * goes the same two ways an instant shot does -- through the time director (health, and the time it costs)
 * and through ApplyPointDamage (anything with its own TakeDamage, such as the test robot).
 */
UCLASS(meta = (DisplayName = "L2H Projectile"))
class LASTTWOHOURS_API AL2H_Projectile : public AActor
{
	GENERATED_BODY()

public:
	AL2H_Projectile();

	/** Server only, straight after spawning: who fired, how hard it hits, and which way it flies. */
	void InitShot(AActor* InShooter, float InDamage, EL2H_DamageType InDamageType, float InHeadshotMultiplier,
		const FVector& Direction, float Speed);

	/** Push given to a simulating body on impact, as a velocity change scaled down by its mass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Projectile", meta = (ClampMin = "0.0"))
	float ImpactPush = 60000.0f;

	/** Seconds before an unspent round is removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Projectile", meta = (ClampMin = "0.1"))
	float MaxLifetime = 3.0f;

	/** Fired on every machine where the round hits, for a spark or a sound in a Blueprint child. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Projectile")
	void OnImpact(const FVector& Location, const FVector& Normal);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal);

	float Damage = 0.0f;
	float HeadshotMultiplier = 2.0f;
	EL2H_DamageType DamageType = EL2H_DamageType::Pistol;
	bool bSpent = false;
};
