// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ball/L2H_TimeBall.h"

#include "Ball/L2H_BallTypes.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/EnumProperty.h"

AL2H_TimeBall::AL2H_TimeBall()
{
	PrimaryActorTick.bCanEverTick = true;

	// The flight is resolved on the server, so that is the only machine that needs to poll it.
	bReplicates = true;
	SetReplicateMovement(true);

	BallCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BallCollision"));
	SetRootComponent(BallCollision);
	BallCollision->InitSphereRadius(16.0f);
	BallCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BallCollision->SetCollisionObjectType(ECC_PhysicsBody);
	BallCollision->SetCollisionResponseToAllChannels(ECR_Block);
	BallCollision->SetGenerateOverlapEvents(false);
	BallCollision->SetHiddenInGame(true);
	BallCollision->CanCharacterStepUpOn = ECB_No;

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(BallCollision);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetRelativeScale3D(FVector(0.32f));
	BallMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(BallCollision);
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->Friction = 0.2f;

	// Overwritten with the ability's limits in InitBall; these are only the constructor defaults.
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->Bounciness = 0.5f;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 120.0f;
}

void AL2H_TimeBall::BeginPlay()
{
	Super::BeginPlay();

	SetActorTickEnabled(HasAuthority());

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &AL2H_TimeBall::HandleProjectileStop);
		ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AL2H_TimeBall::HandleProjectileBounce);
	}
}

void AL2H_TimeBall::InitBall(AL2H_BaseCharacter* InThrower, const FL2H_BallLimits& InLimits, AL2H_TimeDirector* InDirector)
{
	Thrower = InThrower;
	Limits = InLimits;
	Director = InDirector;

	ThrowLocation = GetActorLocation();
	AirborneTime = 0.0f;
	bResolved = false;
	bReturning = false;
	bSettled = false;

	if (!ProjectileMovement)
	{
		return;
	}

	ProjectileMovement->InitialSpeed = Limits.ThrowSpeed;
	ProjectileMovement->MaxSpeed = FMath::Max(Limits.MaxSpeed, Limits.ThrowSpeed);
	ProjectileMovement->ProjectileGravityScale = Limits.GravityScale;
	ProjectileMovement->Bounciness = Limits.Bounciness;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = Limits.SettleVelocityThreshold;

	// The thrower must never be hit by his own ball: the spawn offset clears his capsule, and this
	// keeps a bounce off his own body from being reported as the first impact.
	if (Thrower && BallCollision)
	{
		BallCollision->IgnoreActorWhenMoving(Thrower, true);
	}

	// The direction comes from the controller on the server. A client-supplied vector is never
	// trusted, and in local play the two are the same thing anyway.
	const FVector Direction = Thrower ? Thrower->GetControlRotation().Vector() : GetActorForwardVector();

	ProjectileMovement->Velocity = Direction * Limits.ThrowSpeed;
	ProjectileMovement->Activate(true);

	L2HTime::LogVerbose(FString::Printf(TEXT("ChronoBall: thrown from %s toward %s"),
		*ThrowLocation.ToCompactString(), *Direction.ToCompactString()));
}

void AL2H_TimeBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bResolved)
	{
		return;
	}

	AirborneTime += DeltaSeconds;

	if (bReturning)
	{
		if (!Thrower)
		{
			Finish();
			return;
		}

		const FVector Current = GetActorLocation();
		const FVector ToThrower = Thrower->GetActorLocation() - Current;
		const float Step = FMath::Max(Limits.RecallSpeed, 1.0f) * DeltaSeconds;

		if (ToThrower.Size() <= FMath::Max(Step, Limits.RecallAcceptRadius))
		{
			L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s caught the ball back"), *GetNameSafe(Thrower)));
			Finish();
			return;
		}

		SetActorLocation(Current + ToThrower.GetSafeNormal() * Step, false);
		return;
	}

	if (Limits.BallLifetime > 0.0f && AirborneTime >= Limits.BallLifetime)
	{
		ResolveFailure(TEXT("the ball outlived BallLifetime"));
		return;
	}

	// Zero time mid-flight has to end the attempt: a downed player must not be teleported.
	if (Thrower && Thrower->IsDowned())
	{
		ResolveFailure(TEXT("the thrower went down while the ball was in the air"));
		return;
	}

	if (Limits.MaxRange > 0.0f && FVector::Dist(ThrowLocation, GetActorLocation()) > Limits.MaxRange)
	{
		ResolveFailure(FString::Printf(TEXT("the ball passed MaxRange (%.0f cm)"), Limits.MaxRange));
		return;
	}

	if (Limits.MaxFlightTime > 0.0f && AirborneTime >= Limits.MaxFlightTime)
	{
		ResolveFailure(FString::Printf(TEXT("the ball was still in the air after MaxFlightTime (%.1fs)"), Limits.MaxFlightTime));
		return;
	}
}

