// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "L2H_MusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * The level's music, per machine. An ambient track plays from the start; while an enemy is alive within
 * the level config's MusicCombatRadius of a player on this machine it crossfades to the combat track, and
 * back after MusicCombatHold seconds of quiet. The tracks and numbers live in DA_L2H_LevelTimeConfig, so a
 * level changes its music without code.
 *
 * Music is not replicated: each machine decides from what it can see, which is what a listener expects.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_MusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Plays a different ambient track from now on (an area's own music). Null restores the level's. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Music")
	void SetAmbientOverride(USoundBase* Track);

	UFUNCTION(BlueprintPure, Category = "L2H|Music")
	bool IsCombatMusic() const { return bCombat; }

private:
	UAudioComponent* StartTrack(USoundBase* Track);
	void SwitchTo(USoundBase* Track);
	bool IsEnemyNear() const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Current;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CurrentTrack;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> AmbientOverride;

	bool bStarted = false;
	bool bCombat = false;
	float LastEnemySeen = -1000.0f;
	float NextScan = 0.0f;
};
