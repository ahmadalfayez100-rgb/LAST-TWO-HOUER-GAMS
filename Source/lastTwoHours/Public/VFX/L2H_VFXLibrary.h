// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VFX/L2H_VFXTypes.h"

#include "L2H_VFXLibrary.generated.h"

/**
 * One character's visual language, expressed as data rather than as branches.
 *
 * Yousef and Samer are the same pawn running the same movement; what separates them is which systems
 * this library names and which two colours those systems are tinted with. A new move is a new row
 * here and never a new node in a graph, and changing a pawn's identity is changing one object
 * reference -- which is what lets the identity be tested on one pawn and moved to another later
 * without redoing the work.
 */
UCLASS(BlueprintType)
class LASTTWOHOURS_API UL2H_VFXLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The two colours this character's effects are built from. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|VFX")
	FL2H_VFXIdentityLook IdentityLook;

	/**
	 * Every moment this character can produce, indexed by (moment, surface).
	 *
	 * A row authored on SurfaceType_Default is the row every other surface falls back to, so a floor
	 * that was never assigned a physical material still gets this character's look instead of nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|VFX")
	TMap<FL2H_VFXKey, FL2H_VFXEntry> Entries;

	/**
	 * Rows added on top of Entries, each carrying its own key.
	 *
	 * Entries is keyed by a struct and a struct cannot be an object key in the JSON the out-of-editor
	 * property path speaks, so a moment whose effect has to exist before anyone ever opens this asset
	 * in the editor can only be added here. The map is read first, so a row a designer authored by
	 * hand is never shadowed by one a tool wrote.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|VFX")
	TArray<FL2H_VFXKeyedEntry> AddedEntries;

	/**
	 * The row to play, or null when this library has nothing for this moment.
	 *
	 * Exact surface first, then the SurfaceType_Default row for the same moment. Returns a pointer
	 * rather than a copy because the caller reads two or three fields of it; the Blueprint path is
	 * TryResolve below.
	 */
	const FL2H_VFXEntry* Resolve(EL2H_VFXMoment Moment, EPhysicalSurface Surface) const;

	/** The Blueprint-facing form of Resolve. False leaves OutEntry untouched. */
	UFUNCTION(BlueprintCallable, Category = "L2H|VFX")
	bool TryResolve(EL2H_VFXMoment Moment, EPhysicalSurface Surface, FL2H_VFXEntry& OutEntry) const;

	/**
	 * Writes every row this library holds to the log.
	 *
	 * A data asset has no readable listing from outside the editor, so without this the only way to
	 * find out why a moment played nothing is to open the asset and look. Prints the map's rows and the
	 * added ones, with the fields that change what a row does.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|VFX")
	void LogEntries() const;
};
