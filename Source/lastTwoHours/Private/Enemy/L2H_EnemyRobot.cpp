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
#include "World/L2H_HidingCloset.h"
#include "Character/L2H_FlashlightComponent.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

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

	// The eye: a small light on the head whose colour says what it is doing.
	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(GetMesh(), TEXT("head"));
	EyeLight->SetRelativeLocation(FVector(0.0f, 14.0f, 6.0f));
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(20.0f);
	EyeLight->SetAttenuationRadius(140.0f);
	EyeLight->SetCastShadows(false);
	EyeLight->SetLightColor(FLinearColor(0.25f, 0.6f, 1.0f));
}

void AL2H_EnemyRobot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_EnemyRobot, Health);
	DOREPLIFETIME(AL2H_EnemyRobot, bDead);
	DOREPLIFETIME(AL2H_EnemyRobot, bStunned);
	DOREPLIFETIME(AL2H_EnemyRobot, State);
	DOREPLIFETIME(AL2H_EnemyRobot, Awareness);
}

void AL2H_EnemyRobot::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
	HomeLocation = GetActorLocation();
	HomeRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
	OnRep_State();
}

AL2H_BaseCharacter* AL2H_EnemyRobot::FindVisiblePlayer(float& OutDistance) const
{
	const AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	UWorld* World = GetWorld();
	OutDistance = TNumericLimits<float>::Max();
	if (!Director || !World)
	{
		return nullptr;
	}

	const FVector Eye = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float ViewCos = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

	AL2H_BaseCharacter* Best = nullptr;
	for (AL2H_BaseCharacter* Candidate : Director->GetRegisteredPawns())
	{
		// A body on the floor or on someone's shoulders is left alone: the robot goes for whoever can
		// still fight back, which is also what keeps a revive from being an endless stun-lock.
		if (!Candidate || Candidate->IsDowned() || Candidate->IsDead() || Candidate->IsBeingCarried()
			|| AL2H_HidingCloset::IsHiding(Candidate))
		{
			continue;
		}

		const UL2H_FlashlightComponent* Lamp = Candidate->FindComponentByClass<UL2H_FlashlightComponent>();
		const bool bLit = Lamp && Lamp->IsOn();
		float Range = SightRange * (Candidate->bIsCrouched ? CrouchSightScale : 1.0f);
		if (bLit)
		{
			Range = SightRange * FlashlightSightScale;
		}

		const FVector To = Candidate->GetActorLocation() - GetActorLocation();
		const float Distance = To.Size();
		if (Distance > Range || Distance >= OutDistance)
		{
			continue;
		}

		// In the cone, or close enough to be felt, or carrying a light it can see from any side.
		const bool bInView = FVector::DotProduct(Forward, To.GetSafeNormal2D()) >= ViewCos;
		if (!bInView && !bLit && Distance > CloseSenseRange)
		{
			continue;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HEnemySight), false, this);
		Params.AddIgnoredActor(Candidate);
		if (World->LineTraceTestByChannel(Eye, Candidate->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, Params)
			&& Distance > CloseSenseRange)
		{
			continue;
		}

		Best = Candidate;
		OutDistance = Distance;
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
		bWindingUp = false;
		StopMoving();
		return;
	}

	if (bWindingUp)
	{
		// Committed: no turning, no stepping. The blow lands where it was aimed, not where the player went.
		const UWorld* WindWorld = GetWorld();
		if (WindWorld && WindWorld->GetTimeSeconds() >= StrikeTime)
		{
			Strike();
		}
		return;
	}

	UpdateSenses(DeltaSeconds);

	switch (State)
	{
	case EL2H_RobotState::Patrol:
	case EL2H_RobotState::Return:
		TickPatrol(DeltaSeconds);
		break;
	case EL2H_RobotState::Suspicious:
		TickSuspicious(DeltaSeconds);
		break;
	case EL2H_RobotState::Investigate:
		TickInvestigate(DeltaSeconds);
		break;
	case EL2H_RobotState::Chase:
		TickChase(DeltaSeconds);
		break;
	case EL2H_RobotState::Search:
		TickSearch(DeltaSeconds);
		break;
	}
}

