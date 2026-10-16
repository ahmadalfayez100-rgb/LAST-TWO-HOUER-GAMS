// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_TestCourse.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Interaction/L2H_DoorActor.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Item/L2H_ItemPickup.h"
#include "Weapon/L2H_WeaponDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeShard.h"
#include "World/L2H_AreaVolume.h"
#include "World/L2H_Breakable.h"
#include "World/L2H_ExplosiveBarrel.h"

namespace
{
	UStaticMesh* CubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
}

AL2H_TestCourse::AL2H_TestCourse()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AL2H_TestCourse::BeginPlay()
{
	Super::BeginPlay();

	BuildLocal();
	if (HasAuthority())
	{
		SpawnReplicated();
	}
}

void AL2H_TestCourse::AddLabel(const FVector& Location, const FString& Text, float Size)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(GetRootComponent());
	Label->RegisterComponent();
	Label->SetWorldLocation(Location);
	// Facing the start (the course lies to the start's left, along +X).
	Label->SetWorldRotation(FRotator(0.0f, 90.0f, 0.0f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(Size);
	Label->SetTextRenderColor(FColor(255, 230, 120));
	Label->SetText(FText::FromString(Text));
}

void AL2H_TestCourse::BuildLocal()
{
	UWorld* World = GetWorld();
	UStaticMesh* Cube = CubeMesh();
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!World || !Cube)
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	// Surface tiles, one per surface type, in a row along +X: walk across them to hear each footstep.
	static const TCHAR* const Surfaces[] = { TEXT("Grass"), TEXT("Wood"), TEXT("Metal"), TEXT("Water"), TEXT("Glass"),
		TEXT("Snow"), TEXT("Gravel"), TEXT("Mud"), TEXT("Leaves"), TEXT("Ice") };
	static const FLinearColor Colors[] = { FLinearColor(0.1f, 0.5f, 0.1f), FLinearColor(0.45f, 0.28f, 0.12f), FLinearColor(0.5f, 0.5f, 0.55f),
		FLinearColor(0.1f, 0.3f, 0.6f), FLinearColor(0.6f, 0.8f, 0.85f), FLinearColor(0.9f, 0.9f, 0.95f), FLinearColor(0.4f, 0.4f, 0.38f),
		FLinearColor(0.25f, 0.18f, 0.1f), FLinearColor(0.55f, 0.35f, 0.1f), FLinearColor(0.7f, 0.85f, 1.0f) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Surfaces); ++Index)
	{
		const FVector At = Origin + FVector(-1350.0f + Index * 270.0f, 300.0f, 3.0f);
		UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
		Tile->SetupAttachment(GetRootComponent());
		Tile->SetMobility(EComponentMobility::Movable);
		Tile->SetStaticMesh(Cube);
		Tile->RegisterComponent();
		Tile->SetWorldLocation(At);
		Tile->SetWorldScale3D(FVector(2.6f, 2.6f, 0.06f));
		Tile->SetCollisionProfileName(TEXT("BlockAll"));

		const FString PMPath = FString::Printf(TEXT("/Game/L2HTime/Surfaces/PM_L2H_%s.PM_L2H_%s"), Surfaces[Index], Surfaces[Index]);
		if (UPhysicalMaterial* PM = LoadObject<UPhysicalMaterial>(nullptr, *PMPath))
		{
			Tile->SetPhysMaterialOverride(PM);
		}
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(BaseMaterial, Tile);
			Paint->SetVectorParameterValue(TEXT("Color"), Colors[Index]);
			Tile->SetMaterial(0, Paint);
		}
		AddLabel(At + FVector(0.0f, 0.0f, 120.0f), Surfaces[Index], 34.0f);
	}

	// A tall climbable block (DPTS tags): hook to its edge, climb it, and jump off it for a hard landing.
	FActorSpawnParameters Local;
	Local.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Local.ObjectFlags |= RF_Transient;
	const FVector BlockAt = Origin + FVector(600.0f, -350.0f, 210.0f);
	if (AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), BlockAt, FRotator::ZeroRotator, Local))
	{
		Block->SetReplicates(false);
		UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Block->SetActorScale3D(FVector(3.0f, 3.0f, 4.2f));
		Block->Tags = { TEXT("climb"), TEXT("ledge") };
		AddLabel(BlockAt + FVector(0.0f, 200.0f, 280.0f), TEXT("Climbable 4.2 m: hook to the edge, jump off"), 22.0f);
	}

	// The area: its name on entering.
	if (AL2H_AreaVolume* Area = World->SpawnActor<AL2H_AreaVolume>(AL2H_AreaVolume::StaticClass(), Origin + FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator, Local))
	{
		Area->AreaName = NSLOCTEXT("L2HTest", "Course", "Test Course");
		Area->SetActorScale3D(FVector(3.2f, 1.3f, 1.0f));
	}

	AddLabel(Origin + FVector(-600.0f, -250.0f, 250.0f), TEXT("Door"), 26.0f);
	AddLabel(Origin + FVector(-1120.0f, -250.0f, 200.0f), TEXT("Items"), 26.0f);
	AddLabel(Origin + FVector(-800.0f, -250.0f, 200.0f), TEXT("Pipe (melee: R2, finisher: Triangle on a stunned robot)"), 20.0f);
	AddLabel(Origin + FVector(1300.0f, -200.0f, 260.0f), TEXT("Robot"), 26.0f);
	AddLabel(Origin + FVector(280.0f, -650.0f, 200.0f), TEXT("Crates: shoot or hit them"), 22.0f);
	AddLabel(Origin + FVector(1000.0f, -220.0f, 220.0f), TEXT("Barrels: one shot, keep your distance"), 22.0f);

	AddLabel(Origin + FVector(-1350.0f, 650.0f, 260.0f),
		TEXT("TEST COURSE: tiles = footsteps, door + items = Triangle, block = hook/climb/landing, robot = music & shards"), 20.0f);
}

