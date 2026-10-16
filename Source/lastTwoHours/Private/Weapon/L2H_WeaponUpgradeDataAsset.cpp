// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/L2H_WeaponUpgradeDataAsset.h"

#include "GameFramework/Actor.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"

#define LOCTEXT_NAMESPACE "L2HWeaponUpgrade"

int32 UL2H_WeaponUpgradeDataAsset::GetLevelFor(const AActor* Buyer) const
{
	const UL2H_WeaponComponent* Weapons = Buyer ? Buyer->FindComponentByClass<UL2H_WeaponComponent>() : nullptr;
	return Weapons ? Weapons->GetUpgradeLevel(Weapon, Stat) : 0;
}

int32 UL2H_WeaponUpgradeDataAsset::GetPriceFor(const AActor* Buyer) const
{
	const int32 Level = GetLevelFor(Buyer);
	if (Level >= MaxLevel)
	{
		return -1;
	}
	return FMath::RoundToInt(static_cast<float>(Data.Price) * (1.0f + PriceGrowth * Level));
}

FText UL2H_WeaponUpgradeDataAsset::GetShopLabel(const AActor* Buyer) const
{
	static const FText StatNames[] = {
		LOCTEXT("Firepower", "firepower"),
		LOCTEXT("Capacity", "capacity"),
		LOCTEXT("FireRate", "fire rate"),
		LOCTEXT("RoundPrice", "cheaper rounds"),
	};
	const int32 StatIndex = FMath::Clamp(static_cast<int32>(Stat), 0, UE_ARRAY_COUNT(StatNames) - 1);
	const FText WeaponName = Weapon ? Weapon->Data.DisplayName : FText::GetEmpty();
	const int32 Level = GetLevelFor(Buyer);
	if (Level >= MaxLevel)
	{
		return FText::Format(LOCTEXT("LabelMax", "{0} {1} (max)"), WeaponName, StatNames[StatIndex]);
	}
	return FText::Format(LOCTEXT("Label", "{0} {1} {2}/{3}"), WeaponName, StatNames[StatIndex],
		FText::AsNumber(Level + 1), FText::AsNumber(MaxLevel));
}

int32 UL2H_WeaponUpgradeDataAsset::GetShopPrice(const AActor* Buyer, const UL2H_ItemDataAsset* Item)
{
	if (const UL2H_WeaponUpgradeDataAsset* Upgrade = Cast<UL2H_WeaponUpgradeDataAsset>(Item))
	{
		return Upgrade->GetPriceFor(Buyer);
	}
	return Item ? Item->Data.Price : 0;
}

FText UL2H_WeaponUpgradeDataAsset::GetShopName(const AActor* Buyer, const UL2H_ItemDataAsset* Item)
{
	if (const UL2H_WeaponUpgradeDataAsset* Upgrade = Cast<UL2H_WeaponUpgradeDataAsset>(Item))
	{
		return Upgrade->GetShopLabel(Buyer);
	}
	return Item ? Item->Data.DisplayName : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