void AL2H_EnemyRobot::UpdateSenses(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	float Distance = 0.0f;
	AL2H_BaseCharacter* Seen = FindVisiblePlayer(Distance);

	if (State == EL2H_RobotState::Chase)
	{
		// Hunting: it follows the one it hunts while it can see them, and switches only to someone it sees
		// when it has lost them.
		AL2H_BaseCharacter* Hunted = HuntTarget.Get();
		if (Seen && (Seen == Hunted || !Hunted || Now - LastSeenTime > LoseSightSeconds))
		{
			HuntTarget = Seen;
			LastKnownLocation = Seen->GetActorLocation();
			LastSeenTime = Now;
		}
		Awareness = 1.0f;
		return;
	}

	if (Seen)
	{
		// The closer, the quicker it is sure: right in front of it almost at once, at the far end of its
		// sight several times NoticeSeconds.
		const float Range = FMath::Max(SightRange, 1.0f);
		const float Seconds = FMath::Lerp(0.15f, NoticeSeconds * 2.0f, FMath::Clamp(Distance / Range, 0.0f, 1.0f));
		Awareness = FMath::Min(1.0f, Awareness + DeltaSeconds / FMath::Max(Seconds, 0.05f));
		HuntTarget = Seen;
		LastKnownLocation = Seen->GetActorLocation();
		LastSeenTime = Now;

		if (Awareness >= 1.0f)
		{
			StartHunt(Seen);
		}
		else if (State != EL2H_RobotState::Suspicious)
		{
			SetState(EL2H_RobotState::Suspicious);
		}
		return;
	}

	Awareness = FMath::Max(0.0f, Awareness - AwarenessDecay * DeltaSeconds);

	// Footsteps: a player running close by is heard, wherever they are.
	if (const AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		for (AL2H_BaseCharacter* Candidate : Director->GetRegisteredPawns())
		{
			if (Candidate && !Candidate->IsDowned() && !Candidate->IsDead() && !Candidate->bIsCrouched
				&& Candidate->GetVelocity().Size2D() > RunNoiseSpeed && Candidate->GetCharacterMovement()->IsMovingOnGround()
				&& FVector::Dist(Candidate->GetActorLocation(), GetActorLocation()) < RunHearingRange)
			{
				Investigate(Candidate->GetActorLocation());
				break;
			}
		}
	}
}

void AL2H_EnemyRobot::SetState(EL2H_RobotState NewState)
{
	if (State == NewState)
	{
		return;
	}
	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s %s -> %s (awareness %.2f)"), *GetNameSafe(this),
		*UEnum::GetValueAsString(State), *UEnum::GetValueAsString(NewState), Awareness));
	State = NewState;
	StateStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	MoveGoal = FVector(TNumericLimits<float>::Max());
	if (NewState == EL2H_RobotState::Search)
	{
		SearchTurnTime = StateStartTime;
		SearchYaw = GetActorRotation().Yaw;
	}
	OnRep_State();
}

void AL2H_EnemyRobot::StartHunt(AL2H_BaseCharacter* Who)
{
	if (!Who)
	{
		return;
	}
	const bool bWasHunting = State == EL2H_RobotState::Chase;
	HuntTarget = Who;
	Awareness = 1.0f;
	LastKnownLocation = Who->GetActorLocation();
	LastSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetState(EL2H_RobotState::Chase);

	if (!bWasHunting && AlertRadius > 0.0f && GetWorld())
	{
		for (TActorIterator<AL2H_EnemyRobot> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsDead() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= AlertRadius)
			{
				It->HearAlert(LastKnownLocation);
			}
		}
	}
}

void AL2H_EnemyRobot::HearAlert(const FVector& Location)
{
	Investigate(Location);
	// Told by another robot: halfway to sure, so a glimpse finishes the job.
	Awareness = FMath::Max(Awareness, 0.5f);
}

void AL2H_EnemyRobot::Investigate(const FVector& Location)
{
	if (bDead || State == EL2H_RobotState::Chase)
	{
		return;
	}
	LastKnownLocation = Location;
	if (State == EL2H_RobotState::Investigate)
	{
		MoveGoal = FVector(TNumericLimits<float>::Max());
		return;
	}
	SetState(EL2H_RobotState::Investigate);
}

void AL2H_EnemyRobot::ReportNoise(const UObject* WorldContext, const FVector& Location, float Radius)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || Radius <= 0.0f)
	{
		return;
	}
	for (TActorIterator<AL2H_EnemyRobot> It(World); It; ++It)
	{
		if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), Location) <= Radius)
		{
			It->Investigate(Location);
		}
	}
}

