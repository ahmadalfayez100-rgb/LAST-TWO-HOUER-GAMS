// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_TimeRequestPromptWidget.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "L2HTimeRequest"

void UL2H_TimeRequestPromptWidget::SetRequest(bool bInActive, const FText& InRequesterName,
	float InAmount, float InCost)
{
	bRequestActive = bInActive;

	if (PromptText)
	{
		PromptText->SetText(bInActive
			? FText::Format(LOCTEXT("RequestLine", "{0} asks for {1} s"),
				InRequesterName, FText::AsNumber(InAmount))
			: FText::GetEmpty());
	}

	if (HintText)
	{
		HintText->SetText(bInActive
			? FText::Format(LOCTEXT("RequestHint", "Accept  Z   Decline  X   (costs you {0} s)"),
				FText::AsNumber(InCost))
			: FText::GetEmpty());
	}

	// Collapsed rather than Hidden: an empty strip should not reserve layout space at the screen edge,
	// and it must never be hit-testable or it would eat the partner's input.
	SetVisibility(bInActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	OnPromptChanged(bInActive, InRequesterName, InAmount, InCost);
}

#undef LOCTEXT_NAMESPACE
