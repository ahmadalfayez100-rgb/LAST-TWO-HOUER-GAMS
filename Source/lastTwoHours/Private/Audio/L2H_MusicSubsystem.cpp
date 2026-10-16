// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/L2H_MusicSubsystem.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/AudioComponent.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"

bool UL2H_MusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game worlds only (PIE included); a dedicated server has nobody to play to.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UL2H_MusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bStarted = true;
}

void UL2H_MusicSubsystem::Deinitialize()
{
	if (Current)
	{
		Current->Stop();
		Current = nullptr;
	}
	Super::Deinitialize();
}

TStatId UL2H_MusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UL2H_MusicSubsystem, STATGROUP_Tickables);
}

void UL2H_MusicSubsystem::SetAmbientOverride(USoundBase* Track)
{
	AmbientOverride = Track;
}

void UL2H_MusicSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!bStarted || !World)
	{
		return;
	}

	// The level config is read through the director, which may arrive a moment after begin play.
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(World);
	if (!Config)
	{
		return;
	}

	const float Now = World->GetRealTimeSeconds();
	if (Now >= NextScan)
	{
		// A few times a second is plenty for music.
		NextScan = Now + 0.25f;
		if (IsEnemyNear())
		{
			LastEnemySeen = Now;
		}
	}

	bCombat = Config->CombatMusic && Now - LastEnemySeen < Config->MusicCombatHold;
	USoundBase* Wanted = bCombat ? Config->CombatMusic.Get() : (AmbientOverride ? AmbientOverride.Get() : Config->AmbientMusic.Get());
	if (Wanted != CurrentTrack)
	{
		SwitchTo(Wanted);
	}
}

bool UL2H_MusicSubsystem::IsEnemyNear() const
{
	UWorld* World = GetWorld();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(World);
	if (!World || !Config || Config->MusicCombatRadius <= 0.0f)
	{
		return false;
	}

	// The players this machine shows: its locally controlled pawns (both, in split screen).
	TArray<FVector, TInlineAllocator<2>> Listeners;
	for (TActorIterator<AL2H_BaseCharacter> It(World); It; ++It)
	{
		if (It->IsLocallyControlled())
		{
			Listeners.Add(It->GetActorLocation());
		}
	}

	const float RadiusSq = FMath::Square(Config->MusicCombatRadius);
	for (TActorIterator<AL2H_EnemyRobot> It(World); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		for (const FVector& Listener : Listeners)
		{
			if (FVector::DistSquared(Listener, It->GetActorLocation()) <= RadiusSq)
			{
				return true;
			}
		}
	}
	return false;
}

UAudioComponent* UL2H_MusicSubsystem::StartTrack(USoundBase* Track)
{
	UWorld* World = GetWorld();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(World);
	if (!World || !Track || !Config)
	{
		return nullptr;
	}

	UAudioComponent* Audio = UGameplayStatics::CreateSound2D(World, Track, Config->MusicVolume, 1.0f, 0.0f,
		nullptr, /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);
	if (Audio)
	{
		Audio->bIsUISound = false;
		Audio->FadeIn(Config->MusicFadeTime, Config->MusicVolume);
	}
	return Audio;
}

void UL2H_MusicSubsystem::SwitchTo(USoundBase* Track)
{
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetWorld());
	const float Fade = Config ? Config->MusicFadeTime : 2.0f;

	if (Current)
	{
		// Fades out and is then released; the new one fades in over it.
		Current->FadeOut(Fade, 0.0f);
		Current->bAutoDestroy = true;
		Current = nullptr;
	}
	CurrentTrack = Track;
	Current = StartTrack(Track);

	L2HTime::LogVerbose(FString::Printf(TEXT("Music: %s"), *GetNameSafe(Track)));
}
