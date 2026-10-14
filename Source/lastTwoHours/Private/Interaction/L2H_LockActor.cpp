// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/L2H_LockActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/L2H_DoorActor.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "UObject/ConstructorHelpers.h"

#include "Interaction/L2H_WorldStateId.h"

AL2H_LockActor::AL2H_LockActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionProfileName(TEXT("L2H_Interactable"));
	PadMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeMesh.Object);
		PadMesh->SetRelativeScale3D(FVector(0.16f, 0.12f, 0.22f));
	}
}

void AL2H_LockActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_LockActor, bUnlocked);
	DOREPLIFETIME(AL2H_LockActor, KeypadUser);
}

void AL2H_LockActor::L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags)
{
	// Only whether it gave way. The code itself is never written down: the server-side answer is the one
	// thing in this actor that must not end up in a file a player can read.
	OutFlags.Add(FName(TEXT("Unlocked")), bUnlocked);
}

void AL2H_LockActor::L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags)
{
	const bool* Unlocked = InFlags.Find(FName(TEXT("Unlocked")));
	if (!Unlocked || *Unlocked == bUnlocked)
	{
		return;
	}

	// Restores itself from its own record and does not reach for the door it opens: the door has a record
	// of its own and restores on the same pass. Pushing this state onto a neighbour would apply the same
	// change twice and would break the moment the two actors were recorded in different checkpoints.
	bUnlocked = *Unlocked;
	OnRep_LockState();

	if (bUnlocked)
	{
		// A lock giving way is something the level reacts to, so it is raised here too -- once, with
		// nobody to credit for it.
		OnLockOpened(nullptr);
	}
}

void AL2H_LockActor::L2H_GetWorldStateId_Implementation(FGuid& OutId)
{
	OutId = StateId.IsValid() ? StateId : L2H_MakeWorldStateId(this);
}

void AL2H_LockActor::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	OutTargetName = DisplayName;

	// Once solved there is nothing left to press, so the pad stops offering itself rather than opening
	// a keypad that can only fail.
	bCanInteract = !bUnlocked;

	OutVerb = bUnlocked
		? NSLOCTEXT("L2H|Lock", "Unlocked", "Unlocked")
		: NSLOCTEXT("L2H|Lock", "EnterCode", "Enter code");
}

void AL2H_LockActor::L2H_Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || bUnlocked)
	{
		return;
	}

	// The pad does not unlock anything by being pressed -- it asks for the code. The keypad is shown by
	// the HUD of the player who pressed, which is why the user travels and the code does not.
	KeypadUser = Interactor;

	L2HTime::LogImportant(FString::Printf(TEXT("Lock keypad opened: %s by %s"), *GetName(), *GetNameSafe(Interactor)));

	OnKeypadStateChanged(Interactor, true);
}

bool AL2H_LockActor::SubmitCode(AActor* Interactor, const FString& Code)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (bUnlocked)
	{
		return true;
	}

	if (Code != CorrectCode)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Lock refused: %s -- %s entered a wrong code (%d digits)"),
			*GetName(),
			*GetNameSafe(Interactor),
			Code.Len()));

		OnCodeRejected();
		return false;
	}

	bUnlocked = true;
	KeypadUser = nullptr;

	L2HTime::LogImportant(FString::Printf(TEXT("Lock opened: %s by %s"), *GetName(), *GetNameSafe(Interactor)));

	if (LinkedDoor)
	{
		LinkedDoor->Unlock(Interactor);
	}
	else
	{
		// Worth a line of its own: a lock with no door reads exactly like a broken puzzle.
		L2HTime::LogImportant(FString::Printf(TEXT("Lock %s opened but has no LinkedDoor assigned"), *GetName()));
	}

	OnLockOpened(Interactor);
	OnKeypadStateChanged(nullptr, false);
	return true;
}

void AL2H_LockActor::CloseKeypad(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (KeypadUser != Interactor)
	{
		return;
	}

	KeypadUser = nullptr;
	OnKeypadStateChanged(Interactor, false);
}

void AL2H_LockActor::OnRep_LockState()
{
	// One OnRep for both properties: the keypad and the unlocked flag change together in every path that
	// changes either, and a second callback would only fire a duplicate repaint.
	if (bUnlocked)
	{
		OnLockOpened(nullptr);
	}

	OnKeypadStateChanged(KeypadUser, KeypadUser != nullptr);
}
