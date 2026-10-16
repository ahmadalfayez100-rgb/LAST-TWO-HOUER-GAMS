// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_AreaVolume.h"

#include "Audio/L2H_MusicSubsystem.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AL2H_AreaVolume::AL2H_AreaVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->InitBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(255, 200, 40);
}

void AL2H_AreaVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &AL2H_AreaVolume::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &AL2H_AreaVolume::HandleEndOverlap);
}

void AL2H_AreaVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	Character->ShowAreaName(AreaName);
	if (++LocalPlayersInside == 1 && AmbientMusic)
	{
		if (UL2H_MusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<UL2H_MusicSubsystem>() : nullptr)
		{
			Music->SetAmbientOverride(AmbientMusic);
		}
	}
}

void AL2H_AreaVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	LocalPlayersInside = FMath::Max(LocalPlayersInside - 1, 0);
	if (LocalPlayersInside == 0 && AmbientMusic)
	{
		if (UL2H_MusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<UL2H_MusicSubsystem>() : nullptr)
		{
			Music->SetAmbientOverride(nullptr);
		}
	}
}
