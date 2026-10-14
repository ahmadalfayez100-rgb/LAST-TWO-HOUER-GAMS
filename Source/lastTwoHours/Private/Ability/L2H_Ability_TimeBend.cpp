// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_TimeBend.h"

#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2H_WorldTimeSubsystem.h"
#include "TimerManager.h"

UL2H_Ability_TimeBend::UL2H_Ability_TimeBend()
{
	// The dilation is written on the server and replicated from there.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag BendTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.TimeBend")), false);
		if (BendTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(BendTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_TimeBend::ActivateAbility(
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

	if (Character->IsDowned())
	{
		L2HTime::LogImportant(TEXT("TimeBend: refused -- at zero time there is nothing to bend"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float CurrentTime = GetCurrentTime();
	if (CurrentTime < MinimumTimeToStart)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeBend: refused -- %.1fs left, at least %.1fs is needed to start"), CurrentTime, MinimumTimeToStart));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UL2H_WorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UL2H_WorldTimeSubsystem>();
	if (!TimeSubsystem)
	{
		L2HTime::LogImportant(TEXT("TimeBend: refused -- the world time subsystem is missing"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character))
	{
		TimeDirector->ApplyTimeCost(Character, CostFlat);
	}

	BendCaster = Character;

	// Yousef is compensated to normal speed by the subsystem's own caster handling.
	TimeSubsystem->ApplyWorldSlow(BendDilation, Character);

	// One optional object keeps running: its CustomTimeDilation is set to the reciprocal of the world
	// slow, which cancels the slowdown exactly instead of guessing at a number.
	ExemptActors.Reset();
	for (AActor* Actor : FindExemptActors())
	{
		ExemptActors.Add(Actor);
	}

	if (ExemptActors.Num() > 0)
	{
		TArray<AActor*> Exempt;
		Exempt.Reserve(ExemptActors.Num());
		for (const TObjectPtr<AActor>& Actor : ExemptActors)
		{
			Exempt.Add(Actor.Get());
		}

		TimeSubsystem->ApplyActorSlow(Exempt, 1.0f / FMath::Max(BendDilation, 0.01f));
	}

	World->GetTimerManager().SetTimer(
		BendTimer, this, &UL2H_Ability_TimeBend::HandleBendFinished, FMath::Max(BendDuration, 0.1f), false);

	if (CostPerSecond > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			CostTimer, this, &UL2H_Ability_TimeBend::HandleCostTick, FMath::Max(CostTickInterval, 0.1f), true);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("TimeBend: %s slowed the world to %.2f for %.1fs (%d exempt)"),
		*GetNameSafe(Character), BendDilation, BendDuration, ExemptActors.Num()));
}

void UL2H_Ability_TimeBend::HandleCostTick()
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	if (CostPerSecond > 0.0f)
	{
		if (AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character))
		{
			TimeDirector->ApplyTimeCost(Character, CostPerSecond);
		}
	}

	// Paying the last of the time ends the bend on the same frame instead of leaving the world slowed
	// until the duration runs out. Going below zero here would also end it, but through the downed
	// path, which cancels this ability anyway.
	if (GetCurrentTime() <= 0.0f)
	{
		HandleBendFinished();
	}
}

void UL2H_Ability_TimeBend::HandleBendFinished()
{
	const AActor* Caster = BendCaster.Get();
	if (!Caster)
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UL2H_Ability_TimeBend::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Restore first, whatever ended the bend: the world must never be left slowed because the ability
	// that slowed it went away.
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(BendTimer);
		World->GetTimerManager().ClearTimer(CostTimer);

		if (UL2H_WorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UL2H_WorldTimeSubsystem>())
		{
			if (ExemptActors.Num() > 0)
			{
				TArray<AActor*> Exempt;
				Exempt.Reserve(ExemptActors.Num());
				for (const TObjectPtr<AActor>& Actor : ExemptActors)
				{
					Exempt.Add(Actor.Get());
				}

				TimeSubsystem->RestoreActorSlow(Exempt);
			}

			TimeSubsystem->EndWorldSlow(BendCaster.Get());
		}
	}

	ExemptActors.Reset();
	BendCaster.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

TArray<AActor*> UL2H_Ability_TimeBend::FindExemptActors() const
{
	TArray<AActor*> Result;

	UWorld* World = GetWorld();
	if (!World || ExemptActorTag.IsNone())
	{
		return Result;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(ExemptActorTag))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}
