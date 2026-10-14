// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_PingMarker.h"

#include "Net/UnrealNetwork.h"
#include "World/L2H_StationMarker.h"

#define LOCTEXT_NAMESPACE "L2HPing"

AL2H_PingMarker::AL2H_PingMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Marker = CreateDefaultSubobject<UL2H_StationMarker>(TEXT("Marker"));
	SetRootComponent(Marker);
	// Small next to a station's: a pin, not a building.
	Marker->PadRadius = 35.0f;
	Marker->PoleHeight = 260.0f;
	Marker->Color = Color;
	Marker->Label = LOCTEXT("Here", "HERE");
}

void AL2H_PingMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_PingMarker, bEnemy);
	DOREPLIFETIME(AL2H_PingMarker, Color);
}

void AL2H_PingMarker::Setup(bool bOnEnemy, const FLinearColor& InColor)
{
	bEnemy = bOnEnemy;
	Color = InColor;
	OnRep_Look();
}

void AL2H_PingMarker::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Look();
}

void AL2H_PingMarker::OnRep_Look()
{
	if (!Marker)
	{
		return;
	}
	Marker->Color = bEnemy ? FLinearColor(1.0f, 0.15f, 0.1f) : Color;
	Marker->Label = bEnemy ? LOCTEXT("Enemy", "ENEMY") : LOCTEXT("Here", "HERE");
	Marker->Refresh();
}

#undef LOCTEXT_NAMESPACE
