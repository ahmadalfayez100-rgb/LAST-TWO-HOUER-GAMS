// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_AbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Time/L2HTimeLog.h"

UL2H_AbilitySystemComponent::UL2H_AbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicated(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UL2H_AbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Self-guarded (it early-outs when already initialised) and called automatically from
	// UE 5.3 onward, but calling it here keeps the dependency explicit for the target-actor
	// path the ball and the carry ability rely on.
	UAbilitySystemGlobals::Get().InitGlobalData();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		// Covers the other possibility: this component was created before possession, so the
		// controller arrives later (and can change again on re-possession).
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UL2H_AbilitySystemComponent::HandleControllerChanged);
	}

	// Owner and avatar are both the pawn -- the design decision that keeps this simple.
	InitAbilityActorInfo(Owner, Owner);

	L2HTime::LogVerbose(FString::Printf(TEXT("ASC ready on %s"), *GetNameSafe(Owner)));

	OnTimeSystemReady();
}

void UL2H_AbilitySystemComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	// Owner and avatar are unchanged, but the ability system caches the controller, so the
	// new player would otherwise activate abilities against a stale one.
	InitAbilityActorInfo(GetOwner(), GetOwner());

	L2HTime::LogVerbose(FString::Printf(TEXT("ASC controller changed on %s: %s -> %s"),
		*GetNameSafe(Pawn), *GetNameSafe(OldController), *GetNameSafe(NewController)));
}
