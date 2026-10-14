// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_TimeQuickMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "L2HTimeQuickMenu"

void UL2H_TimeQuickMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		// A Blueprint child that authored its own tree keeps it.
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.78f));
	Panel->SetPadding(FMargin(18.0f, 14.0f));

	// Left edge, vertically centred: the D-pad Left that opens it points there.
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
		PanelSlot->SetPosition(FVector2D(40.0f, 0.0f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(LOCTEXT("Title", "TIME"));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 1.0f, 1.0f)));
	Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
	Column->AddChildToVerticalBox(Rows);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	Hint->SetText(LOCTEXT("Hint", "Up/Down choose   X or Triangle confirms   Circle closes      (keyboard: arrows, Enter or E, Esc)"));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
	FSlateFontInfo HintFont = Hint->GetFont();
	HintFont.Size = 11;
	Hint->SetFont(HintFont);
	Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
}

void UL2H_TimeQuickMenuWidget::SetOptions(const TArray<FText>& InOptions, int32 InSelected, const FText& InStatus)
{
	if (!Rows || !WidgetTree)
	{
		return;
	}

	Rows->ClearChildren();

	for (int32 Index = 0; Index < InOptions.Num(); ++Index)
	{
		const bool bSelected = Index == InSelected;

		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Row->SetText(bSelected
			? FText::Format(LOCTEXT("SelectedRow", "> {0}"), InOptions[Index])
			: FText::Format(LOCTEXT("IdleRow", "   {0}"), InOptions[Index]));
		Row->SetColorAndOpacity(FSlateColor(bSelected ? SelectedColor : IdleColor));
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
	}

	if (StatusText)
	{
		StatusText->SetText(InStatus);
	}
}

#undef LOCTEXT_NAMESPACE
