// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "L2H_InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UL2H_InteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * What the player's interact press needs from a world object.
 *
 * Deliberately three void functions with out-params, and deliberately no _Implementation bodies in this
 * file. UHT generates the empty defaults for an interface's BlueprintNativeEvent, and those defaults
 * are what an unfinished implementer falls back to. A function that RETURNED bool would therefore
 * default to false, which every caller reads as "this object refuses everyone" -- a silent bug. An
 * out-param has no such default, so an unfinished implementer is visibly unfinished instead of
 * quietly unusable.
 *
 * The _Implementation bodies belong to the actors that implement this, never to the interface's own
 * .cpp; declaring one here as well would collide with the generated one (C2084).
 */
class LASTTWOHOURS_API IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Fills the HUD prompt for this object.
	 *
	 * bCanInteract is the refusal channel: a locked door still fills Verb and OutTargetName so the
	 * widget can show "Locked -- Rusted Locker" in a disabled colour, and sets bCanInteract false so
	 * the press is refused. Refusing therefore never has to be spelled out as an empty prompt.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Interaction")
	void L2H_GetInteractPrompt(bool& bCanInteract, FText& OutVerb, FText& OutTargetName);

	/** Performs the interaction. Called on the server only. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Interaction")
	void L2H_Interact(AActor* Interactor);

	/**
	 * This object's stable identity for the save layer.
	 *
	 * A GUID rather than the actor pointer, because a pickup or an unlocked door has to survive a level
	 * reload and a pointer does not. Objects that are not persisted may leave the out-param invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Interaction")
	void L2H_GetWorldStateId(FGuid& OutId);

	/**
	 * Reports this object's own state as name/flag pairs, for the save layer to keep.
	 *
	 * Flags rather than fields, because the save layer has no business knowing that a door has an "open"
	 * and a "locked" while a drawer has only a "locked". A new interactable saves without the save format
	 * changing, and an old slot still loads after an actor gains a flag it did not use to have.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Interaction")
	void L2H_CaptureWorldState(TMap<FName, bool>& OutFlags);

	/**
	 * Puts this object back into the state a recorded capture described.
	 *
	 * Must read only the names it recognises and ignore the rest: a restore has to tolerate a record
	 * written by a version of this actor that had a flag the current one does not, or the reverse.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Interaction")
	void L2H_ApplyWorldState(const TMap<FName, bool>& InFlags);
};
