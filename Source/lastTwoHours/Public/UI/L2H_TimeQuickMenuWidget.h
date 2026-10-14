// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "L2H_TimeQuickMenuWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * The field menu for time between partners: give, request, answer a request. Opened with D-pad Left (X).
 *
 * Driven by the pawn's own input rather than UI focus: in split screen both players share one viewport,
 * and focus-based navigation would let one pad steer the other player's menu. The pawn moves the
 * selection and confirms; this widget only draws the rows it is handed.
 *
 * The tree is built in code, so the class is usable without a Widget Blueprint. A WBP child may still
 * restyle it -- it only has to leave the tree empty for this to build one.
 */
UCLASS(meta = (DisplayName = "L2H Time Quick Menu Widget"))
class LASTTWOHOURS_API UL2H_TimeQuickMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Redraws every row; the selected one is highlighted. */
	void SetOptions(const TArray<FText>& InOptions, int32 InSelected, const FText& InStatus);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Rows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	FLinearColor SelectedColor = FLinearColor(1.0f, 0.85f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	FLinearColor IdleColor = FLinearColor(0.8f, 0.8f, 0.8f, 0.85f);
};
