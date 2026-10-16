// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "L2H_WorldTimeSubsystem.generated.h"

/**
 * Server-authoritative world and per-actor time dilation.
 *
 * AWorldSettings::TimeDilation is replicated, so writing it on the server is enough for
 * every client to follow. There is deliberately no client-side path in here.
 *
 * CustomTimeDilation only scales an actor's Tick delta, so this covers tick-driven actors
 * (doors, platforms, characters) -- and, through the world dilation, simulated physics too,
 * because the physics tick is fed the already-dilated frame delta.
 */
UCLASS()
class LASTTWOHOURS_API UL2H_WorldTimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Slows the world; the caster is compensated to keep ticking at normal speed. Server only. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void ApplyWorldSlow(float Dilation, AActor* Caster);

	/** Restores the dilation cached before the first slow. Pass null to restore every caster. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void EndWorldSlow(AActor* Caster);

	/** Per-actor slow for tick-driven actors (doors, platforms). Server only. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void ApplyActorSlow(const TArray<AActor*>& Targets, float Dilation);

	/** Restores the cached CustomTimeDilation of the passed actors. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void RestoreActorSlow(const TArray<AActor*>& Targets);

	/** AWorldSettings::GetEffectiveTimeDilation(). Returns 1.0 when nothing is slowed. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	float GetEffectiveDilation() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool IsWorldSlowed() const;
	
	/**
	 * True while any actor is individually slowed.
	 *
	 * A Time Bend arena slows actors one at a time instead of moving the world's dilation, so the world
	 * can read as untouched while a bend is in full swing. Anything asking "is time already bent?" has to
	 * ask this as well as IsWorldSlowed, or two slows will end up owning the same actors.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool IsAnyActorSlowed() const { return CachedActorDilation.Num() > 0; }

private:
	/** World dilation captured before the first slow. <= 0 means "not currently slowed". */
	float CachedWorldDilation = -1.0f;

	/** Original CustomTimeDilation per touched actor. Weak keys, so nothing is kept alive. */
	TMap<TWeakObjectPtr<AActor>, float> CachedActorDilation;

	/** Casters compensated to normal speed for the duration of the slow. */
	TArray<TWeakObjectPtr<AActor>> CompensatedCasters;
};
