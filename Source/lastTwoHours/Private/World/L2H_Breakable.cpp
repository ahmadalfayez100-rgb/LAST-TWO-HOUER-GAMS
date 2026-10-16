// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_Breakable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Interaction/L2H_WorldStateId.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Item/L2H_ItemPickup.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "Enemy/L2H_EnemyRobot.h"

AL2H_Breakable::AL2H_Breakable()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	// MSHF's wooden crate and the three pieces it breaks into; the wood sound from ResidentHorrorV2.
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModernSurvivalHorrorFramework/Asset/Mesh/WoodenCrate/SM_WoodenCrate1.SM_WoodenCrate1"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	for (int32 Index = 1; Index <= 3; ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/ModernSurvivalHorrorFramework/Asset/Mesh/WoodenCrate/SM_BrokenCrate-p%d.SM_BrokenCrate-p%d"), Index, Index);
		if (UStaticMesh* Piece = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			PieceMeshes.Add(Piece);
		}
	}
	BreakSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/ResidentHorrorV2/Audio/SoundWaves/WAV_wood-smash-5-170421.WAV_wood-smash-5-170421"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AL2H_Breakable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AL2H_Breakable, Health);
	DOREPLIFETIME(AL2H_Breakable, bBroken);
}

void AL2H_Breakable::BeginPlay()
{
	Super::BeginPlay();
	bPlaced = IsNetStartupActor();
	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

float AL2H_Breakable::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || bBroken || DamageAmount <= 0.0f)
	{
		return Applied;
	}

	Health = FMath::Max(0.0f, Health - DamageAmount);
	if (Health <= 0.0f)
	{
		Break(DamageCauser);
	}
	return DamageAmount;
}

void AL2H_Breakable::Break(AActor* Breaker)
{
	if (!HasAuthority() || bBroken)
	{
		return;
	}
	bBroken = true;
	ShowBroken();
	AL2H_EnemyRobot::ReportNoise(this, GetActorLocation(), BreakNoiseRadius);

	// What it held comes out where it stood.
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		for (int32 Index = 0; Index < LootShards; ++Index)
		{
			Director->SpillShardAt(this, LootShardSeconds);
		}
	}
	if (LootItem && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* PickupClass = LoadClass<AL2H_ItemPickup>(nullptr, TEXT("/Game/L2HTime/Items/BP_L2H_ItemPickup.BP_L2H_ItemPickup_C"));
		if (AL2H_ItemPickup* Pickup = GetWorld()->SpawnActor<AL2H_ItemPickup>(PickupClass ? PickupClass : AL2H_ItemPickup::StaticClass(),
			GetActorLocation() + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, Params))
		{
			FL2H_ItemInstance Instance;
			Instance.Data = LootItem;
			Instance.InstanceId = FGuid::NewGuid();
			Pickup->InitialiseFromInstance(Instance);
		}
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Breakable: %s broken by %s"), *GetNameSafe(this), *GetNameSafe(Breaker)));
}

void AL2H_Breakable::OnRep_Broken()
{
	if (bBroken)
	{
		ShowBroken();
	}
	else
	{
		ShowWhole();
	}
}

void AL2H_Breakable::ShowBroken()
{
	if (bShownBroken)
	{
		return;
	}
	bShownBroken = true;
	Mesh->SetVisibility(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
	}

	// The pieces are cosmetic and local to each machine: they fly out from the centre and are gone soon after.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	const FVector Centre = Mesh->Bounds.Origin;
	for (UStaticMesh* PieceMesh : PieceMeshes)
	{
		AStaticMeshActor* Piece = PieceMesh ? World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), GetActorTransform(), Params) : nullptr;
		if (!Piece)
		{
			continue;
		}
		Piece->SetReplicates(false);
		UStaticMeshComponent* PieceComp = Piece->GetStaticMeshComponent();
		PieceComp->SetMobility(EComponentMobility::Movable);
		PieceComp->SetStaticMesh(PieceMesh);
		PieceComp->SetCollisionProfileName(TEXT("PhysicsActor"));
		// Players walk through debris instead of tripping on it.
		PieceComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		PieceComp->SetSimulatePhysics(true);
		const FVector Out = (PieceComp->Bounds.Origin - Centre).GetSafeNormal2D();
		PieceComp->SetPhysicsLinearVelocity(Out * PieceSpeed + FVector(0.0f, 0.0f, PieceSpeed * 0.6f) + FMath::VRand() * 60.0f);
		PieceComp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 200.0f);
		Piece->SetLifeSpan(6.0f);
	}
}

void AL2H_Breakable::ShowWhole()
{
	bShownBroken = false;
	Mesh->SetVisibility(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AL2H_Breakable::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	bCanInteract = false;
}

void AL2H_Breakable::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	OutId = bPlaced ? L2H_MakeWorldStateId(this) : FGuid();
}

void AL2H_Breakable::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	OutFlags.Add(TEXT("Broken"), bBroken);
}

void AL2H_Breakable::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	const bool* Broken = InFlags.Find(TEXT("Broken"));
	if (!Broken || !HasAuthority())
	{
		return;
	}
	// Restored to the checkpoint: whole again (a rewind past the break), or still broken -- without
	// the scatter, the sound or a second drop of loot.
	bBroken = *Broken;
	Health = bBroken ? 0.0f : MaxHealth;
	if (bBroken)
	{
		bShownBroken = true;
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		ShowWhole();
	}
}
