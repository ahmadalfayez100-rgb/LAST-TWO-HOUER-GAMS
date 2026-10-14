// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/L2H_ReadableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "UObject/ConstructorHelpers.h"

#include "Interaction/L2H_WorldStateId.h"

AL2H_ReadableActor::AL2H_ReadableActor()
{
	// A note is read on one machine and looked at from the other, so its state replicates.
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PaperMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaperMesh"));
	PaperMesh->SetupAttachment(SceneRoot);
	// A thin slab rather than a plane, because a plane has no thickness to aim at and the focus sweep
	// would skip straight past it. The profile blocks only the L2H Interact channel, so a sheet of
	// paper is never something the player trips over or shoots.
	PaperMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	PaperMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Defaulted to the engine cube so a freshly placed readable is already traceable and testable; a
	// designer replaces it with the real sheet mesh per instance.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PaperMesh->SetStaticMesh(CubeMesh.Object);
		PaperMesh->SetRelativeScale3D(FVector(0.3f, 0.42f, 0.005f));
	}
}

void AL2H_ReadableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_ReadableActor, bOpen);
	DOREPLIFETIME(AL2H_ReadableActor, bFrontSide);
}

void AL2H_ReadableActor::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	OutFlags.Add(FName(TEXT("Open")), bOpen);
	OutFlags.Add(FName(TEXT("FrontSide")), bFrontSide);
}

void AL2H_ReadableActor::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	const bool* Open = InFlags.Find(FName(TEXT("Open")));
	const bool* FrontSide = InFlags.Find(FName(TEXT("FrontSide")));

	if (!Open && !FrontSide)
	{
		return;
	}

	if (Open)
	{
		bOpen = *Open;
	}

	if (FrontSide)
	{
		bFrontSide = *FrontSide;
	}

	// Raised once for the pair rather than once per field: the read panel repaints the whole sheet, so two
	// raises in one restore would be two repaints of the same change.
	OnRep_ReadableState();
}

void AL2H_ReadableActor::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	// An authored id wins so a level can re-root a note without losing what players already read; the
	// path-derived one means a level that never authors anything still persists correctly.
	OutId = StateId.IsValid() ? StateId : L2H_MakeWorldStateId(this);
}

void AL2H_ReadableActor::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	bCanInteract = true;
	OutTargetName = Title;

	// The verb says what the press will actually do, so the player never has to discover that the same
	// key means three different things on the same object.
	if (!bOpen)
	{
		OutVerb = NSLOCTEXT("L2H|Readable", "Read", "Read");
	}
	else if (bHasBackSide && bFrontSide)
	{
		OutVerb = NSLOCTEXT("L2H|Readable", "TurnOver", "Turn over");
	}
	else
	{
		OutVerb = NSLOCTEXT("L2H|Readable", "PutDown", "Put down");
	}
}

void AL2H_ReadableActor::L2H_Interact_Implementation(AActor* Interactor)
{
	if (!bOpen)
	{
		bOpen = true;
		bFrontSide = true;
		L2HTime::LogImportant(FString::Printf(TEXT("Readable opened: %s by %s"), *GetName(), *GetNameSafe(Interactor)));
	}
	else if (bHasBackSide && bFrontSide)
	{
		Flip();
		return;
	}
	else
	{
		bOpen = false;
		L2HTime::LogImportant(FString::Printf(TEXT("Readable closed: %s"), *GetName()));
	}

	// Fired here as well as from OnRep: OnRep never runs on the authority, and without this the host
	// would be the one machine whose read panel never appeared.
	OnReadableStateChanged(bOpen, bFrontSide);
}

bool AL2H_ReadableActor::Flip()
{
	if (!HasAuthority() || !bHasBackSide)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Readable flip refused: %s"), *GetName()));
		return false;
	}

	bFrontSide = !bFrontSide;
	L2HTime::LogVerbose(FString::Printf(TEXT("Readable flipped: %s -> %s"), *GetName(), bFrontSide ? TEXT("front") : TEXT("back")));

	OnReadableStateChanged(bOpen, bFrontSide);
	return true;
}

FText AL2H_ReadableActor::GetVisibleText() const
{
	return (bHasBackSide && !bFrontSide) ? BackText : FrontText;
}

void AL2H_ReadableActor::OnRep_ReadableState()
{
	OnReadableStateChanged(bOpen, bFrontSide);
}
