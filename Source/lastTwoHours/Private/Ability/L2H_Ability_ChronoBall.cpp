// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_ChronoBall.h"

#include "Ball/L2H_TimeBall.h"
#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"

UL2H_Ability_ChronoBall::UL2H_Ability_ChronoBall()
{
	// The ball is spawned and resolved on the server, so this never runs on a client.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Instanced per actor so the cooldown survives between throws, and so a second press reaches the
	// same instance rather than a fresh one.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Time is spent through the director's GameplayEffects, never through an ability cost effect.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	BallClass = AL2H_TimeBall::StaticClass();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Registered by the project's tag list. Skipped rather than asserted when it is missing, so a
		// tag list edit can never break the ability from loading.
		const FGameplayTag ChronoTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.ChronoBall")), false);
		if (ChronoTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(ChronoTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_ChronoBall::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();

	if (!Character || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A second press while a ball is out is a decision about that ball, not a new throw. Recall is the
	// baseline, so the ball comes back and the thrower never moves without the flight.
	if (AL2H_TimeBall* Existing = AL2H_TimeBall::FindForOwner(this, Character))
	{
		Existing->HandleRepeatPress();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (Character->IsDowned())
	{
		L2HTime::LogImportant(TEXT("ChronoBall: refused -- at zero time there is nothing to throw"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Cooldown is a gameplay number on the ability rather than a cooldown effect, so a refused press
	// can report exactly how long is left.
	const float CooldownRemaining = FMath::Max(Limits.Cooldown - (World->GetTimeSeconds() - LastThrowTime), 0.0f);
	if (CooldownRemaining > 0.0f)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: refused -- %.1fs of cooldown left"), CooldownRemaining));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float CurrentTime = GetCurrentTime();
	if (CurrentTime <= Limits.CostFlat)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("ChronoBall: refused -- %.1fs of time left, the throw costs %.1fs"), CurrentTime, Limits.CostFlat));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character);

	const FVector Direction = Character->GetControlRotation().Vector();
	const FVector SpawnLocation = Character->GetActorLocation()
		+ Direction * FMath::Max(Limits.ThrowForwardOffset, 0.0f)
		+ FVector(0.0f, 0.0f, Character->BaseEyeHeight * 0.5f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AL2H_TimeBall* Ball = World->SpawnActor<AL2H_TimeBall>(
		BallClass ? BallClass.Get() : AL2H_TimeBall::StaticClass(),
		SpawnLocation, Direction.Rotation(), SpawnParams);

	if (!Ball)
	{
		L2HTime::LogImportant(TEXT("ChronoBall: refused -- the ball could not be spawned"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The throw is paid for the moment it leaves the hand. The airborne part is settled when the ball
	// resolves, and a refusal refunds the flat part.
	if (TimeDirector)
	{
		TimeDirector->ApplyTimeCost(Character, Limits.CostFlat);
	}

	Ball->InitBall(Character, Limits, TimeDirector);
	LastThrowTime = World->GetTimeSeconds();

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s threw a ball for %.1fs"),
		*GetNameSafe(Character), Limits.CostFlat));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
