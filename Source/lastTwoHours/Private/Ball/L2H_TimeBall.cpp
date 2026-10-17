// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ball/L2H_TimeBall.h"

#include "Animation/AnimMontage.h"
#include "Ball/L2H_BallTypes.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_WorldTimeSubsystem.h"
#include "Time/L2H_TimeDirector.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/EnumProperty.h"
#include "VFX/L2H_VFXTypes.h"

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

	// Every machine ticks: the flight is still resolved on the server only, but the held orb's float
	// above the hand is drawn by each machine for itself.
	SetActorTickEnabled(true);

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

void AL2H_TimeBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The hold is the one piece of this actor's state a client cannot work out for itself: the flight
	// replicates through the movement component, but being in the hand is not movement.
	DOREPLIFETIME(AL2H_TimeBall, bHeld);
}

void AL2H_TimeBall::HoldInHand(AL2H_BaseCharacter* InThrower, const FL2H_BallLimits& InLimits, AL2H_TimeDirector* InDirector)
{
	Thrower = InThrower;
	Limits = InLimits;
	Director = InDirector;

	// Nothing about the throw has happened yet, so the flight state starts clean: an orb that arrives
	// here after a recall must not inherit the previous attempt's clock or its settled flag.
	AirborneTime = 0.0f;
	bSettled = false;
	bReturning = false;
	bResolved = false;

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	// Held, the orb is not a solid: it must not block its own holder, be hit by anything, or report a
	// landing it did not make. Collision comes back on the launch.
	if (BallCollision)
	{
		BallCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	bHeld = true;

	// The pose that reads as the spell being held in front of him. Played from here rather than from
	// the ability because the ball, not the ability, is what knows when the hold begins -- the taking
	// out pays and ends, and the orb outlives it.
	if (Thrower)
	{
		// A montage whose sections already loop (summon, then a hold that links to itself) is left to its
		// own links; re-arming its first section would replay the summon forever.
		const bool bAuthoredLoop = Limits.HoldMontage && Limits.HoldMontage->CompositeSections.Num() > 1;
		Thrower->MulticastPlayCharacterMontage(Limits.HoldMontage, 1.0f, /*bLoop=*/!bAuthoredLoop);
	}
	HeldSeconds = 0.0f;

	// Done here as well as in OnRep_Held. A RepNotify never fires on the authority, so the server --
	// the machine the player is really holding the orb for -- has to attach itself.
	AttachToHolder();

	ThrowLocation = GetActorLocation();

	L2HTime::LogVerbose(FString::Printf(TEXT("ChronoBall: %s took the orb out"), *GetNameSafe(InThrower)));
}

void AL2H_TimeBall::LaunchFromHold()
{
	if (!bHeld)
	{
		return;
	}

	bHeld = false;

	// The release replaces the hold pose with the throw. Stopping the hold first keeps the two from
	// blending against each other, and the launch clip is played even when no hold was authored.
	if (Thrower)
	{
		Thrower->MulticastStopCharacterMontage(Limits.HoldMontage, 0.15f);
		Thrower->MulticastPlayCharacterMontage(Limits.LaunchMontage, 1.0f, /*bLoop=*/false);
	}

	// Kept in world space on purpose: the orb leaves from exactly where it was hanging, which is what
	// makes the arc drawn while aiming the arc it actually takes.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (BallCollision)
	{
		BallCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (Thrower)
		{
			BallCollision->IgnoreActorWhenMoving(Thrower, true);
		}
	}

	// The direction is read on the server, from the server's copy of the control rotation, exactly as
	// it is for a throw that never went through the hand.
	const FVector Direction = Thrower ? Thrower->GetControlRotation().Vector() : GetActorForwardVector();

	ThrowLocation = GetActorLocation();
	AirborneTime = 0.0f;
	bSettled = false;
	bReturning = false;

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Limits.ThrowSpeed;
		ProjectileMovement->MaxSpeed = FMath::Max(Limits.MaxSpeed, Limits.ThrowSpeed);
		ProjectileMovement->ProjectileGravityScale = Limits.GravityScale;
		ProjectileMovement->Bounciness = Limits.Bounciness;
		ProjectileMovement->BounceVelocityStopSimulatingThreshold = Limits.SettleVelocityThreshold;

		ProjectileMovement->Velocity = Direction * Limits.ThrowSpeed;
		ProjectileMovement->Activate(true);
	}

	if (Thrower)
	{
		Thrower->MulticastMomentVFX(EL2H_VFXMoment::ChronoBall, NAME_None);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s launched the orb"), *GetNameSafe(Thrower)));
}

void AL2H_TimeBall::HandleLaunchPress()
{
	if (!HasAuthority() || bResolved)
	{
		return;
	}

	// One button, one meaning: commit to where the orb is. In the hand that is the throw; already out
	// it is the teleport. That is what lets the ability be a single press rather than a mode.
	if (bHeld)
	{
		LaunchFromHold();
		return;
	}

	// A field orb commits to nothing once it is out: where it lands is the whole of its effect, so the
	// second meaning of this button -- travel to the orb -- does not exist for it. The throw above is
	// untouched, which is why this sits after the hold check rather than at the top.
	if (IsFieldEnabled())
	{
		return;
	}

	ResolveTeleport();
}

void AL2H_TimeBall::PutAway()
{
	if (bResolved)
	{
		return;
	}

	bHeld = false;
	bResolved = true;

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s put the orb away"), *GetNameSafe(Thrower)));

	// The throw never happened, so the flat cost the hold paid goes straight back. There is nothing
	// airborne to settle, which is why this refunds the flat cost and not the airborne one.
	Refund(Limits.CostFlat);

	Finish();
}

void AL2H_TimeBall::OnRep_Held()
{
	HeldSeconds = 0.0f;
	AttachToHolder();
}

void AL2H_TimeBall::UpdateHeldFloat(float DeltaSeconds)
{
	const AL2H_BaseCharacter* Holder = Thrower ? Thrower.Get() : Cast<AL2H_BaseCharacter>(GetOwner());
	const USkeletalMeshComponent* HolderMesh = Holder ? Holder->GetMesh() : nullptr;
	if (!HolderMesh || GetAttachParentActor() != Holder)
	{
		return;
	}

	// Straight up in the world, not along the hand: the hand turns through the summon, and the orb
	// should rise off the palm rather than swing with the wrist.
	HeldSeconds += DeltaSeconds;
	const float Rise = FMath::SmoothStep(0.0f, HoldRiseTime, HeldSeconds);
	const float Bob = FMath::Sin(HeldSeconds * 2.6f) * HoldBobHeight * Rise;
	const FName Socket = GetAttachParentSocketName();
	const FVector Hand = HolderMesh->GetSocketLocation(Socket);
	SetActorLocation(Hand + FVector(0.0f, 0.0f, HoldRiseHeight * Rise + Bob));
}

void AL2H_TimeBall::AttachToHolder()
{
	// Thrower is not replicated, so a client resolves the holder through the owner the spawn set.
	AL2H_BaseCharacter* Holder = Thrower;
	if (!Holder)
	{
		Holder = Cast<AL2H_BaseCharacter>(GetOwner());
	}

	if (!bHeld)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		return;
	}

	USkeletalMeshComponent* HolderMesh = Holder ? Holder->GetMesh() : nullptr;
	if (HolderMesh)
	{
		AttachToComponent(HolderMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Limits.HoldSocket);

		// The float reads the hand after this frame's pose, not the one before it.
		AddTickPrerequisiteComponent(HolderMesh);
	}
}