void AL2H_TestCourse::SpawnReplicated()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Origin = GetActorLocation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// A door, opened with Interact (the push clip plays on the side it is on).
	UClass* DoorClass = LoadClass<AL2H_DoorActor>(nullptr, TEXT("/Game/L2HTime/Interaction/BP_L2H_Door.BP_L2H_Door_C"));
	World->SpawnActor<AL2H_DoorActor>(DoorClass ? DoorClass : AL2H_DoorActor::StaticClass(),
		Origin + FVector(-600.0f, -350.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f), Params);

	// Three items to pick up.
	UClass* PickupClass = LoadClass<AL2H_ItemPickup>(nullptr, TEXT("/Game/L2HTime/Items/BP_L2H_ItemPickup.BP_L2H_ItemPickup_C"));
	static const TCHAR* const Items[] = { TEXT("DA_L2H_Item_Potion"), TEXT("DA_L2H_Item_Note"), TEXT("DA_L2H_Item_Ammo_Pistol") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Items); ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/L2HTime/Items/%s.%s"), Items[Index], Items[Index]);
		UL2H_ItemDataAsset* Data = LoadObject<UL2H_ItemDataAsset>(nullptr, *Path);
		AL2H_ItemPickup* Pickup = Data ? World->SpawnActor<AL2H_ItemPickup>(PickupClass ? PickupClass : AL2H_ItemPickup::StaticClass(),
			Origin + FVector(-1300.0f + Index * 180.0f, -350.0f, 40.0f), FRotator::ZeroRotator, Params) : nullptr;
		if (Pickup)
		{
			FL2H_ItemInstance Instance;
			Instance.Data = Data;
			Instance.Quantity = 1;
			Instance.InstanceId = FGuid::NewGuid();
			Pickup->InitialiseFromInstance(Instance);
		}
	}

	// The pipe: taking it adds the melee weapon to the loadout (D-pad Down to it, R2 to swing).
	if (UL2H_ItemDataAsset* PipeItem = LoadObject<UL2H_ItemDataAsset>(nullptr, TEXT("/Game/L2HTime/Items/DA_L2H_Item_Pipe.DA_L2H_Item_Pipe")))
	{
		if (AL2H_ItemPickup* Pipe = World->SpawnActor<AL2H_ItemPickup>(PickupClass ? PickupClass : AL2H_ItemPickup::StaticClass(),
			Origin + FVector(-800.0f, -350.0f, 40.0f), FRotator::ZeroRotator, Params))
		{
			FL2H_ItemInstance Instance;
			Instance.Data = PipeItem;
			Instance.InstanceId = FGuid::NewGuid();
			Pipe->GrantsWeapon = LoadObject<UL2H_WeaponDataAsset>(nullptr, TEXT("/Game/L2HTime/Weapons/DA_L2H_Weapon_Pipe.DA_L2H_Weapon_Pipe"));
			Pipe->InitialiseFromInstance(Instance);
		}
	}

	// Crates (break with a shot or the pipe: a time shard each) and barrels next to the robot (one shot, a chain).
	for (int32 Index = 0; Index < 3; ++Index)
	{
		World->SpawnActor<AL2H_Breakable>(AL2H_Breakable::StaticClass(), Origin + FVector(150.0f + Index * 130.0f, -750.0f, 28.0f), FRotator(0.0f, Index * 25.0f, 0.0f), Params);
	}
	World->SpawnActor<AL2H_ExplosiveBarrel>(AL2H_ExplosiveBarrel::StaticClass(), Origin + FVector(1000.0f, -320.0f, 66.0f), FRotator::ZeroRotator, Params);
	World->SpawnActor<AL2H_ExplosiveBarrel>(AL2H_ExplosiveBarrel::StaticClass(), Origin + FVector(1180.0f, -120.0f, 66.0f), FRotator::ZeroRotator, Params);
	World->SpawnActor<AL2H_ExplosiveBarrel>(AL2H_ExplosiveBarrel::StaticClass(), Origin + FVector(1180.0f, -520.0f, 66.0f), FRotator::ZeroRotator, Params);

	// A time shard to collect, and a robot further on: the combat music when near, shards when it dies.
	UClass* ShardClass = LoadClass<AL2H_TimeShard>(nullptr, TEXT("/Game/L2HTime/Blueprints/BP_L2H_TimeShard.BP_L2H_TimeShard_C"));
	World->SpawnActor<AL2H_TimeShard>(ShardClass ? ShardClass : AL2H_TimeShard::StaticClass(), Origin + FVector(-200.0f, -350.0f, 80.0f), FRotator::ZeroRotator, Params);
	World->SpawnActor<AL2H_EnemyRobot>(AL2H_EnemyRobot::StaticClass(), Origin + FVector(1300.0f, -300.0f, 100.0f), FRotator(0.0f, 180.0f, 0.0f), Params);

	L2HTime::LogImportant(TEXT("TestCourse: built beside the first PlayerStart (tiles, door, items, block, shard, robot)"));
}
