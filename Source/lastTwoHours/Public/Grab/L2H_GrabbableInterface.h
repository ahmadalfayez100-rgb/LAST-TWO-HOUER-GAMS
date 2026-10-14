// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "L2H_GrabbableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UL2H_GrabbableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Marks an actor as hookable and carryable.
 *
 * Deliberately an interface rather than a base class, and deliberately separate from
 * UL2H_MassProfileComponent: the component carries the data -- including whether an object may be
 * grabbed at all -- while this carries the behaviour, so a crate, a door or a lever can each opt in
 * without sharing a parent.
 *
 * Both functions are void BlueprintNativeEvents, so UHT's generated default is a no-op and Execute_
 * is safe on an actor that implements the interface but overrides nothing.
 */
class LASTTWOHOURS_API IL2H_GrabbableInterface
{
	GENERATED_BODY()

public:
	/** The hook hit this actor, or the carry attached it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Grab")
	void OnGrabbed(AActor* Grabber);

	/** The carrier let go. Physics and collision are already restored when this fires. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "L2H|Grab")
	void OnReleased(AActor* Grabber);
};