void AL2H_TimeBall::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bResolved)
	{
		return;
	}

	bSettled = true;

	if (Limits.Trigger == EL2H_BallTrigger::Manual)
	{
		L2HTime::LogVerbose(TEXT("ChronoBall: settled -- waiting for the manual trigger"));
		return;
	}

	if (Limits.Trigger == EL2H_BallTrigger::OnFirstImpact)
	{
		// Already resolved on the first bounce.
		return;
	}

	ResolveTeleport();
}

void AL2H_TimeBall::HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (!HasAuthority() || bResolved)
	{
		return;
	}

	if (Limits.Trigger != EL2H_BallTrigger::OnFirstImpact)
	{
		return;
	}

	ResolveTeleport();
}

void AL2H_TimeBall::HandleRepeatPress()
{
	if (!HasAuthority() || bResolved)
	{
		return;
	}

	if (Limits.RepeatPressBehavior == EL2H_BallRepeatBehavior::TeleportNow)
	{
		ResolveTeleport();
		return;
	}

	ResolveRecall();
}

void AL2H_TimeBall::ResolveTeleport()
{
	if (bResolved)
	{
		return;
	}

	bResolved = true;

	if (!Thrower)
	{
		Finish();
		return;
	}

	FString Reason;
	if (!IsParkourStateIdle(Reason) || !IsDestinationClear(Reason))
	{
		// Refused, so nothing has been half-applied: the ball goes back and the throw is refunded.
		bResolved = false;
		ResolveFailure(Reason);
		return;
	}

	PayCost(GetAirborneCost());

	const UCapsuleComponent* Capsule = Thrower->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Destination = GetActorLocation()
		+ FVector(0.0f, 0.0f, HalfHeight + FMath::Max(Limits.LandingClearanceAboveFeet, 0.0f));

	// bNoCheck stays false on purpose: TeleportTo is allowed to slide the landing spot to somewhere it
	// actually fits, and its return value is the only proof the move happened.
	const bool bTeleported = Thrower->TeleportTo(Destination, Thrower->GetActorRotation(), false, false);
	if (!bTeleported)
	{
		bResolved = false;
		ResolveFailure(TEXT("TeleportTo refused the landing spot"));
		return;
	}

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s teleported to %s"),
		*GetNameSafe(Thrower), *Destination.ToCompactString()));

	if (Limits.bDestroyBallAfterSuccess)
	{
		Finish();
		return;
	}

	// Left behind as a marker: it must not resolve a second time, and its lifetime is over.
	SetActorTickEnabled(false);
}