void AL2H_EnemyRobot::TickPatrol(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	if (PatrolPoints.Num() == 0)
	{
		// A guard: back to its post, then facing the way it was placed.
		if (MoveToward(HomeLocation, PatrolSpeed, 60.0f))
		{
			if (State == EL2H_RobotState::Return)
			{
				SetState(EL2H_RobotState::Patrol);
			}
			FaceToward(GetActorLocation() + HomeRotation.Vector() * 100.0f, DeltaSeconds);
		}
		return;
	}

	if (Now < PatrolWaitUntil)
	{
		return;
	}
	PatrolIndex = PatrolIndex % PatrolPoints.Num();
	const FVector Point = HomeLocation + HomeRotation.RotateVector(PatrolPoints[PatrolIndex]);
	if (MoveToward(Point, PatrolSpeed, 70.0f))
	{
		if (State == EL2H_RobotState::Return)
		{
			SetState(EL2H_RobotState::Patrol);
		}
		PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
		PatrolWaitUntil = Now + PatrolWait;
	}
}

void AL2H_EnemyRobot::TickSuspicious(float DeltaSeconds)
{
	// Stops and stares at what it glimpsed while its awareness fills or drains.
	StopMoving();
	FaceToward(LastKnownLocation, DeltaSeconds);

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastSeenTime > 0.6f)
	{
		// Gone from view: a good look is worth walking over for, a flicker is forgotten.
		if (Awareness >= 0.35f)
		{
			SetState(EL2H_RobotState::Investigate);
		}
		else if (Awareness <= 0.0f)
		{
			SetState(EL2H_RobotState::Return);
		}
	}
}

void AL2H_EnemyRobot::TickInvestigate(float DeltaSeconds)
{
	if (MoveToward(LastKnownLocation, InvestigateSpeed, 150.0f))
	{
		SetState(EL2H_RobotState::Search);
	}
	// A walk that cannot get there (a wall, no way round) ends in a look around where it stands.
	else if (GetWorld() && GetWorld()->GetTimeSeconds() - StateStartTime > 15.0f)
	{
		SetState(EL2H_RobotState::Search);
	}
}

void AL2H_EnemyRobot::TickSearch(float DeltaSeconds)
{
	StopMoving();
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// Looks one way, then the other, then behind.
	if (Now >= SearchTurnTime)
	{
		static const float Turns[] = { 70.0f, -140.0f, 180.0f, -110.0f };
		const int32 Step = FMath::FloorToInt((Now - StateStartTime) / 1.4f);
		SearchYaw = GetActorRotation().Yaw + Turns[Step % UE_ARRAY_COUNT(Turns)];
		SearchTurnTime = Now + 1.4f;
	}
	FaceToward(GetActorLocation() + FRotator(0.0f, SearchYaw, 0.0f).Vector() * 100.0f, DeltaSeconds);

	if (Now - StateStartTime > SearchSeconds)
	{
		Awareness = 0.0f;
		SetState(EL2H_RobotState::Return);
	}
}

