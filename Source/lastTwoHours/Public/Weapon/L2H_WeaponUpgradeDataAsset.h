// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/L2H_ItemDataAsset.h"

#include "L2H_WeaponUpgradeDataAsset.generated.h"

class UL2H_WeaponDataAsset;

/** What a level of upgrade improves on one weapon. */
UENUM(BlueprintType)
enum class EL2H_WeaponStat : uint8
{
	/** Damage per hit (shots and melee). */
	Firepower UMETA(DisplayName = "Firepower"),
	/** Rounds in a magazine. */
	Capacity UMETA(DisplayName = "Capacity"),
	/** Shots per second. */
	FireRate UMETA(DisplayName = "Fire Rate"),
	/** Seconds a reload charges per round: the upgrade that makes the gun cheaper to keep fed. */
	RoundPrice UMETA(DisplayName = "Round Price"),
	Count UMETA(Hidden),
};

/** One player's upgrade levels for one weapon. */
USTRUCT(BlueprintType)
struct FL2H_WeaponUpgradeRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<UL2H_WeaponDataAsset> Weapon;

	/** Indexed by EL2H_WeaponStat. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Weapon")
	TArray<uint8> Levels;

	int32 GetLevel(EL2H_WeaponStat Stat) const
	{
		const int32 Index = static_cast<int32>(Stat);
		return Levels.IsValidIndex(Index) ? Levels[Index] : 0;
	}
};

/**
 * A weapon upgrade sold in a hub's shop, RE4-style: one stat of one weapon, a level at a time.
 *
 * It is an item only so the shop can list it beside the tools; it never goes into the bag. Buying it raises
 * the buyer's level for that stat (kept on their weapon component, saved with the hub), and each level costs
 * more than the last: Price x (1 + PriceGrowth x levels already bought).
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Weapon Upgrade"))
class LASTTWOHOURS_API UL2H_WeaponUpgradeDataAsset : public UL2H_ItemDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Upgrade")
	TObjectPtr<UL2H_WeaponDataAsset> Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Upgrade")
	EL2H_WeaponStat Stat = EL2H_WeaponStat::Firepower;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Upgrade", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Upgrade", meta = (ClampMin = "0.0"))
	float PriceGrowth = 0.5f;

	/** Banked seconds the next level costs for Buyer, or -1 when there is no next level. */
	int32 GetPriceFor(const AActor* Buyer) const;

	/** The current level Buyer has (0 = none). */
	int32 GetLevelFor(const AActor* Buyer) const;

	/** "Pistol firepower 1/3" -- for the shop row. */
	FText GetShopLabel(const AActor* Buyer) const;

	/**
	 * What any shop entry costs Buyer: the upgrade's next level, or the item's own Price. The one place
	 * the menu and the till both ask, so the row never shows a price the purchase does not charge.
	 */
	static int32 GetShopPrice(const AActor* Buyer, const UL2H_ItemDataAsset* Item);

	/** The shop row's name for Item: the upgrade's label with its level, or the item's DisplayName. */
	static FText GetShopName(const AActor* Buyer, const UL2H_ItemDataAsset* Item);
};
