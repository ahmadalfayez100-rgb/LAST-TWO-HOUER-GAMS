// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/L2H_ContainerActor.h"

#include "L2H_StorageActor.generated.h"

/**
 * A locker: somewhere the player puts things down and expects to find them again.
 *
 * Deliberately a subclass rather than a second container. The contents, the locking and the whole-array
 * replication all come from AL2H_ContainerActor unchanged, so there is exactly one place where "what is
 * in the box" can be wrong -- and a storage locker is not a different kind of fact, it is a bigger one.
 *
 * Opening it is a UI act. This actor never moves an item itself: the transfer RPCs live on the inventory
 * component, which is the only thing that can keep its own grid consistent with what the world shows.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Storage"))
class LASTTWOHOURS_API AL2H_StorageActor : public AL2H_ContainerActor
{
	GENERATED_BODY()

public:
	AL2H_StorageActor();

	virtual void L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName) override;
	virtual void L2H_Interact_Implementation(AActor* Interactor) override;
};