void AL2H_EnemyRobot::TickChase(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	AL2H_BaseCharacter* Hunted = HuntTarget.Get();

	if (!Hunted || Hunted->IsDowned() || Hunted->IsDead() || Hunted->IsBeingCarried())
	{
		HuntTarget.Reset();
		SetState(EL2H_RobotState::Search);
		return;
	}

	// It watched them step into a closet: it goes to the doors and pulls them out.
	if (AL2H_HidingCloset::IsHiding(Hunted))
	{
		AL2H_HidingCloset* Closet = AL2H_HidingCloset::FindClosetOf(Hunted);
		if (Closet && Now - LastSeenTime <= LoseSightSeconds + 1.0f)
		{
			ClosetToOpen = Closet;
		}
	}
	if (AL2H_HidingCloset* Closet = ClosetToOpen.Get())
	{
		if (!AL2H_HidingCloset::IsHiding(Hunted))
		{
			ClosetToOpen.Reset();
		}
		else
		{
			LastSeenTime = Now;
			const FVector Front = Closet->GetFrontPoint(70.0f);
			if (MoveToward(Front, ChaseSpeed, 100.0f))
			{
				FaceToward(Closet->GetActorLocation(), DeltaSeconds);
				Closet->ForceOpen();
				ClosetToOpen.Reset();
			}
			return;
		}
	}

	if (Now - LastSeenTime > LoseSightSeconds)
	{
		// Lost them: to where they were last seen, then a look around.
		Awareness = 0.75f;
		SetState(EL2H_RobotState::Investigate);
		return;
	}

	const FVector ToTarget = Hunted->GetActorLocation() - GetActorLocation();
	const float Distance = ToTarget.Size2D();
	if (Distance > AttackRange * 0.85f)
	{
		MoveToward(Hunted->GetActorLocation(), ChaseSpeed, AttackRange * 0.7f);
		return;
	}

	// In reach: face the player and swing on the cooldown.
	StopMoving();
	SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));

	if (Now < NextAttackTime || Distance > AttackRange || FMath::Abs(ToTarget.Z) > AttackHeightReach)
	{
		return;
	}

	// The swing is announced first: it stops, leans back and trembles for AttackWindup seconds, and only then
	// does the blow land -- on whoever is still in front of it, in reach and in sight. A step aside, a dodge
	// (its i-frames), or a climb out of reach all get away from it.
	NextAttackTime = Now + AttackWindup + AttackInterval;
	bWindingUp = true;
	StrikeTime = Now + AttackWindup;
	WindupTarget = Hunted;
	GetCharacterMovement()->StopMovementImmediately();
	MulticastWindup(AttackWindup);
}

bool AL2H_EnemyRobot::MoveToward(const FVector& Goal, float Speed, float AcceptRadius)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	if (FVector::DistSquared2D(Goal, GetActorLocation()) <= FMath::Square(AcceptRadius))
	{
		StopMoving();
		return true;
	}

	// Blocked short of the goal (a crate in the way, a ledge it cannot take): when it has not gained ground
	// for two seconds it counts as there, so the next step of its routine still happens.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float GoalDistance = FVector::Dist2D(Goal, GetActorLocation());
	if (FVector::DistSquared(Goal, ProgressGoal) > FMath::Square(80.0f) || GoalDistance < BestGoalDistance - 25.0f)
	{
		ProgressGoal = Goal;
		BestGoalDistance = GoalDistance;
		LastProgressTime = Now;
	}
	else if (Now - LastProgressTime > 2.0f)
	{
		BestGoalDistance = TNumericLimits<float>::Max();
		LastProgressTime = Now;
		StopMoving();
		return true;
	}

	// The navmesh when the level has one: re-asked only when the goal has moved, so the path is followed.
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && !bNoNavMesh)
	{
		if (AI->GetMoveStatus() == EPathFollowingStatus::Moving && FVector::DistSquared(Goal, MoveGoal) <= FMath::Square(80.0f))
		{
			return false;
		}
		MoveGoal = Goal;
		const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Goal, AcceptRadius * 0.8f, /*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/true, /*bProjectDestinationToNavigation=*/true, /*bCanStrafe=*/false, nullptr, /*bAllowPartialPath=*/true);
		if (Result != EPathFollowingRequestResult::Failed)
		{
			return Result == EPathFollowingRequestResult::AlreadyAtGoal;
		}
		// No navmesh in this level: steer by hand from now on. (With one, a goal off the mesh is steered to directly.)
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		bNoNavMesh = !Nav || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	}

	AddMovementInput(SteerAround((Goal - GetActorLocation()).GetSafeNormal2D()), 1.0f);
	return false;
}

void AL2H_EnemyRobot::StopMoving()
{
	MoveGoal = FVector(TNumericLimits<float>::Max());
	if (AAIController* AI = Cast<AAIController>(GetController()); AI && AI->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		AI->StopMovement();
	}
}

FVector AL2H_EnemyRobot::SteerAround(const FVector& Direction) const
{
	// Whiskers at knee height: straight on if clear, otherwise the nearest clear angle either side.
	UWorld* World = GetWorld();
	if (!World || Direction.IsNearlyZero())
	{
		return Direction;
	}
	const FVector Start = GetActorLocation() - FVector(0.0f, 0.0f, 40.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HEnemySteer), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	static const float Angles[] = { 0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 125.0f, -125.0f };
	for (const float Angle : Angles)
	{
		const FVector Dir = Direction.RotateAngleAxis(Angle, FVector::UpVector);
		if (!World->SweepTestByObjectType(Start, Start + Dir * 140.0f, FQuat::Identity, Objects, FCollisionShape::MakeSphere(30.0f), Params))
		{
			return Dir;
		}
	}
	return Direction;
}