void AL2H_TimeBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHeld)
	{
		UpdateHeldFloat(DeltaSeconds);
	}

	if (!HasAuthority() || bResolved)
	{
		return;
	}

	// A planted orb is owned by its field timers rather than by a flight: it is not airborne, so it
	// must not be charged CostPerSecondAirborne and must not be given up on at BallLifetime.
	if (bFieldActive)
	{
		return;
	}

	// A held orb is not in flight: it hangs in the hand until the launch button. Letting it tick here
	// would keep charging CostPerSecondAirborne for time it spends doing nothing, and would let
	// BallLifetime give up on a throw that was never made.
	if (bHeld)
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

	// A field orb has finished travelling the moment it stops: pinning itself to what it hit IS the
	// effect. There is nothing to teleport to, so every trigger mode below is skipped rather than
	// re-interpreted -- a field orb authored with OnSettled would otherwise teleport its thrower.
	if (IsFieldEnabled())
	{
		PlantField(ImpactResult);
		return;
	}

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

	// A planted field is not recalled: a second press closes it where it stands, which is the only
	// decision a field orb has left to make. Recalling would drag the field back into the hand.
	if (bFieldActive)
	{
		EndField();
		Finish();
		return;
	}

	// Still in the hand. Taking it out was the whole of the first press, so pressing again simply puts
	// it back: the orb never left, so this is not a recall and there is no flight to fly.
	if (bHeld)
	{
		PutAway();
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
	
	// He has to leave something behind. Without it the move reads as a camera cut: the eye sees the
	// arrival but never the departure, and a teleport with only an arrival looks like a pop.
	Thrower->MulticastMomentVFX(EL2H_VFXMoment::TeleportOut, NAME_None);

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
	
	// The arrival is the beat the player actually reads, so the ball plays it on every machine rather
	// than leaving it to the throw's pulse, which fired at the other end of the flight. It is the teleport
	// moment rather than the ball moment: the throw's own pulse already fired where he left from.
	Thrower->MulticastMomentVFX(EL2H_VFXMoment::TeleportIn, NAME_None);
	
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

	// Whatever ended the orb, its field lets go first: an actor must never be left dilated because the
	// orb that dilated it was destroyed. No-op when no field was ever planted.
	EndField();

	// Every ending passes through here -- a teleport, a recall, a refusal, a put-away and the
	// lifetime running out -- so this is the one place the hold pose is guaranteed to come off. Left
	// to the launch path alone, a recalled or refused orb would leave the pose stuck on the mesh.
	if (Thrower)
	{
		Thrower->MulticastStopCharacterMontage(Limits.HoldMontage, 0.2f);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	Destroy();
}

bool AL2H_TimeBall::IsFieldEnabled() const
{
	// Both come off the Blueprint child, so a missing value reads as "this is the plain travel orb"
	// rather than as a field with no size.
	return FieldDiameter > 0.0f && !FMath::IsNearlyEqual(FieldDilation, 1.0f, 0.01f);
}

void AL2H_TimeBall::PlantField(const FHitResult& Impact)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || bFieldActive || !IsFieldEnabled())
	{
		return;
	}

	bFieldActive = true;

	// Adhesive rather than bouncing: the orb keeps the surface it hit. Left to its own physics on a
	// slope it would keep rolling and drag the whole field across the level with it, which is exactly
	// the thing a placed field must not do.
	if (UPrimitiveComponent* HitComponent = Impact.GetComponent())
	{
		AttachToComponent(HitComponent, FAttachmentTransformRules::KeepWorldTransform, Impact.BoneName);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	// Planted, the orb is scenery: it must not block or push the players standing in its own field.
	if (BallCollision)
	{
		BallCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// First fill happens here, not on the first timer tick, so the field is real on the frame it
	// plants -- a 0.2s gap would be visible as the orb landing and nothing happening.
	RefreshField();

	if (FieldDuration > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			FieldEndTimer, this, &AL2H_TimeBall::HandleFieldExpired, FieldDuration, false);
	}

	World->GetTimerManager().SetTimer(
		FieldRefreshTimer, this, &AL2H_TimeBall::RefreshField,
		FMath::Max(FieldRefreshInterval, 0.05f), true);

	L2HTime::LogImportant(FString::Printf(
		TEXT("ChronoBall: %s planted a %.2fx field %.0fcm across for %.1fs"),
		*GetNameSafe(Thrower), FieldDilation, FieldDiameter, FieldDuration));
}

