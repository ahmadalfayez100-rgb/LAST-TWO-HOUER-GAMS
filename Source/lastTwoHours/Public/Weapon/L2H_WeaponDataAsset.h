// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapon/L2H_WeaponTypes.h"

#include "L2H_WeaponDataAsset.generated.h"

/**
 * One weapon, as an asset.
 *
 * A thin wrapper around FL2H_WeaponBaseData, matching the shape the reference framework's weapon assets
 * were authored against -- so re-authoring one is copying rows rather than re-deciding each field.
 *
 * No behaviour lives here. A weapon that fired itself would need to know who was holding it; what a
 * weapon DOES is the fire ability's job and what it looks like is this asset's.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Weapon Data"))
class LASTTWOHOURS_API UL2H_WeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Weapon")
	FL2H_WeaponBaseData Data;

	/** True when the weapon has a magazine that can run out. The pipe deliberately has none. */
	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	bool UsesAmmo() const { return Data.ClipSize > 0; }
};
