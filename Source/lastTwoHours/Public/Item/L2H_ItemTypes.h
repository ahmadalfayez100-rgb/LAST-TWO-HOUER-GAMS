// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interaction/L2H_InteractionTypes.h"

#include "L2H_ItemTypes.generated.h"

class UL2H_ItemDataAsset;
class UStaticMesh;
class UTexture2D;

/**
 * The authored half of an item: what it is, before anyone has one.
 *
 * This is the shape of the reference framework's S_ItemBaseData, kept field for field so the 32
 * shipped DA_* item assets can be re-authored against it without a translation step.
 */
/** What using an item does. The gameplay layer (the pawn) turns this into the effect. */
UENUM(BlueprintType)
enum class EL2H_ItemUseEffect : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	/** The world stops for EffectMagnitude seconds; both players keep moving. */
	TimeStop UMETA(DisplayName = "Time Stop"),
	/** EffectMagnitude seconds to a partner within EffectRange, standing or on the floor. */
	TimeElixir UMETA(DisplayName = "Time Elixir"),
	/** EffectMagnitude Health back to the user. */
	HealthInjector UMETA(DisplayName = "Health Injector"),
};

USTRUCT(BlueprintType)
struct FL2H_ItemBaseData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	TObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	EL2H_PickupType Type = EL2H_PickupType::InspectOnly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bUsable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bCombinable = false;

	/** Weapons and tools set this so the inventory knows it may live in a shortcut slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bEquipable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bDiscardable = true;

	/** Occupies two adjacent slots, so the inventory treats it as one item spanning two cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	bool bOccupiesTwoSlots = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	int32 Price = 0;

	/** What using it does (see EL2H_ItemUseEffect). Price above is in banked seconds, paid at a hub shop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	EL2H_ItemUseEffect UseEffect = EL2H_ItemUseEffect::None;

	/** Seconds (time stop, elixir) or Health (injector) the effect gives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item", meta = (ClampMin = "0.0"))
	float EffectMagnitude = 0.0f;

	/** How close the partner has to be for an effect aimed at them (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item", meta = (ClampMin = "0.0"))
	float EffectRange = 300.0f;

	/** Seconds handed back to the player when this is used. Routed through the director. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	float HealingAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item", meta = (ClampMin = "1"))
	int32 MaxUses = 1;

	/** Resting offset and rotation when this item is placed in the world as a pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	float PlacementOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	FRotator PlacementRotation = FRotator::ZeroRotator;
};

/**
 * One owned copy of an item.
 *
 * InstanceId is the load-bearing field: the save layer and the world's pickups both identify an item
 * by this GUID rather than by an actor pointer, because a pickup has to survive a level reload and an
 * actor does not. Two copies of a stackable item are one stack with one Id, which is why Quantity sits
 * here and not on the data asset.
 */
USTRUCT(BlueprintType)
struct FL2H_ItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	TObjectPtr<UL2H_ItemDataAsset> Data;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** Stable identity, minted when the instance is created. Never reused. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Item")
	FGuid InstanceId;

	/** Uses left on a tool. Initialised from the data asset's MaxUses. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Item")
	int32 UsesLeft = 1;

	/**
	 * The key this item opens, for a key or keycard. Held as a tag rather than a name so a door can
	 * ask "do I hold the tag this door wants" without either side knowing the other's spelling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Item")
	FGameplayTag KeyTag;

	/** True for the half of a two-slot item that carries no data of its own. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Item")
	bool bSecondaryHalf = false;

	bool IsSet() const { return Data != nullptr; }
};