void AL2H_TimeBall::RefreshField()
{
	UWorld* World = GetWorld();
	if (!World || !bFieldActive)
	{
		return;
	}

	UL2H_WorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UL2H_WorldTimeSubsystem>();
	if (!TimeSubsystem)
	{
		L2HTime::LogImportant(TEXT("ChronoBall: refused -- the world time subsystem is missing"));
		EndField();
		Finish();
		return;
	}

	const FVector Center = GetActorLocation();
	const float RadiusSquared = FMath::Square(FieldDiameter * 0.5f);

	// Rebuilt on every beat rather than fixed at plant time: the field has to take in whoever walks
	// into it and let go of whoever walks out. Restoring an actor twice would also restore a dilation
	// that is no longer the one that was cached, so membership is tracked exactly.
	TArray<AActor*> Inside;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!Actor || Actor == this)
		{
			continue;
		}

		// A slow orb must not slow the man who threw it; a haste orb usually is thrown to be stood in,
		// so which way this reads is authored per orb instead of decided here.
		if (bFieldExemptsThrower && Actor == Thrower)
		{
			continue;
		}

		if (FVector::DistSquared(Actor->GetActorLocation(), Center) > RadiusSquared)
		{
			continue;
		}

		Inside.Add(Actor);
	}

	TArray<AActor*> ToRestore;
	for (const TObjectPtr<AActor>& Previous : FieldActors)
	{
		AActor* Actor = Previous.Get();
		if (Actor && !Inside.Contains(Actor))
		{
			ToRestore.Add(Actor);
		}
	}

	if (ToRestore.Num() > 0)
	{
		TimeSubsystem->RestoreActorSlow(ToRestore);
	}

	if (Inside.Num() > 0)
	{
		// The same call slows and hastens: the subsystem clamps only to a sane range, not to 1.
		TimeSubsystem->ApplyActorSlow(Inside, FieldDilation);
	}

	FieldActors.Reset();
	FieldActors.Reserve(Inside.Num());
	for (AActor* Actor : Inside)
	{
		FieldActors.Add(Actor);
	}
}

