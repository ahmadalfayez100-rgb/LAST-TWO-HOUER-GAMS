// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_TimeDamageComponent.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"

UL2H_TimeDamageComponent::UL2H_TimeDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Damage is decided on the server; there is nothing for this component to own on a client.
	SetIsReplicatedByDefault(false);
}

void UL2H_TimeDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bDamageOnOverlap)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	if (!Root)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeDamage: %s has no primitive root -- overlap damage disabled"), *GetNameSafe(Owner)));
		return;
	}

	WatchedPrimitive = Root;
	Root->SetGenerateOverlapEvents(true);
	Root->OnComponentBeginOverlap.AddDynamic(this, &UL2H_TimeDamageComponent::HandleBeginOverlap);
}

void UL2H_TimeDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WatchedPrimitive)
	{
		WatchedPrimitive->OnComponentBeginOverlap.RemoveDynamic(
			this, &UL2H_TimeDamageComponent::HandleBeginOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void UL2H_TimeDamageComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ApplyTo(OtherActor);
}

void UL2H_TimeDamageComponent::ApplyTo(AActor* Target)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Target || DamageAmount <= 0.0f)
	{
		return;
	}

	// Only a TimeCore pawn holds a clock worth taking time from.
	if (!Cast<AL2H_BaseCharacter>(Target))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	if (const float* LastHit = LastHitTimes.Find(Target))
	{
		if (Now - *LastHit < RetriggerDelay)
		{
			return;
		}
	}
	LastHitTimes.Add(Target, Now);

	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		return;
	}

	L2HTime::LogVerbose(FString::Printf(
		TEXT("TimeDamage: %s hit %s for %.1fs"),
		*GetNameSafe(Owner), *GetNameSafe(Target), DamageAmount));

	Director->ApplyTimeDamage(Target, DamageAmount);
}
