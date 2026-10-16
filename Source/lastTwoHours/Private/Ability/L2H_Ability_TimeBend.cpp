// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_TimeBend.h"

#include "Animation/AnimMontage.h"
#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2H_WorldTimeSubsystem.h"
#include "TimerManager.h"
#include "VFX/L2H_VFXTypes.h"

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
	
	// A second slow layered on this one would have two owners fighting over the same actors' dilation and
	// neither could restore cleanly, so the bend refuses while anything at all is already slowed.
	if (TimeSubsystem->IsWorldSlowed() || TimeSubsystem->IsAnyActorSlowed())
	{
		L2HTime::LogImportant(TEXT("TimeBend: refused -- time is already bent"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character))
	{
		TimeDirector->ApplyTimeCost(Character, CostFlat);
	}

	BendCaster = Character;
	
	// One optional object keeps running: it is simply never put in the arena. That is all the exempt tag
	// has to mean once the slow is a list of actors rather than the whole level.
	ExemptActors.Reset();
	for (AActor* Actor : FindExemptActors())
	{
		ExemptActors.Add(Actor);
	}
	
	// Yousef is not compensated, he is left out: the arena is built from everyone else.
	RefreshArena();
	
	// Re-evaluated on a timer rather than fixed at cast. The arena is Yousef's, so it has to take in
	// whoever he walks up to and let go of whoever he walks away from.
	World->GetTimerManager().SetTimer(
		ArenaTimer, this, &UL2H_Ability_TimeBend::RefreshArena,
		FMath::Max(ArenaRefreshInterval, 0.05f), true);
	
	// Held, the bend has no length of its own: the release ends it, and the cap only guards a stuck key.
	BendStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(
		BendTimer, this, &UL2H_Ability_TimeBend::HandleBendFinished,
		FMath::Max(bHoldToSustain ? MaxHoldSeconds : BendDuration, 0.1f), false);

	if (CostPerSecond > 0.0f || bHoldToSustain)
	{
		World->GetTimerManager().SetTimer(
			CostTimer, this, &UL2H_Ability_TimeBend::HandleCostTick,
			FMath::Max(bHoldToSustain ? 0.25f : CostTickInterval, 0.1f), true);
	}

	// The beat that tells the player the world just changed, on every machine. Unreliable and cosmetic:
	// the dilation above is the truth and travels on its own, so a lost pulse costs a flourish only.
	// The arena's own size rides along, so the ring the player sees is the ground that is actually slow.
	Character->MulticastMomentVFX(
		EL2H_VFXMoment::TimeBend, NAME_None, BendDiameter / BendEffectReferenceDiameter);

	// The arena itself, on top of the opening beat: the dome that marks the slowed volume and the dial
	// drawn on the ground that is the circle the slow actually covers. They ride the same call and take
	// the same scale, so the three can never disagree about where the boundary of the bend is.
	Character->MulticastMomentVFX(
		EL2H_VFXMoment::TimeBendDome, NAME_None, BendDiameter / BendEffectReferenceDiameter);
	Character->MulticastMomentVFX(
		EL2H_VFXMoment::TimeBendArena, NAME_None, BendDiameter / BendEffectReferenceDiameter);

	// The casting pose rides the same unreliable path as the beat, and for the same reason: the world
	// is already slowed on every machine whether or not any of them plays it.
	Character->MulticastPlayCharacterMontage(CastMontage, 1.0f, /*bLoop=*/false);

	L2HTime::LogImportant(FString::Printf(
		TEXT("TimeBend: %s slowed %.0fcm across to %.2f for %.1fs (%d inside, %d exempt)"),
		*GetNameSafe(Character), BendDiameter, BendDilation, BendDuration,
		SlowedActors.Num(), ExemptActors.Num()));
}

