// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "L2H_EnemyRobot.generated.h"

class AL2H_BaseCharacter;

/**
 * A test enemy: the Robot Scout R21 walking at the nearest standing player and hitting them.
 *
 * Deliberately small. It exists to exercise the systems the players already have -- weapons, health
 * eating into time, death, carry and revive -- not to be the game's AI. It steers straight at its
 * target (the test levels have no navmesh), attacks in melee range on a cooldown, and goes limp when
 * shot down.
 *
 * Server-authoritative throughout: the server decides the target, the hits and the death; clients see
 * the movement through normal character replication and the death through OnRep_Dead.
 */
UCLASS(meta = (DisplayName = "L2H Enemy Robot"))
class LASTTWOHOURS_API AL2H_EnemyRobot : public ACharacter
{
	GENERATED_BODY()

public:
	AL2H_EnemyRobot();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Bullets arrive here through ApplyPointDamage, which the fire ability sends to whatever it hits. */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Enemy")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "L2H|Enemy")
	float GetHealth() const { return Health; }

	/** Server: stops chasing and attacking for Seconds (a melee hit). A stunned robot can be finished. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Enemy")
	void Stun(float Seconds);

	UFUNCTION(BlueprintPure, Category = "L2H|Enemy")
	bool IsStunned() const { return bStunned; }

	/** Server: the finisher. Dies at once and is thrown away from Attacker. */
	void FinishBy(AActor* Attacker);

	/** Fired on every machine when the robot swings, so a Blueprint child can add a sound or a flash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Enemy")
	void OnAttack(AActor* Target);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "1.0"))
	float MaxHealth = 60.0f;

	/** Damage per swing, in health points: it goes through the director like any other hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "0.0"))
	float AttackDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "50.0"))
	float AttackRange = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "0.1"))
	float AttackInterval = 2.0f;

	/** How far it notices a player. Beyond this it stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "100.0"))
	float SightRange = 2500.0f;

	/** Seconds the wreck stays before it is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "0.0"))
	float CorpseLifetime = 10.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Enemy")
	float Health = 60.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly, Category = "L2H|Enemy")
	bool bDead = false;

	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Enemy")
	bool bStunned = false;

	float StunEndTime = 0.0f;

	/** Every machine: the body rocks while stunned, so the opening to finish it reads at a glance. */
	void UpdateStunLook();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastThrow(FVector_NetQuantize Velocity);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttack(AActor* Target);

	/** Nearest player who is neither downed nor dead, within SightRange. */
	AL2H_BaseCharacter* FindTarget() const;

	void Die();
	void ApplyDeathPhysics();

	float NextAttackTime = 0.0f;
};
