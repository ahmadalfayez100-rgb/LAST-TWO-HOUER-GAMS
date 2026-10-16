// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "L2H_StruggleWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * The grab prompt, low in the middle of the screen: what to do, and for the one held a bar that fills with
 * each press. Built in code; a Blueprint child may author its own tree.
 */
UCLASS(meta = (DisplayName = "L2H Struggle Widget"))
class LASTTWOHOURS_API UL2H_StruggleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Progress < 0 hides the bar (the partner's view). */
	void SetState(const FText& InText, float Progress);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;
};
