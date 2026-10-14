// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "Damage/L2H_DamageTypes.h"

#include "L2H_WeaponTypes.generated.h"

class UAnimationAsset;
class UGameplayAbility;
class USkeletalMesh;
class UStaticMesh;
class AL2H_Projectile;

/** Gun, pipe or fists. Unarmed is a real entry so a holstered state has a name. */
UENUM(BlueprintType)
enum class EL2H_WeaponType : uint8
{
	Unarmed = 0 UMETA(DisplayName = "Unarmed"),
	Pipe UMETA(DisplayName = "Pipe"),
	Pistol UMETA(DisplayName = "Pistol"),
	Rifle UMETA(DisplayName = "Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun"),
	Hook UMETA(DisplayName = "Hook")
};

/**
 * Where the weapon mesh lives, in the two states it can be in.
 *
 * Two sockets rather than one plus an offset: the equipped socket is on the hand and the holstered
 * socket is on the body, and the animation drives neither, so the component attaches to whichever one
 * the state names. Both names travel to every machine, because attachment is not replicated.
 */
USTRUCT(BlueprintType)
struct FS_L2H_WeaponSocket
{
	GENERATED_BODY()

	/** On the character's body, e.g. back_pistol. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FName Holster = NAME_None;

	/** On the hand, e.g. hand_r. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FName Equip = NAME_None;
};

/**
 * The montages and animations one weapon plays.
 *
 * Split from the numeric data so a weapon skin can be re-authored without touching its ballistics.
 */
USTRUCT(BlueprintType)
struct FS_L2H_WeaponAnimations
{
	GENERATED_BODY()

	/** Melee combo, in order. Empty for a firearm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TArray<TObjectPtr<UAnimMontage>> CharacterAttack;

	/** Fired on the shooter's own mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UAnimMontage> CharacterFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UAnimMontage> CharacterReload;

	/** Played on the weapon's own skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UAnimationAsset> WeaponFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UAnimationAsset> WeaponReload;
};

/**
 * Everything one weapon is, as authored data.
 *
 * The shape of the reference framework's S_WeaponBaseData, with two deliberate edits: the damage is a
 * number of seconds rather than a health value, and which kind of damage it is has been pulled out
 * into EL2H_DamageType so the response (shake, impact, montage) is looked up instead of duplicated per
 * weapon.
 */
USTRUCT(BlueprintType)
struct FL2H_WeaponBaseData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	EL2H_WeaponType Type = EL2H_WeaponType::Unarmed;

	/** Which damage row this weapon fires. Selects the response in UL2H_DamageTypeDataAsset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	EL2H_DamageType DamageType = EL2H_DamageType::Pistol;

	/** Held fire while the trigger is down, at the rate below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	bool bAutomatic = false;

	/** Shots per second. 0 means one shot per press, whatever the timer says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0.0"))
	float FireRate = 5.0f;

	/** Trace length in centimetres from the fire socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "100.0"))
	float FireRange = 8000.0f;

	/** Seconds removed per hit, before the damage row's headshot multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	/**
	 * When set, the fire button activates this ability instead of shooting. The hook is the case: it sits
	 * in the loadout and is switched to like a gun, but "firing" it throws the hook. Clip and damage are
	 * ignored for such a weapon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TSubclassOf<UGameplayAbility> FireAbilityOverride;

	/** Rounds in a magazine. 0 means the weapon never needs reloading (the pipe). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0"))
	int32 ClipSize = 0;

	/**
	 * Seconds of the player's clock each round costs. Paid at the reload, for the rounds put in, so the
	 * price is a decision made (and shown) before the fight rather than a drain on the trigger. 0 = free.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0.0"))
	float SecondsPerRound = 1.0f;

	/** Which ammo item a reload consumes. Empty means reload is free. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FName AmmoItemName = NAME_None;

	/** Socket the trace leaves from, on the third-person mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FName FireSocketName = NAME_None;

	/**
	 * When set, a shot is a physical round: a projectile that flies with gravity from the muzzle toward the
	 * crosshair and deals its damage where it lands. Unset keeps the instant trace.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TSubclassOf<AL2H_Projectile> ProjectileClass;

	/** Muzzle speed of that round, cm/s. Slow enough to see the drop over a long shot, fast enough to feel like a gun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "500.0"))
	float ProjectileSpeed = 9000.0f;

	/** How far the camera pulls in while aiming down sights. 0 disables the aim zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0.0"))
	float AimFOV = 55.0f;

	/** Walk-speed scale while aiming. Composed through the pawn's cache/change/restore helpers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AimSpeedScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<USkeletalMesh> Mesh;

	/**
	 * The mesh for a weapon that ships as a static mesh rather than a rigged one. The marketplace packs
	 * carry both kinds, so which one a weapon has is a fact about the asset rather than about the design --
	 * but it still has to be authored here, because one actor draws whichever of the two fields is filled.
	 *
	 * Mesh wins when both are set: only a rigged weapon can play its own fire and reload animations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UStaticMesh> StaticMesh;
	
	/**
	 * Scale applied to whichever mesh draws this weapon.
	 *
	 * The marketplace packs were authored at scales that have nothing to do with each other, and the actor
	 * attaches with SnapToTargetNotIncludingScale -- so without this the displayed size is exactly whatever
	 * the exporter happened to use. The Fab rifle is the worked example: 893 cm as authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FVector MeshRelativeScale = FVector(1.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FS_L2H_WeaponSocket Sockets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FS_L2H_WeaponAnimations Animations;
};
