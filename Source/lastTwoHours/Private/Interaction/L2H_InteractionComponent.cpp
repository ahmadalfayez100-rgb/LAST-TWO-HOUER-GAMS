// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/L2H_InteractionComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"

UL2H_InteractionComponent::UL2H_InteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// An inactive component never ticks, so without this the focus trace never runs and every press is
	// refused for "nothing focused". Blueprint-added components default to inactive.
	bAutoActivate = true;

	// Focus and prompt are replicated state, so the component opts in itself. Leaving this to the
	// Blueprint would mean a pawn that forgot the checkbox still looked correct on the host and showed
	// no prompt at all on a client -- the kind of bug that only shows up in the second window.
	SetIsReplicatedByDefault(true);
}

void UL2H_InteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Force a refresh on the first tick rather than waiting out a full interval, so a pawn that spawns
	// already facing a door shows its prompt immediately.
	TimeSinceRefresh = RefreshInterval;
}

void UL2H_InteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// A client never traces. It reads the replicated focus, which is also what keeps the two
		// players in a co-op session agreeing about what is in range.
		return;
	}

	TimeSinceRefresh += DeltaTime;
	if (TimeSinceRefresh < RefreshInterval)
	{
		return;
	}

	TimeSinceRefresh = 0.0f;
	RefreshFocus();
}

void UL2H_InteractionComponent::RefreshFocus()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	// The eyes, not the actor origin: the trace has to start where the player is looking from, or a
	// tall object would be reachable from the wrong side of the capsule.
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const FVector End = EyeLocation + EyeRotation.Vector() * InteractRange;

	FCollisionQueryParams Params(TEXT("L2HInteractFocus"), /*bTraceComplex=*/false, Owner);

	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit,
		EyeLocation,
		End,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(ProbeRadius),
		Params);

	// A hit on scenery is not a refusal -- it is just not a candidate. Only an implementer is ever
	// adopted, so a door frame in front of a paper cannot blank the prompt.
	AActor* NewFocus = (bHit && ImplementsInteractable(Hit.GetActor())) ? Hit.GetActor() : nullptr;

	const bool bChanged = (NewFocus != FocusedActor);
	PreviousFocusedActor = FocusedActor;
	FocusedActor = NewFocus;

	// Rebuilt even when the focus did not change, because the object's own state can (a door unlocks
	// while it is still being looked at, and the prompt has to stop saying "Locked").
	RefreshPrompt();

	if (bChanged)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("%s interaction focus: %s"), *GetNameSafe(Owner), *GetNameSafe(NewFocus)));

		// Fired here rather than left to OnRep, because OnRep never runs on the authority and a listen
		// server's own player would otherwise be the one machine with no highlight.
		OnFocusChanged.Broadcast(FocusedActor, PreviousFocusedActor);
	}
}

void UL2H_InteractionComponent::RefreshPrompt()
{
	FL2H_InteractionPrompt NewPrompt;

	if (ImplementsInteractable(FocusedActor))
	{
		bool bCanInteract = false;
		FText Verb;
		FText TargetName;

		IL2H_InteractableInterface::Execute_L2H_GetInteractPrompt(FocusedActor, bCanInteract, Verb, TargetName);

		NewPrompt.bEnabled = bCanInteract;
		NewPrompt.Verb = Verb;
		NewPrompt.TargetName = TargetName;
	}

	Prompt = NewPrompt;
}

bool UL2H_InteractionComponent::ImplementsInteractable(const AActor* Candidate)
{
	return Candidate != nullptr && Candidate->GetClass()->ImplementsInterface(UL2H_InteractableInterface::StaticClass());
}

bool UL2H_InteractionComponent::Interact()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// The guard is the point: a client that got this far would run world logic twice, once here
		// and once again when the server's copy ran, which for a pickup means two items from one press.
		return false;
	}

	if (!ImplementsInteractable(FocusedActor))
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("%s pressed interact with nothing focused"), *GetNameSafe(Owner)));
		return false;
	}

	bool bCanInteract = false;
	FText Verb;
	FText TargetName;
	IL2H_InteractableInterface::Execute_L2H_GetInteractPrompt(FocusedActor, bCanInteract, Verb, TargetName);

	if (!bCanInteract)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Interact refused: %s -- %s is focused but will not accept it right now"),
			*GetNameSafe(Owner),
			*GetNameSafe(FocusedActor)));
		return false;
	}

	AActor* Target = FocusedActor;
	IL2H_InteractableInterface::Execute_L2H_Interact(Target, Owner);

	L2HTime::LogImportant(FString::Printf(TEXT("Interact: %s -> %s"), *GetNameSafe(Owner), *GetNameSafe(Target)));

	// The object may have changed state (opened, taken, destroyed), so the prompt is re-derived at
	// once instead of waiting out the refresh interval with a stale verb on screen.
	RefreshFocus();
	return true;
}

void UL2H_InteractionComponent::RequestInteract()
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Interact();
		return;
	}

	ServerInteract();
}

void UL2H_InteractionComponent::ServerInteract_Implementation()
{
	Interact();
}

void UL2H_InteractionComponent::OnRep_FocusedActor()
{
	L2HTime::LogVerbose(FString::Printf(TEXT("%s interaction focus replicated: %s"), *GetNameSafe(GetOwner()), *GetNameSafe(FocusedActor)));
	OnFocusChanged.Broadcast(FocusedActor, PreviousFocusedActor);
}

void UL2H_InteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UL2H_InteractionComponent, FocusedActor);
	DOREPLIFETIME(UL2H_InteractionComponent, Prompt);
}
