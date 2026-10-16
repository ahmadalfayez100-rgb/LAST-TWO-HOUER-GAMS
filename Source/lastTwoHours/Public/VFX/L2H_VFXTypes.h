// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ChaosEngineInterface.h"
#include "CoreMinimal.h"

#include "L2H_VFXTypes.generated.h"

class UNiagaraSystem;

/**
 * Every moment that may produce a contact effect.
 *
 * The list is deliberately a closed set rather than a gameplay tag: a tag would let a typo create a
 * new moment that nothing ever answers, while an enum makes a missing row a compile-visible name.
 * The rule for what belongs here is contact, acceleration, impact, ability activation or discovery --
 * never idle, breathing, ordinary walking, a plain direction change, crouching, aiming, reloading or
 * carrying a box.
 */
UENUM(BlueprintType)
enum class EL2H_VFXMoment : uint8
{
	None		UMETA(DisplayName = "None"),
	SprintStart	UMETA(DisplayName = "Sprint Start"),
	StopHard	UMETA(DisplayName = "Hard Stop"),
	Jump		UMETA(DisplayName = "Jump"),
	LandLight	UMETA(DisplayName = "Light Land"),
	LandHeavy	UMETA(DisplayName = "Heavy Land"),
	Footstep	UMETA(DisplayName = "Footstep"),
	Roll		UMETA(DisplayName = "Roll"),
	DiveRoll	UMETA(DisplayName = "Dive Roll"),
	Backstep	UMETA(DisplayName = "Backstep"),
	Slide		UMETA(DisplayName = "Slide"),
	Mantle		UMETA(DisplayName = "Mantle"),
	LedgeCatch	UMETA(DisplayName = "Ledge Catch"),
	WallRun		UMETA(DisplayName = "Wall Run"),
	LadderPole	UMETA(DisplayName = "Ladder or Pole"),
	BarSwing	UMETA(DisplayName = "Bar Swing"),
	BeamWalk	UMETA(DisplayName = "Beam Walk"),
	Hook		UMETA(DisplayName = "Hook"),
	TimeBend	UMETA(DisplayName = "Time Bend"),
	TimeBendDome	UMETA(DisplayName = "Time Bend Dome"),
	TimeBendArena	UMETA(DisplayName = "Time Bend Arena"),
	ChronoBall	UMETA(DisplayName = "Chrono Ball"),
	WeaponFire	UMETA(DisplayName = "Weapon Fire"),
	Impact		UMETA(DisplayName = "Impact"),
};

/**
 * One row of a character's library: one ready-made system, where it attaches, and how it is tinted.
 *
 * A soft reference because a pawn only ever plays a handful of its rows on any given frame, and a
 * hard pointer would pull every system in the table into memory the moment the pawn spawns.
 */
USTRUCT(BlueprintType)
struct FL2H_VFXEntry
{
	GENERATED_BODY()

	/** The effect itself. A row with no system is an authored hole, not a fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	TSoftObjectPtr<UNiagaraSystem> System;

	/** Where on the character it attaches when bAttachToSocket is set. NAME_None means the mesh root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FName SocketName = NAME_None;

	/** Uniform scale handed to the system's User.Scale, so one asset can serve two characters at different sizes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;

	/** Added to the attach rotation. A hard landing ring authored flat needs 90 degrees of pitch to stand up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** Handed to the system's User.PaletteColor, which is what makes two libraries share one system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FLinearColor PaletteColor = FLinearColor::White;

	/** Seconds before this row may spawn again on this machine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.08f;

	/**
	 * Seconds the spawned component may live before it is taken down by hand.
	 *
	 * A system authored to loop never reports completion, and a component with bAutoDestroy only dies
	 * when its system does -- so an endlessly looping row would leave one live component per spawn for
	 * the rest of the session. Three seconds outlasts every one-shot row in the library and still bounds
	 * the looping ones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX", meta = (ClampMin = "0.0"))
	float LifetimeSeconds = 3.0f;

	/** True attaches the effect to the character socket; false spawns it at the world contact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	bool bAttachToSocket = true;
};

/**
 * The (moment, surface) pair that indexes a library row.
 *
 * The surface is the engine's own EPhysicalSurface rather than a parallel enum, so a row is authored
 * against the same value the physics material already carries and there is no second place for the
 * two to disagree.
 */
USTRUCT(BlueprintType)
struct FL2H_VFXKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	EL2H_VFXMoment Moment = EL2H_VFXMoment::None;

	/** SurfaceType_Default is the universal row: every surface without its own row falls back to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	bool operator==(const FL2H_VFXKey& Other) const
	{
		return Moment == Other.Moment && Surface == Other.Surface;
	}
};

FORCEINLINE uint32 GetTypeHash(const FL2H_VFXKey& Key)
{
	return HashCombine(
		GetTypeHash(static_cast<uint8>(Key.Moment)),
		GetTypeHash(static_cast<uint8>(Key.Surface.GetValue())));
}

/**
 * One row with its own key carried alongside it, for a list a row can be added to from outside the
 * editor.
 *
 * Entries is a map keyed by a struct, and a struct cannot be an object key in the JSON the
 * out-of-editor property path speaks -- so a row that has to exist before anyone ever opens the asset
 * can only be added as one of these. Same row, same key, spelled so either path can write it.
 */
USTRUCT(BlueprintType)
struct FL2H_VFXKeyedEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FL2H_VFXKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FL2H_VFXEntry Entry;
};

/** The two colours that separate one character's effects from the other's. */
USTRUCT(BlueprintType)
struct FL2H_VFXIdentityLook
{
	GENERATED_BODY()

	/** The colour a moment of contact is built from. Yousef cyan, Samer sand-gold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FLinearColor Primary = FLinearColor(0.247f, 0.784f, 0.878f);

	/** The second colour, used for the moments that are the character's signature rather than a contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|VFX")
	FLinearColor Secondary = FLinearColor(0.784f, 0.478f, 0.180f);
};