void AL2H_TimeBall::EndField()
{
	if (!bFieldActive)
	{
		return;
	}

	bFieldActive = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FieldRefreshTimer);
	World->GetTimerManager().ClearTimer(FieldEndTimer);

	UL2H_WorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UL2H_WorldTimeSubsystem>();
	if (TimeSubsystem)
	{
		// The list this field applied, not the list standing in it now: an actor that walked out
		// between the last refresh and this moment is still owed its own speed back.
		TArray<AActor*> ToRestore;
		ToRestore.Reserve(FieldActors.Num());
		for (const TObjectPtr<AActor>& Actor : FieldActors)
		{
			if (AActor* Resolved = Actor.Get())
			{
				ToRestore.Add(Resolved);
			}
		}

		if (ToRestore.Num() > 0)
		{
			TimeSubsystem->RestoreActorSlow(ToRestore);
		}
	}

	FieldActors.Reset();

	L2HTime::LogImportant(FString::Printf(
		TEXT("ChronoBall: the field on %s closed"), *GetNameSafe(Thrower)));
}

void AL2H_TimeBall::HandleFieldExpired()
{
	// Closing before the orb goes away, so the actors are released while the orb that owns them is
	// still alive to be named in the log.
	EndField();
	Finish();
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

AL2H_TimeBall* AL2H_TimeBall::FindForOwner(const UObject* WorldContextObject, const AActor* Thrower, const UClass* ExactClass)
{
	if (!WorldContextObject || !Thrower)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject, AL2H_TimeBall::StaticClass(), Found);

	// The hand first, then a flight, then a planted field: with a field standing, the launch button and
	// the aim arc still have to find the orb he is holding, not the one on the floor.
	AL2H_TimeBall* Flying = nullptr;
	AL2H_TimeBall* Planted = nullptr;
	for (AActor* Actor : Found)
	{
		AL2H_TimeBall* Ball = Cast<AL2H_TimeBall>(Actor);
		if (!Ball || Ball->IsResolved() || Ball->GetThrower() != Thrower || (ExactClass && Ball->GetClass() != ExactClass))
		{
			continue;
		}
		if (Ball->IsHeld())
		{
			return Ball;
		}
		AL2H_TimeBall*& Slot = Ball->IsFieldActive() ? Planted : Flying;
		Slot = Slot ? Slot : Ball;
	}

	return Flying ? Flying : Planted;
}
