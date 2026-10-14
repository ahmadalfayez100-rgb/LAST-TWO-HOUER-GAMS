// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_WorldTimeSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Time/L2HTimeLog.h"

namespace
{
	/** Guard rail: a dilation of 0 would divide by zero when compensating the caster. */
	constexpr float GL2HMinDilation = 0.01f;
}

void UL2H_WorldTimeSubsystem::Deinitialize()
{
	// Put the dilation back on teardown, so a new PIE session never inherits a slow.
	if (UWorld* World = GetWorld())
	{
		if (CachedWorldDilation > 0.0f)
		{
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->SetTimeDilation(CachedWorldDilation);
			}
		}
	}

	CachedWorldDilation = -1.0f;
	CachedActorDilation.Reset();
	CompensatedCasters.Reset();

	Super::Deinitialize();
}

void UL2H_WorldTimeSubsystem::ApplyWorldSlow(float Dilation, AActor* Caster)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings)
	{
		return;
	}

	const float Clamped = FMath::Clamp(Dilation, GL2HMinDilation, 1.0f);

	if (CachedWorldDilation <= 0.0f)
	{
		CachedWorldDilation = Settings->TimeDilation;
	}

	Settings->SetTimeDilation(Clamped);

	// The caster stays at normal speed. CustomTimeDilation scales this actor's tick delta
	// on top of the world dilation, so 1/Dilation cancels it out exactly.
	if (IsValid(Caster))
	{
		if (!CachedActorDilation.Contains(Caster))
		{
			CachedActorDilation.Add(Caster, Caster->CustomTimeDilation);
		}

		Caster->CustomTimeDilation = 1.0f / Clamped;

		if (!CompensatedCasters.Contains(Caster))
		{
			CompensatedCasters.Add(Caster);
		}
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("ApplyWorldSlow: requested=%.3f applied=%.3f effective=%.3f caster=%s"),
		Dilation, Clamped, Settings->GetEffectiveTimeDilation(), *GetNameSafe(Caster)));
}

void UL2H_WorldTimeSubsystem::EndWorldSlow(AActor* Caster)
{
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		if (AWorldSettings* Settings = World->GetWorldSettings())
		{
			if (CachedWorldDilation > 0.0f)
			{
				Settings->SetTimeDilation(CachedWorldDilation);
				L2HTime::LogImportant(FString::Printf(TEXT("EndWorldSlow: restored=%.3f"), CachedWorldDilation));
			}
		}
	}

	TArray<AActor*> ToRestore;

	if (IsValid(Caster))
	{
		ToRestore.Add(Caster);
		CompensatedCasters.Remove(Caster);
	}
	else
	{
		for (const TWeakObjectPtr<AActor>& WeakCaster : CompensatedCasters)
		{
			if (WeakCaster.IsValid())
			{
				ToRestore.Add(WeakCaster.Get());
			}
		}

		CompensatedCasters.Reset();
	}

	RestoreActorSlow(ToRestore);

	CachedWorldDilation = -1.0f;
}

void UL2H_WorldTimeSubsystem::ApplyActorSlow(const TArray<AActor*>& Targets, float Dilation)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// Above 1 is allowed: an actor exempt from a world slow is given the reciprocal, which cancels the slow
	// exactly. Capping at 1 left every "exempt" actor frozen with the rest of the world.
	const float Clamped = FMath::Clamp(Dilation, GL2HMinDilation, 1.0f / GL2HMinDilation);

	for (AActor* Target : Targets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		if (!CachedActorDilation.Contains(Target))
		{
			CachedActorDilation.Add(Target, Target->CustomTimeDilation);
		}

		Target->CustomTimeDilation = Clamped;
	}
}

void UL2H_WorldTimeSubsystem::RestoreActorSlow(const TArray<AActor*>& Targets)
{
	for (AActor* Target : Targets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		if (float* OriginalDilation = CachedActorDilation.Find(Target))
		{
			Target->CustomTimeDilation = *OriginalDilation;
			CachedActorDilation.Remove(Target);
		}
		else
		{
			Target->CustomTimeDilation = 1.0f;
		}
	}
}

float UL2H_WorldTimeSubsystem::GetEffectiveDilation() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	if (const AWorldSettings* Settings = World->GetWorldSettings())
	{
		return Settings->GetEffectiveTimeDilation();
	}

	return 1.0f;
}

bool UL2H_WorldTimeSubsystem::IsWorldSlowed() const
{
	return CachedWorldDilation > 0.0f;
}