void UL2H_Ability_TimeBend::HandleCostTick()
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// Held: the clock itself already takes the normal second, so only the extra above 1x is charged here,
	// a quarter second at a time so the counter visibly runs fast rather than jumping once a second.
	const float Charge = bHoldToSustain
		? (GetHoldDrainRate() - 1.0f) * 0.25f
		: CostPerSecond;
	if (Charge > 0.0f)
	{
		if (AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character))
		{
			TimeDirector->ApplyTimeCost(Character, Charge);
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

float UL2H_Ability_TimeBend::GetHoldDrainRate() const
{
	const UWorld* World = GetWorld();
	const float Held = World ? World->GetTimeSeconds() - BendStartTime : 0.0f;
	return FMath::Min(HoldDrainStart + HoldDrainRampPerSecond * FMath::Max(Held, 0.0f), FMath::Max(HoldDrainMax, HoldDrainStart));
}

void UL2H_Ability_TimeBend::ReleaseHold()
{
	UWorld* World = GetWorld();
	if (!bHoldToSustain || !World || !BendCaster.IsValid())
	{
		return;
	}

	const float Remaining = MinBendSeconds - (World->GetTimeSeconds() - BendStartTime);
	L2HTime::LogImportant(FString::Printf(TEXT("TimeBend: released after %.1fs at %.2fx drain"),
		World->GetTimeSeconds() - BendStartTime, GetHoldDrainRate()));
	if (Remaining > 0.05f)
	{
		World->GetTimerManager().SetTimer(BendTimer, this, &UL2H_Ability_TimeBend::HandleBendFinished, Remaining, false);
		return;
	}
	HandleBendFinished();
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
	// Restore first, whatever ended the bend: an actor must never be left slow because the ability that
	// slowed it went away.
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(BendTimer);
		World->GetTimerManager().ClearTimer(CostTimer);
		World->GetTimerManager().ClearTimer(ArenaTimer);
	
		if (UL2H_WorldTimeSubsystem* TimeSubsystem = World->GetSubsystem<UL2H_WorldTimeSubsystem>())
		{
			// The list this bend applied, not the list standing inside the arena right now: an actor that
			// walked out between the last refresh and this moment is still owed its normal speed.
			TArray<AActor*> ToRestore;
			ToRestore.Reserve(SlowedActors.Num());
			for (const TObjectPtr<AActor>& Actor : SlowedActors)
			{
				ToRestore.Add(Actor.Get());
			}
			
			if (ToRestore.Num() > 0)
			{
				TimeSubsystem->RestoreActorSlow(ToRestore);
			}
		}
	}

	AL2H_BaseCharacter* EndingCaster = Cast<AL2H_BaseCharacter>(BendCaster.Get());
	ExemptActors.Reset();
	SlowedActors.Reset();
	BendCaster.Reset();

	// The same beat on the way out, so the end of the bend reads as clearly as its start. The caster is
	// taken before the handle is cleared -- afterwards there is nothing left to tell.
	if (EndingCaster)
	{
		// The arena comes down on the beat that ends the bend, so the slow and the shape drawn around it
		// stop together however the bend ended -- the duration running out, a cancel, or Yousef reaching
		// zero time. Left to their own lifetime they would still be standing over a world that had
		// already sped back up.
		EndingCaster->MulticastStopMomentVFX(EL2H_VFXMoment::TimeBendDome);
		EndingCaster->MulticastStopMomentVFX(EL2H_VFXMoment::TimeBendArena);

		// The same arena size on the way out, so the closing beat closes over the same ground.
		EndingCaster->MulticastMomentVFX(
			EL2H_VFXMoment::TimeBend, NAME_None, BendDiameter / BendEffectReferenceDiameter);

		// Taken off here as well as at the end of its own length, so a bend that ends early -- the cost
		// running Yousef out, or a cancel -- never leaves the casting pose stranded on the mesh.
		EndingCaster->MulticastStopCharacterMontage(CastMontage, 0.25f);
	}

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
	
TArray<AActor*> UL2H_Ability_TimeBend::GatherArenaActors() const
{
	TArray<AActor*> Result;
	
	UWorld* World = GetWorld();
	AActor* Caster = BendCaster.Get();
	if (!World || !Caster)
	{
		return Result;
	}
	
	// Measured from Yousef himself, because the arena is his and travels with him.
	const FVector Origin = Caster->GetActorLocation();
	const float RadiusSquared = FMath::Square(GetBendRadius());
	
	// Walked linearly rather than found with a collision overlap: doors, platforms and lights are exactly
	// what this needs to catch, and a sphere trace would miss every one of them without collision.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == Caster)
		{
			continue;
		}
	
		if (Actor->ActorHasTag(ExemptActorTag))
		{
			continue;
		}
	
		if (FVector::DistSquared(Actor->GetActorLocation(), Origin) <= RadiusSquared)
		{
			Result.Add(Actor);
		}
	}
	
	return Result;
}
	
void UL2H_Ability_TimeBend::RefreshArena()
{
	UWorld* World = GetWorld();
	UL2H_WorldTimeSubsystem* TimeSubsystem = World ? World->GetSubsystem<UL2H_WorldTimeSubsystem>() : nullptr;
	if (!TimeSubsystem)
	{
		return;
	}
	
	// Whoever has left gets his own dilation back -- the subsystem cached it on the way in -- and only
	// then is the new set applied. Restoring first is what stops an actor that crossed the line from
	// being slowed twice over on one refresh, and the cache never compounds: it only ever holds the value
	// that was there before this bend touched the actor.
	TArray<AActor*> ToRestore;
	ToRestore.Reserve(SlowedActors.Num());
	for (const TObjectPtr<AActor>& Actor : SlowedActors)
	{
		ToRestore.Add(Actor.Get());
	}
	
	if (ToRestore.Num() > 0)
	{
		TimeSubsystem->RestoreActorSlow(ToRestore);
	}
	
	SlowedActors.Reset();
	
	TArray<AActor*> Inside = GatherArenaActors();
	for (AActor* Actor : Inside)
	{
		SlowedActors.Add(Actor);
	}
	
	if (Inside.Num() > 0)
	{
		TimeSubsystem->ApplyActorSlow(Inside, BendDilation);
	}
}
