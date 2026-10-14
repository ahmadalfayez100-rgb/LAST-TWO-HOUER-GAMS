// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/L2H_DoorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "UObject/ConstructorHelpers.h"

#include "Interaction/L2H_WorldStateId.h"

AL2H_DoorActor::AL2H_DoorActor()
{
	bReplicates = true;
	// The tick exists only to walk OpenAlpha toward its target, and it stops mattering the moment the
	// leaf arrives -- but a door that has to be looked at to swing is a door that pops when it comes
	// back into view, so it ticks continuously.
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(SceneRoot);
	FrameMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	FrameMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Both default to the engine cube so a door placed from this class alone is already a visible,
	// traceable, openable object; per-instance meshes and scales replace them.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FrameMesh->SetStaticMesh(CubeMesh.Object);
		FrameMesh->SetRelativeScale3D(FVector(0.1f, 1.2f, 2.1f));
		FrameMesh->SetRelativeLocation(FVector(0.0f, 0.55f, 1.05f));

		DoorMesh->SetStaticMesh(CubeMesh.Object);
		// The leaf spans from the actor origin outwards, so the origin reads as the hinge line.
		DoorMesh->SetRelativeScale3D(FVector(0.06f, 1.0f, 2.0f));
		DoorMesh->SetRelativeLocation(FVector(0.0f, 0.5f, 1.0f));
	}
}

void AL2H_DoorActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		bLocked = bStartLocked;
		bOpen = bStartOpen;
	}

	OpenAlpha = bOpen ? 1.0f : 0.0f;
	if (DoorMesh)
	{
		DoorMesh->SetRelativeRotation(FRotator(0.0f, OpenAngle * OpenAlpha, 0.0f));
	}
}

void AL2H_DoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_DoorActor, bOpen);
	DOREPLIFETIME(AL2H_DoorActor, bLocked);
}

void AL2H_DoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!DoorMesh)
	{
		return;
	}

	const float Target = bOpen ? 1.0f : 0.0f;
	if (FMath::IsNearlyEqual(OpenAlpha, Target, KINDA_SMALL_NUMBER))
	{
		return;
	}

	const float Speed = 1.0f / FMath::Max(OpenSeconds, KINDA_SMALL_NUMBER);
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, Target, DeltaSeconds, Speed);

	DoorMesh->SetRelativeRotation(FRotator(0.0f, OpenAngle * OpenAlpha, 0.0f));
}

void AL2H_DoorActor::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	OutFlags.Add(FName(TEXT("Open")), bOpen);
	OutFlags.Add(FName(TEXT("Locked")), bLocked);
}

void AL2H_DoorActor::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	// Restored through the actor's own authority-gated setters, so a load takes exactly the path a
	// gameplay interaction takes -- including the OnRep that raises OnDoorStateChanged.
	if (const bool* Found = InFlags.Find(FName(TEXT("Open"))))
	{
		SetOpen(*Found);
	}

	if (const bool* Found = InFlags.Find(FName(TEXT("Locked"))))
	{
		SetLocked(*Found);
	}
}

void AL2H_DoorActor::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	OutId = StateId.IsValid() ? StateId : L2H_MakeWorldStateId(this);
}

void AL2H_DoorActor::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	bCanInteract = !bLocked;
	OutTargetName = DisplayName;

	// A locked door still names itself and still says why it will not open. An empty prompt would read
	// as "nothing here", which is the one thing a locked door must not look like.
	if (bLocked)
	{
		OutVerb = NSLOCTEXT("L2H|Door", "Locked", "Locked");
	}
	else
	{
		OutVerb = bOpen
			? NSLOCTEXT("L2H|Door", "Close", "Close")
			: NSLOCTEXT("L2H|Door", "Open", "Open");
	}
}

void AL2H_DoorActor::L2H_Interact_Implementation(AActor* Interactor)
{
	if (bLocked)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Door refused: %s is locked (%s tried)"), *GetName(), *GetNameSafe(Interactor)));
		return;
	}

	bOpen = !bOpen;

	L2HTime::LogImportant(FString::Printf(
		TEXT("Door %s: %s by %s"),
		bOpen ? TEXT("opened") : TEXT("closed"),
		*GetName(),
		*GetNameSafe(Interactor)));

	OnDoorStateChanged(bOpen, bLocked);
}

void AL2H_DoorActor::Unlock(AActor* UnlockedBy)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bLocked)
	{
		return;
	}

	bLocked = false;

	L2HTime::LogImportant(FString::Printf(TEXT("Door unlocked: %s (by %s)"), *GetName(), *GetNameSafe(UnlockedBy)));

	OnDoorStateChanged(bOpen, bLocked);
}

void AL2H_DoorActor::SetLocked(bool bNewLocked)
{
	if (!HasAuthority() || bLocked == bNewLocked)
	{
		return;
	}

	bLocked = bNewLocked;

	L2HTime::LogImportant(FString::Printf(TEXT("Door %s: %s"), bLocked ? TEXT("locked") : TEXT("unlocked"), *GetName()));

	OnDoorStateChanged(bOpen, bLocked);
}

void AL2H_DoorActor::SetOpen(bool bNewOpen)
{
	if (!HasAuthority() || bOpen == bNewOpen)
	{
		return;
	}

	bOpen = bNewOpen;
	OnDoorStateChanged(bOpen, bLocked);
}

void AL2H_DoorActor::OnRep_DoorState()
{
	OnDoorStateChanged(bOpen, bLocked);
}
