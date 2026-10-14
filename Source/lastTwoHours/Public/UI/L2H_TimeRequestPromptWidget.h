// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "L2H_TimeRequestPromptWidget.generated.h"

class UTextBlock;

/**
 * The partner's answer surface for a time request: a small strip at the edge of their own view.
 *
 * Deliberately not a modal. The partner may be mid-jump when the request lands, so the prompt never
 * takes input focus and never covers the screen -- it sits at the edge and the answer comes from two
 * input actions (accept / decline) that the pawn already listens for. Declining by doing nothing is a
 * supported outcome: the request lapses on its own after the config's RequestTimeoutSeconds.
 */
UCLASS(Abstract, meta = (DisplayName = "L2H Time Request Prompt Widget"))
class LASTTWOHOURS_API UL2H_TimeRequestPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows or hides the strip. Called from the pawn's replicated request state. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Request")
	void SetRequest(bool bInActive, const FText& InRequesterName, float InAmount, float InCost);

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	bool bRequestActive = false;

	/** Fired after every change, for Blueprint-side polish (fade, sound). */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Request")
	void OnPromptChanged(bool bActive, const FText& RequesterName, float Amount, float Cost);

protected:
	/** "Yousef asks for 20 s" -- the one line that has to be readable at a glance. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request", meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;

	/** The accept / decline key hints, so neither player has to be told out of band. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request", meta = (BindWidget))
	TObjectPtr<UTextBlock> HintText;
};
