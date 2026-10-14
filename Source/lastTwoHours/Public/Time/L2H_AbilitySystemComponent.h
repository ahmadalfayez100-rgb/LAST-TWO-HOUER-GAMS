// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"

#include "L2H_AbilitySystemComponent.generated.h"

/**
 * Pawn-owned ability system component for the TimeCore layer.
 *
 * Mixed replication: the owner (including a second local split-screen player) gets full
 * effect info while simulated proxies get the minimum -- which is all the HUD needs, since
 * it only ever reads the replicated Time attribute.
 *
 * It lives on the pawn rather than the PlayerState because the design never destroys the
 * pawn (0 time is Downed, not death), which keeps actor-info initialisation simple.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent))
class LASTTWOHOURS_API UL2H_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UL2H_AbilitySystemComponent();

	virtual void BeginPlay() override;

	/** Fired once actor info is valid. Grant the level's ability sets from here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Abilities")
	void OnTimeSystemReady();

protected:
	/**
	 * A locally created second player can be possessed after this component's BeginPlay,
	 * so the ability system has to re-resolve its controller when that happens.
	 */
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
};
