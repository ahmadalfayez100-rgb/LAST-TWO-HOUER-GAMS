// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "L2H_InteractionTypes.generated.h"

/**
 * What the focused object is, from the interaction layer's point of view.
 *
 * One enum rather than a class hierarchy: the interaction component needs to decide how to phrase a
 * prompt and whether a press is a pickup, an open or a turn, and that decision does not justify a
 * different actor class per verb.
 */
UENUM(BlueprintType)
enum class EL2H_ObjectType : uint8
{
	Undefined = 0 UMETA(DisplayName = "Undefined"),
	Pickup UMETA(DisplayName = "Pickup"),
	Door UMETA(DisplayName = "Door"),
	Drawer UMETA(DisplayName = "Drawer"),
	Locker UMETA(DisplayName = "Locker"),
	Readable UMETA(DisplayName = "Readable"),
	Lock UMETA(DisplayName = "Lock"),
	Storage UMETA(DisplayName = "Storage")
};

/** Classification of a pickup, used by the inventory to place it and by loot tables to select it. */
UENUM(BlueprintType)
enum class EL2H_PickupType : uint8
{
	Ammo = 0 UMETA(DisplayName = "Ammo"),
	Note UMETA(DisplayName = "Note"),
	Potion UMETA(DisplayName = "Potion"),
	Weapon UMETA(DisplayName = "Weapon"),
	Key UMETA(DisplayName = "Key"),
	Tool UMETA(DisplayName = "Tool"),
	InspectOnly UMETA(DisplayName = "Inspect Only")
};

/**
 * The HUD's view of whatever the interaction component is currently looking at.
 *
 * Built on the server and replicated, because the prompt has to name the same object for both
 * players and only the server may decide what is in range.
 */
USTRUCT(BlueprintType)
struct FL2H_InteractionPrompt
{
	GENERATED_BODY()

	/** The verb, e.g. "Read", "Open", "Pick up". Empty means nothing is focused. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Interaction")
	FText Verb;

	/** What the verb applies to, so the widget can read "Open -- Rusted Locker". */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Interaction")
	FText TargetName;

	/** False when the object is focused but refuses right now (locked, no key, over weight). */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Interaction")
	bool bEnabled = false;

	bool IsSet() const { return !Verb.IsEmpty(); }
};
