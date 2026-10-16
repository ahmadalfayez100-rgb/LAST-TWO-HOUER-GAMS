// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_ModeOrb.h"

#include "Character/L2H_AbilityModeComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"

AL2H_ModeOrb::AL2H_ModeOrb()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	Mesh->SetRelativeScale3D(FVector(0.2f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(30.0f);
	Glow->SetAttenuationRadius(260.0f);
	Glow->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bAutoActivate = false;
	Movement->ProjectileGravityScale = 0.6f;
	Movement->bShouldBounce = false;
	Movement->OnProjectileStop.AddDynamic(this, &AL2H_ModeOrb::OnStop);
}

void AL2H_ModeOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AL2H_ModeOrb, bSlow);
	DOREPLIFETIME(AL2H_ModeOrb, bLanded);
	DOREPLIFETIME(AL2H_ModeOrb, SlowedTarget);
	DOREPLIFETIME(AL2H_ModeOrb, SlowFactor);
}

void AL2H_ModeOrb::Launch(UL2H_AbilityModeComponent* InOwnerModes, bool bInSlow, const FVector& Velocity, float InSlowFactor, float InSlowSeconds, float InLifetime)
{
	OwnerModes = InOwnerModes;
	bSlow = bInSlow;
	SlowFactor = InSlowFactor;
	SlowSeconds = InSlowSeconds;
	// Gold for the return marker, blue-violet for the slow.
	Glow->SetLightColor(bSlow ? FLinearColor(0.45f, 0.4f, 1.0f) : FLinearColor(1.0f, 0.75f, 0.25f));
	if (AActor* Thrower = InOwnerModes ? InOwnerModes->GetOwner() : nullptr)
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}
	Movement->Velocity = Velocity;
	Movement->Activate();
	SetLifeSpan(InLifetime);
}

void AL2H_ModeOrb::OnStop(const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}
	bLanded = true;
	AActor* HitActor = Hit.GetActor();

	if (bSlow)
	{
		// Players are not slowed; anything else that moves is (a robot, a moving platform, a swinging thing).
		const bool bSlowable = HitActor && !Cast<AL2H_BaseCharacter>(HitActor) && HitActor->IsRootComponentMovable();
		if (bSlowable)
		{
			AttachToActor(HitActor, FAttachmentTransformRules::KeepWorldTransform);
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SlowedTarget = HitActor;
			OnRep_SlowedTarget();
			SetLifeSpan(SlowSeconds);
			L2HTime::LogImportant(FString::Printf(TEXT("SlowOrb: %s slowed to %.0f%% for %.1fs"), *GetNameSafe(HitActor), SlowFactor * 100.0f, SlowSeconds));
		}
		else
		{
			SetLifeSpan(0.6f);
		}
	}
	else
	{
		// The marker sits where it stopped, no longer in anyone's way.
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		L2HTime::LogImportant(FString::Printf(TEXT("ReturnMarker: laid at %s"), *GetActorLocation().ToString()));
	}

	if (UL2H_AbilityModeComponent* Modes = OwnerModes.Get())
	{
		Modes->OnOrbLanded(this);
	}
}

void AL2H_ModeOrb::OnRep_SlowedTarget()
{
	ApplySlowLocally(SlowedTarget);
}

void AL2H_ModeOrb::ApplySlowLocally(AActor* Target)
{
	if (!Target || LocallySlowed.IsValid())
	{
		return;
	}
	// Multiplied in and divided out again, so it stacks fairly with Yousef's arena instead of fighting it.
	Target->CustomTimeDilation *= SlowFactor;
	LocallySlowed = Target;
}

void AL2H_ModeOrb::RemoveSlowLocally()
{
	if (AActor* Target = LocallySlowed.Get())
	{
		Target->CustomTimeDilation = FMath::Min(1.0f, Target->CustomTimeDilation / FMath::Max(SlowFactor, 0.01f));
	}
	LocallySlowed.Reset();
}

void AL2H_ModeOrb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveSlowLocally();
	Super::EndPlay(EndPlayReason);
}