void AL2H_EnemyRobot::FaceToward(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return;
	}
	const FRotator Current = GetActorRotation();
	const FRotator Wanted(0.0f, To.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpConstantTo(Current, Wanted, DeltaSeconds, GetCharacterMovement()->RotationRate.Yaw));
}

void AL2H_EnemyRobot::OnRep_State()
{
	if (!EyeLight)
	{
		return;
	}
	FLinearColor Colour(0.25f, 0.6f, 1.0f);
	switch (State)
	{
	case EL2H_RobotState::Suspicious:
	case EL2H_RobotState::Investigate:
	case EL2H_RobotState::Search:
		Colour = FLinearColor(1.0f, 0.75f, 0.1f);
		break;
	case EL2H_RobotState::Chase:
		Colour = FLinearColor(1.0f, 0.08f, 0.05f);
		break;
	default:
		break;
	}
	EyeLight->SetLightColor(Colour);
	EyeLight->SetIntensity(State == EL2H_RobotState::Chase ? 40.0f : 20.0f);
}

void AL2H_EnemyRobot::Strike()
{
	bWindingUp = false;
	AL2H_BaseCharacter* Victim = WindupTarget.Get();
	WindupTarget.Reset();
	UWorld* World = GetWorld();
	if (!Victim || !World || Victim->IsDowned() || Victim->IsDead() || AL2H_HidingCloset::IsHiding(Victim))
	{
		return;
	}

	const FVector ToTarget = Victim->GetActorLocation() - GetActorLocation();
	const float Facing = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToTarget.GetSafeNormal2D());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HEnemyStrike), false, this);
	Params.AddIgnoredActor(Victim);
	const FVector Eye = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const bool bBlocked = World->LineTraceTestByChannel(Eye, Victim->GetActorLocation(), ECC_Visibility, Params);

	const bool bInReach = ToTarget.Size2D() <= AttackRange && FMath::Abs(ToTarget.Z) <= AttackHeightReach;
	const bool bLands = bInReach && Facing >= AttackConeCos && !bBlocked;

	MulticastAttack(Victim);

	if (!bLands)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s missed %s (dist %.0f, dz %.0f, facing %.2f, blocked %d)"),
			*GetNameSafe(this), *GetNameSafe(Victim), ToTarget.Size2D(), ToTarget.Z, Facing, bBlocked ? 1 : 0));
		return;
	}

	// Through the director, like a bullet: invulnerability, the health layer and the time it costs all
	// apply without the robot knowing about any of them.
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->ApplyTimeDamage(Victim, AttackDamage);
	}
	L2HTime::LogImportant(FString::Printf(TEXT("Enemy: %s hit %s for %.0f"), *GetNameSafe(this), *GetNameSafe(Victim), AttackDamage));
}

void AL2H_EnemyRobot::MulticastWindup_Implementation(float Seconds)
{
	const UWorld* World = GetWorld();
	WindupLookUntil = World ? World->GetTimeSeconds() + Seconds : -1.0f;
}

void AL2H_EnemyRobot::MulticastAttack_Implementation(AActor* Target)
{
	// No attack clip ships with the pack, so the swing reads as a short lunge along where it was aimed.
	if (HasAuthority() && Target)
	{
		LaunchCharacter(GetActorForwardVector().GetSafeNormal2D() * 300.0f, true, false);
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
	else if (AL2H_BaseCharacter* Shooter = EventInstigator ? Cast<AL2H_BaseCharacter>(EventInstigator->GetPawn()) : nullptr)
	{
		// Hit: it knows at once who did it and where they stand.
		StartHunt(Shooter);
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
	StopMoving();

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
	float Rock = (bStunned && !bDead && World) ? FMath::Sin(World->GetTimeSeconds() * 14.0f) * 7.0f : 0.0f;
	// A swing on its way: leaning back and trembling until the blow.
	if (World && !bDead && World->GetTimeSeconds() < WindupLookUntil)
	{
		Rock = -10.0f + FMath::Sin(World->GetTimeSeconds() * 60.0f) * 2.5f;
	}
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
	if (EyeLight)
	{
		EyeLight->SetVisibility(false);
	}
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
