// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "L2H_EnemyRobot.generated.h"

class AL2H_BaseCharacter;
class AL2H_HidingCloset;
class UPointLightComponent;
class UAnimSequenceBase;

/** What the robot is doing; its eye light shows it (blue calm, yellow wary, red hunting). */
UENUM(BlueprintType)
enum class EL2H_RobotState : uint8
{
	/** Standing at home, or walking its patrol points. */
	Patrol,
	/** Something caught its eye: it stops and stares while its awareness fills. */
	Suspicious,
	/** Walking to where it last saw or heard something. */
	Investigate,
	/** It knows where a player is and goes for them. */
	Chase,
	/** Lost them: looks around where they were last seen. */
	Search,
	/** Gave up: back home. */
	Return,
};

/**
 * The Robot Scout R21: a patrolling enemy that has to notice you before it fights.
 *
 * Senses, all on the server:
 *  - Sight: a cone ahead (SightHalfAngle) out to SightRange, blocked by walls. Crouching shortens it;
 *    a lit flashlight lengthens it and shows from any side. Within CloseSenseRange it feels you anyway.
 *    Seeing is not knowing: Awareness fills over time, faster the closer you are, and drains when you
 *    are out of view -- a quick crossing at the far end of a room is a yellow eye, not a fight.
 *  - Hearing: shots, explosions and breaking crates (ReportNoise), and a player running nearby. A noise
 *    sends it to look; it does not know who made it.
 *  - Being hit: it knows where the shot came from at once.
 * It tells the robots around it when it starts a hunt, and remembers where it last saw its target: it
 * walks there, looks around, and goes home if it finds nobody. A player it watched step into a closet is
 * pulled back out.
 *
 * It walks the level's navmesh where there is one; without one it steers round what is in front of it.
 * Attacks are telegraphed (AttackWindup) and land only on a target still in front, in reach and in sight.
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

	UFUNCTION(BlueprintPure, Category = "L2H|Enemy")
	EL2H_RobotState GetState() const { return State; }

	/** 0..1: how sure it is that someone is there. 1 starts a hunt. */
	UFUNCTION(BlueprintPure, Category = "L2H|Enemy")
	float GetAwareness() const { return Awareness; }

	/**
	 * Server: a sound at Location that robots within Radius may hear (a shot, a blast, a crate breaking). Each
	 * one that hears it and is not already hunting goes to look. Instigator is not revealed by it.
	 */
	static void ReportNoise(const UObject* WorldContext, const FVector& Location, float Radius);

	/** Server: another robot started a hunt nearby; this one comes to look. */
	void HearAlert(const FVector& Location);

	/** Before BeginPlay (a deferred spawn): the patrol route, relative to where it stands and the way it faces. */
	void SetPatrolPoints(const TArray<FVector>& Points) { PatrolPoints = Points; }

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

	/** Seconds between committing to a swing and the blow landing: it stops and leans back, facing fixed, so a
	 * step aside or a dodge (its i-frames) gets out of the way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "0.0"))
	float AttackWindup = 0.5f;

	/** The blow reaches this far up or down (cm, centre to centre): a player on a block above it is out of reach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "0.0"))
	float AttackHeightReach = 100.0f;

	/** The blow lands only on a target inside this cone ahead (cosine of the half-angle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float AttackConeCos = 0.5f;

	// ---------------------------------------------------------------------------------------------
	// Senses
	// ---------------------------------------------------------------------------------------------

	/** How far it can see a standing player in its view cone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "100.0"))
	float SightRange = 2200.0f;

	/** Half the width of its view, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float SightHalfAngle = 65.0f;

	/** Inside this it notices a player from any side, seen or not. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float CloseSenseRange = 250.0f;

	/** Sight range multiplier for a crouched player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSightScale = 0.5f;

	/** Sight range multiplier for a player with a lit flashlight, who is also seen from any side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "1.0"))
	float FlashlightSightScale = 1.4f;

	/** Seconds of looking at a player at half its sight range before it is sure. Closer is quicker, farther slower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.05"))
	float NoticeSeconds = 1.2f;

	/** Awareness lost per second while nothing is in view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float AwarenessDecay = 0.3f;

	/** A player moving faster than RunNoiseSpeed (not crouched) is heard within this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float RunHearingRange = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float RunNoiseSpeed = 420.0f;

	/** Robots within this join a hunt this one starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float AlertRadius = 1500.0f;

	/** Seconds out of sight before a hunt turns into a walk to the last place it saw them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float LoseSightSeconds = 1.0f;

	/** Seconds it looks around where it lost them before going home. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Senses", meta = (ClampMin = "0.0"))
	float SearchSeconds = 6.0f;

	// ---------------------------------------------------------------------------------------------
	// Grab: a swing that lands may be a grab instead. The player mashes to break free; the partner can
	// free them by hitting the robot; otherwise the bite lands.
	// ---------------------------------------------------------------------------------------------

	/** Chance a landed swing is a grab (when the cooldown allows). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrabChance = 0.35f;

	/** Seconds between grabs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.0"))
	float GrabCooldown = 10.0f;

	/** Seconds the player has to break free before the bite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.5"))
	float GrabSeconds = 4.0f;

	/** Health the bite takes (through the director, so it costs time too). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.0"))
	float GrabBiteDamage = 30.0f;

	/** How far in front of the robot the held player stands (capsule centres). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.0"))
	float GrabDistance = 40.0f;

	/** Seconds the robot reels after a player breaks free or the partner hits it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab", meta = (ClampMin = "0.0"))
	float GrabEscapeStun = 1.6f;

	/** MSHF's paired grab clips: start, loop, break free, bite. [0..3] the robot's, [4..7] the player's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Grab")
	TArray<TObjectPtr<UAnimSequenceBase>> GrabClips;

	// ---------------------------------------------------------------------------------------------
	// Movement
	// ---------------------------------------------------------------------------------------------

	/** Patrol route, relative to where it was placed. Empty = it stands guard at home. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Patrol", meta = (MakeEditWidget = "true"))
	TArray<FVector> PatrolPoints;

	/** Seconds it waits at each patrol point, looking about. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWait = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Patrol", meta = (ClampMin = "0.0"))
	float InvestigateSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Enemy|Patrol", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 380.0f;

	/** The eye: its colour is the state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Enemy")
	TObjectPtr<UPointLightComponent> EyeLight;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "L2H|Enemy")
	EL2H_RobotState State = EL2H_RobotState::Patrol;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Enemy")
	float Awareness = 0.0f;

	UFUNCTION()
	void OnRep_State();

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


	void Die();
	void ApplyDeathPhysics();

	float NextAttackTime = 0.0f;

	/** Server: a swing is wound up and lands at StrikeTime on whoever is then in front and in reach. */
	bool bWindingUp = false;
	float StrikeTime = 0.0f;
	TWeakObjectPtr<AL2H_BaseCharacter> WindupTarget;
	void Strike();

	/** Every machine: the lean-back that telegraphs the swing. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastWindup(float Seconds);
	float WindupLookUntil = -1.0f;

	// ---------------------------------------------------------------------------------------------
	// Brain (server)
	// ---------------------------------------------------------------------------------------------

	void SetState(EL2H_RobotState NewState);

	/** Looks for players; fills or drains Awareness; remembers where the one it watches was. */
	void UpdateSenses(float DeltaSeconds);

	/** The player it can see best right now (in range, in view, not behind a wall), or null. */
	AL2H_BaseCharacter* FindVisiblePlayer(float& OutDistance) const;

	void TickPatrol(float DeltaSeconds);
	void TickSuspicious(float DeltaSeconds);
	void TickInvestigate(float DeltaSeconds);
	void TickChase(float DeltaSeconds);
	void TickSearch(float DeltaSeconds);

	/** Starts a hunt on Who: chase state, and the robots around it are told. */
	void StartHunt(AL2H_BaseCharacter* Who);

	/** Sends it to look at Location (a noise, a glimpse, an alert). Ignored while hunting. */
	void Investigate(const FVector& Location);

	/** Walks to Goal: along the navmesh if the level has one, otherwise steering round what is ahead. True when there. */
	bool MoveToward(const FVector& Goal, float Speed, float AcceptRadius);
	void StopMoving();
	FVector SteerAround(const FVector& Direction) const;

	/** Turns in place toward Location at the movement component's rotation rate. */
	void FaceToward(const FVector& Location, float DeltaSeconds);

	// Grab (server state; the multicast plays it everywhere).
	enum class EGrabPhase : uint8 { Start, Escape, Bite, Cancel };
	void BeginGrab(AL2H_BaseCharacter* Victim);
	void TickGrab();
	void EndGrab(EGrabPhase How);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastGrab(AL2H_BaseCharacter* Victim, uint8 Phase, FVector_NetQuantize VictimLocation, float VictimYaw);
	UAnimSequenceBase* GetGrabClip(int32 Index) const { return GrabClips.IsValidIndex(Index) ? GrabClips[Index].Get() : nullptr; }
	void PlayGrabClipOnRobot(int32 Index, bool bLoop);
	void RestoreRobotAnimation();
	TWeakObjectPtr<AL2H_BaseCharacter> GrabVictim;
	float GrabEndTime = 0.0f;
	float NextGrabTime = 0.0f;
	/** Every machine: bumped per grab, so a timer from an earlier one does nothing. */
	int32 GrabSerial = 0;

	TWeakObjectPtr<AL2H_BaseCharacter> HuntTarget;
	TWeakObjectPtr<AL2H_HidingCloset> ClosetToOpen;
	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;
	FVector LastKnownLocation = FVector::ZeroVector;
	float LastSeenTime = -100.0f;
	float StateStartTime = 0.0f;
	int32 PatrolIndex = 0;
	float PatrolWaitUntil = 0.0f;
	float SearchTurnTime = 0.0f;
	float SearchYaw = 0.0f;
	FVector MoveGoal = FVector(TNumericLimits<float>::Max());
	bool bNoNavMesh = false;
	/** Progress check: closest it has been to the current goal, and when it last got closer. */
	FVector ProgressGoal = FVector(TNumericLimits<float>::Max());
	float BestGoalDistance = TNumericLimits<float>::Max();
	float LastProgressTime = 0.0f;
};