void AL2H_TimeBall::ResolveRecall()
{
	if (bResolved || bReturning)
	{
		return;
	}

	// The flight was still paid for. Only the teleport is abandoned.
	PayCost(GetAirborneCost());

	L2HTime::LogImportant(TEXT("ChronoBall: recalled -- the ball returns and the thrower stays put"));

	bReturning = true;

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	if (BallCollision)
	{
		BallCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AL2H_TimeBall::ResolveFailure(const FString& Reason)
{
	if (bResolved)
	{
		return;
	}

	bResolved = true;

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: refused (%s)"), *Reason));

	if (Limits.bRefundCostOnFailure)
	{
		Refund(Limits.CostFlat);
	}
	else
	{
		PayCost(GetAirborneCost());
	}

	Finish();
}

void AL2H_TimeBall::Finish()
{
	bResolved = true;

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	Destroy();
}

void AL2H_TimeBall::PayCost(float Seconds)
{
	if (!Thrower || Seconds <= 0.0f)
	{
		return;
	}

	AL2H_TimeDirector* TimeDirector = Director;
	if (!TimeDirector)
	{
		TimeDirector = AL2H_TimeDirector::Get(Thrower);
	}

	if (TimeDirector)
	{
		TimeDirector->ApplyTimeCost(Thrower, Seconds);
	}
}

void AL2H_TimeBall::Refund(float Seconds)
{
	if (!Thrower || Seconds <= 0.0f)
	{
		return;
	}

	AL2H_TimeDirector* TimeDirector = Director;
	if (!TimeDirector)
	{
		TimeDirector = AL2H_TimeDirector::Get(Thrower);
	}

	if (TimeDirector)
	{
		TimeDirector->ApplyTimeRestore(Thrower, Seconds);
	}
}

float AL2H_TimeBall::GetAirborneCost() const
{
	return FMath::Max(Limits.CostPerSecondAirborne, 0.0f) * AirborneTime;
}

bool AL2H_TimeBall::IsDestinationClear(FString& OutReason) const
{
	if (!Limits.bRequireClearLandingSpot || !Thrower)
	{
		return true;
	}

	const UCapsuleComponent* Capsule = Thrower->GetCapsuleComponent();
	UWorld* World = GetWorld();

	if (!Capsule || !World)
	{
		return true;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Center = GetActorLocation()
		+ FVector(0.0f, 0.0f, HalfHeight + FMath::Max(Limits.LandingClearanceAboveFeet, 0.0f));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HChronoBallLanding), false, Thrower);
	Params.AddIgnoredActor(this);

	const bool bBlocked = World->OverlapBlockingTestByChannel(
		Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	if (bBlocked)
	{
		OutReason = TEXT("the space a player would occupy there is blocked");
		return false;
	}

	return true;
}

bool AL2H_TimeBall::IsParkourStateIdle(FString& OutReason) const
{
	if (!Limits.bBlockOnParkourState || !Thrower)
	{
		return true;
	}

	for (UActorComponent* Component : Thrower->GetComponents())
	{
		if (!Component)
		{
			continue;
		}

		const UClass* ComponentClass = Component->GetClass();
		if (!ComponentClass || !ComponentClass->GetName().Contains(TEXT("DynamicParkourTraceSystem")))
		{
			continue;
		}

		// Busy flags first: they are the cheapest thing to read and the ones a transition sets.
		static const FName BusyNames[] = { FName(TEXT("Busy")), FName(TEXT("bIsTransitioning")), FName(TEXT("IsTransitioning")) };
		for (const FName& PropertyName : BusyNames)
		{
			const FBoolProperty* Flag = CastField<FBoolProperty>(ComponentClass->FindPropertyByName(PropertyName));
			if (Flag && Flag->GetPropertyValue_InContainer(Component))
			{
				OutReason = FString::Printf(TEXT("the parkour component is busy (%s)"), *PropertyName.ToString());
				return false;
			}
		}

		const FProperty* StateProperty = ComponentClass->FindPropertyByName(FName(TEXT("ActiveParkourState")));
		if (!StateProperty)
		{
			// Nothing to read: the component is present but its state is not exposed the way we expect.
			// Continuing is deliberate -- an unreadable state is not an active one.
			continue;
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(StateProperty))
		{
			const FNumericProperty* Underlying = EnumProperty->GetUnderlyingProperty();
			const UEnum* StateEnum = EnumProperty->GetEnum();

			if (Underlying && StateEnum)
			{
				const int64 StateValue = Underlying->GetSignedIntPropertyValue_InContainer(Component);
				const FString StateName = StateEnum->GetNameStringByValue(StateValue);

				if (!StateName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
				{
					OutReason = FString::Printf(TEXT("the parkour state is %s, not None"), *StateName);
					return false;
				}
			}
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(StateProperty))
		{
			// A byte-backed enum with no type information: the idle entry is the first one by convention.
			if (ByteProperty->GetPropertyValue_InContainer(Component) != 0)
			{
				OutReason = TEXT("the parkour state is not the idle entry");
				return false;
			}
		}
	}

	return true;
}

AL2H_TimeBall* AL2H_TimeBall::FindForOwner(const UObject* WorldContextObject, const AActor* Thrower)
{
	if (!WorldContextObject || !Thrower)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject, AL2H_TimeBall::StaticClass(), Found);

	for (AActor* Actor : Found)
	{
		AL2H_TimeBall* Ball = Cast<AL2H_TimeBall>(Actor);
		if (Ball && !Ball->IsResolved() && Ball->GetThrower() == Thrower)
		{
			return Ball;
		}
	}

	return nullptr;
}
