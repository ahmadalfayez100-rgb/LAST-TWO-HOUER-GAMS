// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_StruggleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UL2H_StruggleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.08f, 0.0f, 0.0f, 0.72f));
	Panel->SetPadding(FMargin(22.0f, 12.0f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.78f, 0.5f, 0.78f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.75f, 1.0f)));
	Label->SetJustification(ETextJustify::Center);
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 22;
	Label->SetFont(Font);
	Column->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(360.0f);
	BarBox->SetHeightOverride(16.0f);
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	Bar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.55f, 0.1f, 1.0f));
	BarBox->SetContent(Bar);
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarBox))
	{
		BarSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		BarSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UL2H_StruggleWidget::SetState(const FText& InText, float Progress)
{
	if (Label)
	{
		Label->SetText(InText);
	}
	if (Bar)
	{
		Bar->SetVisibility(Progress >= 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Bar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	}
}
