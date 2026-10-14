// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Damage/L2H_DamageTypes.h"
#include "Engine/DataAsset.h"

#include "L2H_DamageTypeDataAsset.generated.h"

/**
 * One damage type's response, as an asset.
 *
 * A thin wrapper around FL2H_DamageResponse, matching the shape UL2H_WeaponDataAsset already uses for
 * FL2H_WeaponBaseData -- re-authoring is copying rows rather than re-deciding each field.
 *
 * These assets are held by the thing being hit, not by the thing doing the hitting. A pistol's asset knows
 * what a pistol is, but it cannot know what a pistol means to every target in the game; the target can.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Damage Type Data"))
class LASTTWOHOURS_API UL2H_DamageTypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Which damage type this is the response to.
	 *
	 * Stated on the asset as well as being the map key, so the asset describes itself when read on its own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	EL2H_DamageType DamageType = EL2H_DamageType::Pistol;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	FL2H_DamageResponse Response;
};
