// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_RecallAnchor.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

AL2H_RecallAnchor::AL2H_RecallAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Marker and root share an origin, and the effect is authored to sit above that origin: the mark is
	// drawn on the floor the actor stands on, not floating around the actor's middle.
	MarkerFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MarkerFX"));
	MarkerFX->SetupAttachment(SceneRoot);
	MarkerFX->SetAutoActivate(false);
}

void AL2H_RecallAnchor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_RecallAnchor, AnchorOwner);
}

void AL2H_RecallAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (MarkerFX && MarkerSystem)
	{
		MarkerFX->SetAsset(MarkerSystem);
		MarkerFX->Activate(true);
	}
}

void AL2H_RecallAnchor::SetAnchorOwner(APawn* InOwner)
{
	AnchorOwner = InOwner;
}

FVector AL2H_RecallAnchor::GetReturnLocation() const
{
	float HalfHeight = 88.0f;

	if (const ACharacter* Character = Cast<ACharacter>(AnchorOwner))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	// Two centimetres of clearance rather than zero: TeleportTo refuses a spot that starts in contact
	// with the floor it is standing on.
	return GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
}
