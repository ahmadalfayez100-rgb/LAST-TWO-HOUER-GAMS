// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_TimeShard.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"
#include "UObject/ConstructorHelpers.h"

AL2H_TimeShard::AL2H_TimeShard()
{
	// The magnet runs on the server only, but the shard has to tick everywhere so a client that is
	// not the authority still shows the drift the replicated movement describes.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(PickupRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(false);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.35f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
}

void AL2H_TimeShard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_TimeShard, TimeValue);
	DOREPLIFETIME(AL2H_TimeShard, RemainingLifetime);
}

void AL2H_TimeShard::InitialiseFromConfig(float InTimeValue, float InLifetime)
{
	TimeValue = FMath::Max(0.0f, InTimeValue);
	RemainingLifetime = FMath::Max(0.0f, InLifetime);
}

void AL2H_TimeShard::IgnoreCollectorFor(AActor* InIgnored, float Seconds)
{
	const UWorld* World = GetWorld();
	IgnoredCollector = InIgnored;
	IgnoreCollectorUntil = (World ? World->GetTimeSeconds() : 0.0f) + FMath::Max(0.0f, Seconds);
}

void AL2H_TimeShard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (RemainingLifetime > 0.0f)
	{
		RemainingLifetime -= DeltaSeconds;
		if (RemainingLifetime <= 0.0f)
		{
			L2HTime::LogVerbose(TEXT("Shard: expired"));
			Destroy();
			return;
		}
	}

	TickMagnet(DeltaSeconds);
}

void AL2H_TimeShard::TickMagnet(float DeltaSeconds)
{
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float MagnetRange = Config ? Config->ShardMagnetRange : 350.0f;
	const float MagnetSpeed = Config ? Config->ShardMagnetSpeed : 600.0f;

	AL2H_BaseCharacter* Collector = FindCollector(MagnetRange);
	if (!Collector)
	{
		return;
	}

	const FVector ShardLocation = GetActorLocation();
	const FVector TargetLocation = Collector->GetActorLocation();

	if (FVector::Dist(ShardLocation, TargetLocation) <= PickupRadius)
	{
		TryAbsorb(Collector);
		return;
	}

	if (MagnetSpeed <= 0.0f)
	{
		return;
	}

	const FVector Step = (TargetLocation - ShardLocation).GetSafeNormal() * MagnetSpeed * DeltaSeconds;
	SetActorLocation(ShardLocation + Step, /*bSweep=*/false);

	if (FVector::Dist(GetActorLocation(), TargetLocation) <= PickupRadius)
	{
		TryAbsorb(Collector);
	}
}

AL2H_BaseCharacter* AL2H_TimeShard::FindCollector(float MagnetRange) const
{
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		return nullptr;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const int32 MaxShards = Config ? Config->MaxCarriedShards : 1;

	const FVector ShardLocation = GetActorLocation();
	const float RangeSq = FMath::Square(MagnetRange);

	AL2H_BaseCharacter* Best = nullptr;
	float BestDistanceSq = RangeSq;

	const UWorld* World = GetWorld();
	const bool bIgnoreWindowOpen = World && World->GetTimeSeconds() < IgnoreCollectorUntil;

	for (AL2H_BaseCharacter* Candidate : Director->GetRegisteredPawns())
	{
		// A dead body collects nothing, for the same reason a downed one does not.
		if (!Candidate || Candidate->IsDowned() || Candidate->IsDead() || !Candidate->HasShardRoom(MaxShards))
		{
			continue;
		}

		if (bIgnoreWindowOpen && IgnoredCollector.Get() == Candidate)
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(ShardLocation, Candidate->GetActorLocation());
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	return Best;
}

bool AL2H_TimeShard::TryAbsorb(AL2H_BaseCharacter* Character)
{
	if (bConsumed || !HasAuthority() || !Character)
	{
		return false;
	}

	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		return false;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const int32 MaxShards = Config ? Config->MaxCarriedShards : 1;

	// Re-checked through the pawn, because the room the magnet looked for may be gone by now.
	if (!Character->HasShardRoom(MaxShards))
	{
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.0f;
		if (Now - LastRefusalLogTime >= 1.0f)
		{
			LastRefusalLogTime = Now;
			L2HTime::LogImportant(FString::Printf(
				TEXT("Shard: %s cannot hold another shard -- pickup refused"), *GetNameSafe(Character)));
		}
		return false;
	}

	bConsumed = true;

	Director->ApplyTimeRestore(Character, TimeValue);
	Character->BeginShardAbsorb(Config ? Config->ShardCarryDuration : 1.0f);

	L2HTime::LogImportant(FString::Printf(
		TEXT("Shard: %s absorbed %.1fs"), *GetNameSafe(Character), TimeValue));

	Destroy();
	return true;
}
