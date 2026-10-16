// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/L2H_EnemyRobot.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "UObject/ConstructorHelpers.h"

AL2H_EnemyRobot::AL2H_EnemyRobot()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Any controller turns AddMovementInput into movement; a plain AI controller is all the steering needs.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(40.0f, 90.0f);

	// Shots trace on Visibility. The capsule answers them so a hit never slips between the limbs.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// The pack's own mesh and its demo animation graph, which reads speed and air state off any pawn.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotMesh(TEXT("/Game/Robot_scout_R_21/Mesh/SK_Robot_scout_R21.SK_Robot_scout_R21"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> RobotAnim(TEXT("/Game/Robot_scout_R_21/Demo/Animations/ThirdPerson_AnimBP"));

	if (RobotMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(RobotMesh.Object);
	}
	if (RobotAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(RobotAnim.Class);
	}

	// UE4 mannequin convention: feet at the capsule bottom, facing +X.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	Move->MaxWalkSpeed = 380.0f;
	bUseControllerRotationYaw = false;
}

void AL2H_EnemyRobot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_EnemyRobot, Health);
	DOREPLIFETIME(AL2H_EnemyRobot, bDead);
	DOREPLIFETIME(AL2H_EnemyRobot, bStunned);
}

void AL2H_EnemyRobot::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

AL2H_BaseCharacter* AL2H_EnemyRobot::FindTarget() const
{
	const AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		return nullptr;
	}

	AL2H_BaseCharacter* Best = nullptr;
	float BestDistanceSq = FMath::Square(SightRange);

	for (AL2H_BaseCharacter* Candidate : Director->GetRegisteredPawns())
	{
		// A body on the floor or on someone's shoulders is left alone: the robot goes for whoever can
		// still fight back, which is also what keeps a revive from being an endless stun-lock.
		if (!Candidate || Candidate->IsDowned() || Candidate->IsDead() || Candidate->IsBeingCarried())
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	return Best;
}

void AL2H_EnemyRobot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateStunLook();

	if (!HasAuthority() || bDead)
	{
		return;
	}

	if (bStunned)
	{
		const UWorld* StunWorld = GetWorld();
		if (StunWorld && StunWorld->GetTimeSeconds() >= StunEndTime)
		{
			bStunned = false;
		}
		return;
	}

	AL2H_BaseCharacter* Target = FindTarget();
	if (!Target)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Distance = ToTarget.Size2D();

	if (Distance > AttackRange * 0.85f)
	{
		// Straight at the player. Without a navmesh this walks into walls, which is fine for a test dummy
		// in open test levels and keeps the robot honest about where it is going.
		AddMovementInput(ToTarget.GetSafeNormal2D(), 1.0f);
		return;
	}

	// In reach: face the player and swing on the cooldown.
	SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now < NextAttackTime || Distance > AttackRange)
	{
		return;
	}

	NextAttackTime = Now + AttackInterval;

	// Through the director, like a bullet: invulnerability, the health layer and the time it costs all
	// apply without the robot knowing about any of them.
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->ApplyTimeDamage(Target, AttackDamage);
	}

	MulticastAttack(Target);

	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s hit %s for %.0f"), *GetNameSafe(this), *GetNameSafe(Target), AttackDamage));
}

void AL2H_EnemyRobot::MulticastAttack_Implementation(AActor* Target)
{
	// No attack clip ships with the pack, so the swing reads as a short lunge toward the target.
	if (HasAuthority() && Target)
	{
		LaunchCharacter((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 300.0f + FVector(0.0f, 0.0f, 150.0f), true, true);
	}

	OnAttack(Target);
}

float AL2H_EnemyRobot::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!HasAuthority() || bDead || DamageAmount <= 0.0f)
	{
		return Applied;
	}

	Health = FMath::Max(0.0f, Health - DamageAmount);

	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s took %.0f (%.0f left)"), *GetNameSafe(this), DamageAmount, Health));

	if (Health <= 0.0f)
	{
		Die();
	}

	return DamageAmount;
}

void AL2H_EnemyRobot::Die()
{
	if (!HasAuthority() || bDead)
	{
		return;
	}

	bDead = true;
	ApplyDeathPhysics();

	// Part of what the fight cost comes back: the shards wait by the wreck for whoever walks over.
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (Director && Config)
	{
		for (int32 Index = 0; Index < Config->EnemyShardCount; ++Index)
		{
			Director->SpillShardAt(this, Config->EnemyShardValue);
		}
	}

	if (CorpseLifetime > 0.0f)
	{
		SetLifeSpan(CorpseLifetime);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s destroyed"), *GetNameSafe(this)));
}

void AL2H_EnemyRobot::Stun(float Seconds)
{
	const UWorld* World = GetWorld();
	if (!HasAuthority() || bDead || !World || Seconds <= 0.0f)
	{
		return;
	}
	bStunned = true;
	StunEndTime = FMath::Max(StunEndTime, World->GetTimeSeconds() + Seconds);
	GetCharacterMovement()->StopMovementImmediately();
	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s stunned for %.1fs"), *GetNameSafe(this), Seconds));
}

void AL2H_EnemyRobot::UpdateStunLook()
{
	// The mesh's authored rotation is (0, -90, 0); a stun rocks it about the forward axis.
	const UWorld* World = GetWorld();
	const float Rock = (bStunned && !bDead && World) ? FMath::Sin(World->GetTimeSeconds() * 14.0f) * 7.0f : 0.0f;
	if (!bDead)
	{
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, Rock));
	}
}

void AL2H_EnemyRobot::FinishBy(AActor* Attacker)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	const FVector Away = Attacker ? (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	Health = 0.0f;
	Die();
	MulticastThrow(Away * 700.0f + FVector(0.0f, 0.0f, 350.0f));
	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s finished by %s"), *GetNameSafe(this), *GetNameSafe(Attacker)));
}

void AL2H_EnemyRobot::MulticastThrow_Implementation(FVector_NetQuantize Velocity)
{
	// On every machine after the ragdoll is on, so the wreck flies the same way everywhere.
	ApplyDeathPhysics();
	GetMesh()->SetAllPhysicsLinearVelocity(FVector(Velocity));
}

void AL2H_EnemyRobot::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeathPhysics();
	}
}

void AL2H_EnemyRobot::ApplyDeathPhysics()
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// The capsule stops blocking so the players walk over the wreck; the mesh falls as a ragdoll.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
}
