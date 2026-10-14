// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_ReviveStation.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/L2H_WorldStateId.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"
#include "World/L2H_StationMarker.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AL2H_ReviveStation::AL2H_ReviveStation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Range = CreateDefaultSubobject<UBoxComponent>(TEXT("Range"));
	SetRootComponent(Range);
	Range->InitBoxExtent(FVector(150.0f, 150.0f, 120.0f));
	Range->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Range->SetCollisionResponseToAllChannels(ECR_Ignore);
	Range->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Range);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -115.0f));
	Body->SetRelativeScale3D(FVector(2.4f, 2.4f, 0.1f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Disc.Succeeded())
	{
		Body->SetStaticMesh(Disc.Object);
	}
	Body->SetVisibility(false);

	bAlwaysRelevant = true;
	Marker = CreateDefaultSubobject<UL2H_StationMarker>(TEXT("Marker"));
	Marker->SetupAttachment(Range);
	Marker->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
}

void AL2H_ReviveStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Marker->Color = FLinearColor(1.0f, 0.12f, 0.1f);
	Marker->Label = NSLOCTEXT("L2HStation", "Revive", "REVIVE STATION: carry a downed partner here");
	Marker->PadRadius = 150.0f;
	Marker->Refresh();
}

void AL2H_ReviveStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AL2H_ReviveStation, ChargesUsed);
	DOREPLIFETIME(AL2H_ReviveStation, bPortable);
}

void AL2H_ReviveStation::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(ScanTimer, this, &AL2H_ReviveStation::Scan, 0.25f, true);
	}
}

void AL2H_ReviveStation::Scan()
{
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (!Director || !Config || GetChargesLeft() <= 0)
	{
		return;
	}

	for (AL2H_BaseCharacter* Pawn : Director->GetRegisteredPawns())
	{
		if (!Pawn || !(Pawn->IsDead() || Pawn->IsDowned()))
		{
			continue;
		}

		// A carried body has no collision of its own, so it is found through its carrier; one laid down
		// is found where it lies. Either way, the body's position is what has to be inside.
		const FVector Where = Pawn->GetActorLocation();
		const FVector Local = Range->GetComponentTransform().InverseTransformPosition(Where);
		const FVector Extent = Range->GetUnscaledBoxExtent();
		if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z + 60.0f)
		{
			continue;
		}

		++ChargesUsed;
		OnRep_ChargesUsed();

		Director->RevivePawn(Pawn, Pawn->IsDead() ? Config->MaxHealth * 0.5f : 0.0f,
			FMath::Max(AL2H_TimeDirector::GetTimeFor(Pawn), 0.0f) + ReviveSeconds);

		L2HTime::LogImportant(FString::Printf(TEXT("ReviveStation: %s revived %s (+%.0fs), %d charge(s) left"),
			*GetNameSafe(this), *GetNameSafe(Pawn), ReviveSeconds, GetChargesLeft()));

		if (bPortable)
		{
			Destroy();
			return;
		}
		if (GetChargesLeft() <= 0)
		{
			return;
		}
	}
}

void AL2H_ReviveStation::Refill()
{
	if (bPortable)
	{
		// A beacon belongs to the stage it was placed in.
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	if (HasAuthority() && ChargesUsed != 0)
	{
		ChargesUsed = 0;
		OnRep_ChargesUsed();
	}
}

void AL2H_ReviveStation::MakePortable(float InReviveSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	bPortable = true;
	Charges = 1;
	ChargesUsed = 0;
	ReviveSeconds = FMath::Max(InReviveSeconds, 1.0f);
	OnRep_Portable();
}

void AL2H_ReviveStation::OnRep_Portable()
{
	if (bPortable && Marker)
	{
		Marker->Color = FLinearColor(0.2f, 0.8f, 1.0f);
		Marker->Label = NSLOCTEXT("L2HStation", "Beacon", "BEACON: carry a downed partner here");
		Marker->PadRadius = 120.0f;
		Marker->Refresh();
	}
}

void AL2H_ReviveStation::OnRep_ChargesUsed()
{
	OnChargesChanged(GetChargesLeft());
}

void AL2H_ReviveStation::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	OutId = bPortable ? FGuid() : L2H_MakeWorldStateId(this);
}

void AL2H_ReviveStation::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	// One flag per charge, since the record only holds booleans.
	for (int32 Index = 0; Index < Charges; ++Index)
	{
		OutFlags.Add(FName(*FString::Printf(TEXT("Used%d"), Index)), Index < ChargesUsed);
	}
}

void AL2H_ReviveStation::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	int32 Used = 0;
	for (int32 Index = 0; Index < Charges; ++Index)
	{
		const bool* Flag = InFlags.Find(FName(*FString::Printf(TEXT("Used%d"), Index)));
		Used += (Flag && *Flag) ? 1 : 0;
	}
	if (HasAuthority())
	{
		ChargesUsed = Used;
		OnRep_ChargesUsed();
	}
}
