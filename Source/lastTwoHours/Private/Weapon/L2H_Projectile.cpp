// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/L2H_Projectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Damage/L2H_HitReactionComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "UObject/ConstructorHelpers.h"

AL2H_Projectile::AL2H_Projectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	// A small sphere does the colliding, so the round hits what its centre reaches rather than what its
	// stretched visual grazes.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(2.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(Collision);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The pack's round is under a centimetre; at gun speed that is invisible, so it is drawn stretched
	// along the flight, which reads as a bullet with a short streak behind it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BulletMesh(TEXT("/Game/HS_WeaponPack_03/Meshes/Bullets/SM_HS_WP03_Bullet01_0.SM_HS_WP03_Bullet01_0"));
	if (BulletMesh.Succeeded())
	{
		Visual->SetStaticMesh(BulletMesh.Object);
	}
	Visual->SetRelativeScale3D(FVector(12.0f, 3.0f, 3.0f));

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 9000.0f;
	Movement->MaxSpeed = 20000.0f;
	Movement->ProjectileGravityScale = 1.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;

	Collision->OnComponentHit.AddDynamic(this, &AL2H_Projectile::HandleHit);
}

void AL2H_Projectile::InitShot(AActor* InShooter, float InDamage, EL2H_DamageType InDamageType, float InHeadshotMultiplier,
	const FVector& Direction, float Speed)
{
	Damage = InDamage;
	DamageType = InDamageType;
	HeadshotMultiplier = InHeadshotMultiplier;
	SetLifeSpan(MaxLifetime);

	// The shooter and everything they hold is ignored, so a round fired from inside the capsule's reach
	// does not stop on the hand that fired it.
	if (InShooter)
	{
		Collision->IgnoreActorWhenMoving(InShooter, true);
		TArray<AActor*> Attached;
		InShooter->GetAttachedActors(Attached);
		for (AActor* Held : Attached)
		{
			Collision->IgnoreActorWhenMoving(Held, true);
		}
	}

	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	SetActorRotation(Direction.Rotation());
}

void AL2H_Projectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bSpent)
	{
		return;
	}
	bSpent = true;

	const FVector Velocity = Movement->Velocity;
	const FVector Direction = Velocity.GetSafeNormal();
	AActor* Shooter = GetOwner();
	APawn* ShooterPawn = Cast<APawn>(Shooter);

	// The physics half: a loose body is knocked along the flight, less the heavier it is.
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		const float Mass = FMath::Max(OtherComp->GetMass(), 1.0f);
		OtherComp->AddImpulseAtLocation(Direction * ImpactPush / Mass * FMath::Min(Mass, 30.0f), Hit.ImpactPoint, Hit.BoneName);
	}

	if (OtherActor && OtherActor != Shooter)
	{
		const bool bHeadshot = Hit.BoneName == TEXT("head");

		// The same reading an instant shot makes: the target's own response to this damage type first,
		// the round's own number when it has none.
		UL2H_HitReactionComponent* Reactions = OtherActor->FindComponentByClass<UL2H_HitReactionComponent>();
		const FL2H_DamageResponse* Response = Reactions ? Reactions->FindResponse(DamageType) : nullptr;
		const float Base = Response ? Response->TimeDamage : Damage;
		const float Multiplier = Response ? Response->HeadshotMultiplier : HeadshotMultiplier;
		const float Applied = Base * (bHeadshot ? Multiplier : 1.0f);

		UGameplayStatics::ApplyPointDamage(OtherActor, Applied, Direction, Hit,
			ShooterPawn ? ShooterPawn->GetController() : nullptr, this, nullptr);

		if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
		{
			Director->ApplyTimeDamage(OtherActor, Applied);
		}

		if (Reactions)
		{
			Reactions->ApplyHitReaction(DamageType, Shooter, bHeadshot, Hit.ImpactPoint, Direction);
		}

		L2HTime::LogImportant(FString::Printf(TEXT("Projectile: hit %s for %.1f%s"),
			*GetNameSafe(OtherActor), Applied, bHeadshot ? TEXT(" (headshot)") : TEXT("")));
	}

	MulticastImpact(Hit.ImpactPoint, Hit.ImpactNormal);
	Destroy();
}

void AL2H_Projectile::MulticastImpact_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal)
{
	OnImpact(Location, Normal);
}
