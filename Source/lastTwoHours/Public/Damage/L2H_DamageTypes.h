// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "CoreMinimal.h"

#include "L2H_DamageTypes.generated.h"

class UAnimMontage;
class UParticleSystem;
class USoundBase;

/**
 * What hit the player, not how much it took.
 *
 * TimeCore has no health: time is life, so a damage type is the classification that decides how much
 * time is removed, which surface response plays and which camera shake the shooter feels. Anything
 * that removes time still goes through AL2H_TimeDirector, so this enum never becomes a second
 * damage pathway -- it is the lookup key for a UL2H_DamageTypeDataAsset.
 */
UENUM(BlueprintType)

enum class EL2H_DamageType : uint8
{
	Pipe = 0 UMETA(DisplayName = "Pipe"),
	Pistol UMETA(DisplayName = "Pistol"),
	Rifle UMETA(DisplayName = "Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun"),
	Bite UMETA(DisplayName = "Bite")
};

/**
 * One damage type's response, as authored data.
 *
 * ShardSpillChance is a designer number rather than a derived one: the director already spills when a
 * single hit crosses the level's ShardThreshold, and this is the extra chance a heavy type has of
 * knocking a shard loose from a target that did not cross it.
 */
USTRUCT(BlueprintType)
struct FL2H_DamageResponse
{
	GENERATED_BODY()

	/**
	 * Damage per hit, in health points, before the headshot multiplier.
	 *
	 * Health points and not seconds, because Health is what a hit lands on now: the director takes
	 * this much Health and then charges TimePerHealthPoint seconds for each point it actually removed.
	 * A response value that is larger than the target has left therefore costs only what was there to
	 * take, rather than billing for a life the target did not have.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "0.0"))
	float TimeDamage = 20.0f;

	/** 0..1. Rolled once per hit on top of the director's own threshold rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShardSpillChance = 0.0f;

	/** Applied when the trace hits the HeadShot socket or the head bone. 1 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 2.0f;

	/** Played on the shooter's own camera only. Never replicated: a shake is a local presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	/** Reaction montage on whoever took the hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	TObjectPtr<UAnimMontage> HitMontage;

	/** Impact effect at the hit location, spawned by the server and replicated as an FX actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	TObjectPtr<UParticleSystem> ImpactEffect;

	/** Impact sound at the hit location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	TObjectPtr<USoundBase> ImpactSound;
};
