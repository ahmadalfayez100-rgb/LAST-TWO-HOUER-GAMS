// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_ItemDataAsset.generated.h"

/**
 * One item, as an asset.
 *
 * A thin wrapper around FL2H_ItemBaseData rather than a dozen loose properties, because the reference
 * framework's 32 DA_* item assets are all authored against that one struct: re-authoring them becomes
 * copying the rows across instead of re-deciding each field.
 *
 * No behaviour lives here. An item that did something on its own would need to know who picked it up;
 * what an item DOES is the inventory's job and what it LOOKS like is this asset's.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Item Data"))
class LASTTWOHOURS_API UL2H_ItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	FL2H_ItemBaseData Data;
};
