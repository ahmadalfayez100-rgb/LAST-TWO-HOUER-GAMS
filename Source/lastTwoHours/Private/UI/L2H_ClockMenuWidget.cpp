// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_ClockMenuWidget.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/DateTime.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Time/L2H_TimeDirector.h"

namespace L2HClock
{
	/** A point on the dial: clockwise degrees from 12 o'clock, at Radius from Centre (local space). */
	FVector2f Polar(FVector2D Centre, float Degrees, float Radius)
	{
		const float Rad = FMath::DegreesToRadians(Degrees);
		return FVector2f(Centre.X + FMath::Sin(Rad) * Radius, Centre.Y - FMath::Cos(Rad) * Radius);
	}

	/** Degrees wrapped into [0, 360). */
	float Wrap(float Degrees)
	{
		return FMath::Fmod(FMath::Fmod(Degrees, 360.0f) + 360.0f, 360.0f);
	}

	/** Shortest signed turn from A to B. */
	float Delta(float A, float B)
	{
		float D = Wrap(B - A);
		return D > 180.0f ? D - 360.0f : D;
	}

	void Sector(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, float FromDeg, float ToDeg,
		float InnerRadius, float OuterRadius, const FLinearColor& Colour)
	{
		if (ToDeg <= FromDeg)
		{
			return;
		}
		static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
		const FSlateRenderTransform& Transform = Geo.GetAccumulatedRenderTransform();
		// Vertex colours go to the GPU as they are: the values below are written as the colour wanted on screen.
		const FColor Packed = Colour.ToFColor(false);

		const int32 Steps = FMath::Max(2, FMath::CeilToInt((ToDeg - FromDeg) / 3.0f));
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		Verts.Reserve((Steps + 1) * 2);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float Deg = FMath::Lerp(FromDeg, ToDeg, static_cast<float>(Step) / Steps);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Polar(Centre, Deg, InnerRadius), FVector2f(0.5f, 0.5f), Packed));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Polar(Centre, Deg, OuterRadius), FVector2f(0.5f, 0.5f), Packed));
		}
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const SlateIndex I = static_cast<SlateIndex>(Step * 2);
			Indices.Append({ I, static_cast<SlateIndex>(I + 1), static_cast<SlateIndex>(I + 2), static_cast<SlateIndex>(I + 1), static_cast<SlateIndex>(I + 3), static_cast<SlateIndex>(I + 2) });
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Indices, nullptr, 0, 0);
	}

	void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, float FromDeg, float ToDeg, float Radius,
		const FLinearColor& Colour, float Thickness)
	{
		if (ToDeg <= FromDeg)
		{
			return;
		}
		TArray<FVector2f> Points;
		const int32 Steps = FMath::Max(2, FMath::CeilToInt((ToDeg - FromDeg) / 3.0f));
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			Points.Add(Polar(Centre, FMath::Lerp(FromDeg, ToDeg, static_cast<float>(Step) / Steps), Radius));
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
	}

	void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2f A, FVector2f B, const FLinearColor& Colour, float Thickness)
	{
		TArray<FVector2f> Points{ A, B };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FText& InText, FVector2f CentreAt,
		const FSlateFontInfo& Font, const FLinearColor& Colour)
	{
		if (InText.IsEmpty())
		{
			return;
		}
		const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(InText, Font);
		const FVector2f TopLeft(CentreAt.X - Size.X * 0.5f, CentreAt.Y - Size.Y * 0.5f);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(TopLeft)), InText, Font,
			ESlateDrawEffect::None, Colour);
	}
}

UL2H_ClockMenuWidget::UL2H_ClockMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UL2H_ClockMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The hour hand starts at the hour it really is.
	const FDateTime Now = FDateTime::Now();
	HourDeg = HourFromDeg = HourToDeg = (Now.GetHour() % 12 + Now.GetMinute() / 60.0f) * 30.0f;
}

void UL2H_ClockMenuWidget::TakeFocus()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		SetUserFocus(PC);
	}
	SetKeyboardFocus();
}

void UL2H_ClockMenuWidget::ShowPage(const FL2H_ClockPage& Page, bool bForward)
{
	Previous = Current;
	PreviousSelected = Selected;
	Current = Page;
	Selected = 0;
	for (int32 Index = 0; Index < Current.Options.Num(); ++Index)
	{
		if (Current.Options[Index].bEnabled)
		{
			Selected = Index;
			break;
		}
	}
	AimDeg = OptionAngle(Selected, Current.Options.Num());

	if (!bHasPage)
	{
		// The first face appears as it is; every later one is wiped in by the second hand.
		bHasPage = true;
		Transition = 1.0f;
		MinuteDeg = AimDeg;
		return;
	}
	Transition = 0.0f;
	HourFromDeg = HourDeg;
	HourToDeg = HourDeg + (bForward ? 60.0f : -60.0f);
}

void UL2H_ClockMenuWidget::SetStatus(const FText& InStatus, float Seconds)
{
	Status = InStatus;
	StatusUntil = GetWorld() ? GetWorld()->GetRealTimeSeconds() + Seconds : 0.0f;
}

void UL2H_ClockMenuWidget::RefreshPage(const FL2H_ClockPage& Page)
{
	Current = Page;
	Selected = FMath::Clamp(Selected, 0, FMath::Max(Current.Options.Num() - 1, 0));
}

void UL2H_ClockMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Transition < 1.0f)
	{
		Transition = FMath::Min(1.0f, Transition + InDeltaTime / FMath::Max(TransitionSeconds, 0.05f));
		HourDeg = FMath::InterpEaseInOut(HourFromDeg, HourToDeg, Transition, 2.0f);
	}
	// The minute hand follows the aim with a little weight, the short way round.
	MinuteDeg += L2HClock::Delta(MinuteDeg, AimDeg) * FMath::Min(1.0f, InDeltaTime * 14.0f);
}

int32 UL2H_ClockMenuWidget::OptionAtAngle(float Degrees, int32 Count) const
{
	if (Count <= 0)
	{
		return 0;
	}
	const float Span = 360.0f / Count;
	return FMath::FloorToInt(L2HClock::Wrap(Degrees + Span * 0.5f) / Span) % Count;
}

void UL2H_ClockMenuWidget::AimAt(float Degrees, bool bSnap)
{
	const int32 Count = Current.Options.Num();
	Selected = OptionAtAngle(Degrees, Count);
	AimDeg = bSnap ? OptionAngle(Selected, Count) : L2HClock::Wrap(Degrees);
}

void UL2H_ClockMenuWidget::Step(int32 Direction)
{
	const int32 Count = Current.Options.Num();
	if (Count == 0)
	{
		return;
	}
	// Past the ones that cannot be chosen.
	for (int32 Tries = 0; Tries < Count; ++Tries)
	{
		Selected = (Selected + Direction + Count) % Count;
		if (Current.Options[Selected].bEnabled)
		{
			break;
		}
	}
	AimDeg = OptionAngle(Selected, Count);
}

void UL2H_ClockMenuWidget::Confirm()
{
	if (Transition < 1.0f || !Current.Options.IsValidIndex(Selected) || !Current.Options[Selected].bEnabled)
	{
		return;
	}
	OnChosen.ExecuteIfBound(Current.Options[Selected].Id);
}

void UL2H_ClockMenuWidget::Back()
{
	if (Transition < 1.0f)
	{
		return;
	}
	OnBack.ExecuteIfBound();
}

FReply UL2H_ClockMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	if (LastMouse.X >= 0.0f && FVector2D::DistSquared(Local, LastMouse) < 4.0f)
	{
		return FReply::Handled();
	}
	LastMouse = Local;
	const FVector2D Size = InGeometry.GetLocalSize();
	const FVector2D Offset = Local - Size * 0.5;
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.40f;
	if (Offset.Size() > Radius * 0.2f)
	{
		AimAt(FMath::RadiansToDegrees(FMath::Atan2(Offset.X, -Offset.Y)), false);
	}
	return FReply::Handled();
}

FReply UL2H_ClockMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Confirm();
	}
	else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Back();
	}
	return FReply::Handled();
}

FReply UL2H_ClockMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Virtual_Accept)
	{
		Confirm();
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Virtual_Back
		|| Key == EKeys::Gamepad_Special_Right || Key == EKeys::P)
	{
		Back();
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::D || Key == EKeys::S)
	{
		Step(+1);
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::A || Key == EKeys::W)
	{
		Step(-1);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UL2H_ClockMenuWidget::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	const float Value = InAnalogEvent.GetAnalogValue();
	if (Key == EKeys::Gamepad_LeftX || Key == EKeys::Gamepad_RightX)
	{
		LastStick.X = Value;
	}
	else if (Key == EKeys::Gamepad_LeftY || Key == EKeys::Gamepad_RightY)
	{
		LastStick.Y = Value;
	}
	else
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
	}
	// A stick pushed well over points the minute hand; a resting one leaves it where it was.
	if (LastStick.Size() > 0.55f)
	{
		AimAt(FMath::RadiansToDegrees(FMath::Atan2(LastStick.X, LastStick.Y)), false);
	}
	return FReply::Handled();
}

void UL2H_ClockMenuWidget::PaintPage(const FL2H_ClockPage& Page, int32 PageSelected, float FromDeg, float ToDeg, const FGeometry& Geo,
	FVector2D Centre, float Radius, FSlateWindowElementList& Out, int32 Layer) const
{
	const int32 Count = Page.Options.Num();
	if (Count == 0 || ToDeg <= FromDeg)
	{
		return;
	}
	const float Span = 360.0f / Count;
	const float Gap = FMath::Min(1.6f, Span * 0.1f);
	const float Inner = Radius * 0.55f;
	const float Outer = Radius * 0.97f;
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp(FMath::RoundToInt(Radius * 0.062f), 12, 26));
	const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::Clamp(FMath::RoundToInt(Radius * 0.044f), 10, 18));

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FL2H_ClockOption& Option = Page.Options[Index];
		const bool bSelected = Index == PageSelected;
		const FLinearColor Tint = Option.bDanger ? DangerColor : AccentColor;
		const float Centr = OptionAngle(Index, Count);

		FLinearColor Fill(0.07f, 0.11f, 0.16f, 0.88f);
		if (Option.bDanger)
		{
			Fill = FLinearColor(0.24f, 0.06f, 0.06f, 0.88f);
		}
		if (bSelected)
		{
			Fill = FLinearColor(Tint.R * 0.42f, Tint.G * 0.42f, Tint.B * 0.42f, 0.95f);
		}

		// The segment, clipped to the part of the dial this face owns during a wipe. Segment 0 straddles
		// 12 o'clock, so it is tried a turn either way too.
		for (const float Shift : { -360.0f, 0.0f, 360.0f })
		{
			const float A = FMath::Max(Centr - Span * 0.5f + Gap + Shift, FromDeg);
			const float B = FMath::Min(Centr + Span * 0.5f - Gap + Shift, ToDeg);
			if (B > A)
			{
				L2HClock::Sector(Out, Layer, Geo, Centre, A, B, Inner, Outer, Fill);
				const FLinearColor Edge = bSelected ? Tint : FLinearColor(Tint.R, Tint.G, Tint.B, 0.35f);
				L2HClock::Arc(Out, Layer + 1, Geo, Centre, A, B, Outer, Edge, bSelected ? 3.0f : 1.2f);
				L2HClock::Arc(Out, Layer + 1, Geo, Centre, A, B, Inner, FLinearColor(Edge.R, Edge.G, Edge.B, Edge.A * 0.6f), 1.0f);
			}
		}

		// The label only when its middle is on this face.
		const float Mid = L2HClock::Wrap(Centr);
		if (Mid >= FromDeg && Mid < ToDeg)
		{
			const FVector2f At = L2HClock::Polar(Centre, Centr, (Inner + Outer) * 0.5f);
			const FLinearColor TextColour = !Option.bEnabled ? FLinearColor(0.4f, 0.4f, 0.4f, 0.8f)
				: (bSelected ? FLinearColor::White : FLinearColor(0.82f, 0.9f, 1.0f, 0.92f));
			const bool bHasValue = !Option.Value.IsEmpty();
			L2HClock::Text(Out, Layer + 2, Geo, Option.Label, FVector2f(At.X, At.Y - (bHasValue ? Radius * 0.03f : 0.0f)), LabelFont, TextColour);
			if (bHasValue)
			{
				L2HClock::Text(Out, Layer + 2, Geo, Option.Value, FVector2f(At.X, At.Y + Radius * 0.045f), ValueFont,
					FLinearColor(Tint.R, Tint.G, Tint.B, 0.95f));
			}
		}
	}
}

int32 UL2H_ClockMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Centre = Size * 0.5;
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.40f;
	if (Radius <= 1.0f)
	{
		return Layer;
	}

	// The world behind, dimmed.
	if (BackdropOpacity > 0.0f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, ++Layer, AllottedGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.01f, BackdropOpacity));
	}

	// The case: a dark disc, the rim, and sixty ticks.
	++Layer;
	L2HClock::Sector(OutDrawElements, Layer, AllottedGeometry, Centre, 0.0f, 360.0f, 0.0f, Radius * 1.12f, FLinearColor(0.03f, 0.05f, 0.08f, 0.9f));
	L2HClock::Arc(OutDrawElements, Layer + 1, AllottedGeometry, Centre, 0.0f, 360.0f, Radius * 1.12f, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.55f), 2.0f);
	L2HClock::Arc(OutDrawElements, Layer + 1, AllottedGeometry, Centre, 0.0f, 360.0f, Radius * 1.01f, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.25f), 1.0f);
	for (int32 Tick = 0; Tick < 60; ++Tick)
	{
		const bool bHour = Tick % 5 == 0;
		const float Deg = Tick * 6.0f;
		L2HClock::Line(OutDrawElements, Layer + 1, AllottedGeometry, L2HClock::Polar(Centre, Deg, Radius * (bHour ? 1.03f : 1.06f)),
			L2HClock::Polar(Centre, Deg, Radius * 1.10f), FLinearColor(0.75f, 0.88f, 1.0f, bHour ? 0.8f : 0.35f), bHour ? 2.0f : 1.0f);
	}
	Layer += 2;

	// The faces: during a wipe the new one from 12 o'clock up to the second hand, the old one beyond it.
	if (Transition < 1.0f)
	{
		const float Sweep = Transition * 360.0f;
		PaintPage(Previous, PreviousSelected, Sweep, 360.0f, AllottedGeometry, Centre, Radius, OutDrawElements, Layer);
		PaintPage(Current, Selected, 0.0f, Sweep, AllottedGeometry, Centre, Radius, OutDrawElements, Layer);
	}
	else
	{
		PaintPage(Current, Selected, 0.0f, 360.0f, AllottedGeometry, Centre, Radius, OutDrawElements, Layer);
	}
	Layer += 3;

	// The hands: hour (short and broad), minute (to the dial, pointing at the choice), second (thin, red).
	const FDateTime Now = FDateTime::Now();
	const float SecondDeg = Transition < 1.0f ? Transition * 360.0f : Now.GetSecond() * 6.0f;
	L2HClock::Line(OutDrawElements, Layer, AllottedGeometry, L2HClock::Polar(Centre, HourDeg + 180.0f, Radius * 0.06f),
		L2HClock::Polar(Centre, HourDeg, Radius * 0.34f), FLinearColor(0.9f, 0.95f, 1.0f, 0.95f), 6.0f);
	// The minute hand stops at the ring with a tip, so it points at the choice without covering its name.
	L2HClock::Line(OutDrawElements, Layer + 1, AllottedGeometry, L2HClock::Polar(Centre, MinuteDeg + 180.0f, Radius * 0.08f),
		L2HClock::Polar(Centre, MinuteDeg, Radius * 0.5f), AccentColor, 3.0f);
	L2HClock::Sector(OutDrawElements, Layer + 1, AllottedGeometry, Centre, MinuteDeg - 3.0f, MinuteDeg + 3.0f, Radius * 0.5f, Radius * 0.545f, AccentColor);
	L2HClock::Line(OutDrawElements, Layer + 2, AllottedGeometry, L2HClock::Polar(Centre, SecondDeg + 180.0f, Radius * 0.12f),
		L2HClock::Polar(Centre, SecondDeg, Radius * 1.08f), FLinearColor(1.0f, 0.25f, 0.2f, 1.0f), Transition < 1.0f ? 2.5f : 1.5f);
	L2HClock::Sector(OutDrawElements, Layer + 3, AllottedGeometry, Centre, 0.0f, 360.0f, 0.0f, Radius * 0.025f, FLinearColor(0.9f, 0.95f, 1.0f, 1.0f));
	Layer += 4;

	// The middle: the page's title and the digital clock, over the hands so they stay readable.
	L2HClock::Arc(OutDrawElements, Layer, AllottedGeometry, Centre, 0.0f, 360.0f, Radius * 0.5f, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.45f), 1.5f);
	const FL2H_ClockPage& Shown = Transition < 0.5f ? Previous : Current;
	L2HClock::Text(OutDrawElements, Layer + 1, AllottedGeometry, Shown.Title, FVector2f(Centre.X, Centre.Y - Radius * 0.24f),
		FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp(FMath::RoundToInt(Radius * 0.055f), 10, 22)), FLinearColor(0.75f, 0.88f, 1.0f, 0.95f));

	FText Digital;
	FLinearColor DigitalColour(0.92f, 0.97f, 1.0f, 1.0f);
	if (const AActor* Source = TimeSource.Get())
	{
		const float Seconds = FMath::Max(0.0f, AL2H_TimeDirector::GetTimeFor(Source));
		const int32 Whole = FMath::FloorToInt(Seconds);
		Digital = FText::FromString(FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60));
		if (Seconds < 30.0f)
		{
			DigitalColour = DangerColor;
		}
	}
	else
	{
		Digital = FText::FromString(FString::Printf(TEXT("%02d:%02d"), Now.GetHour(), Now.GetMinute()));
	}
	L2HClock::Text(OutDrawElements, Layer + 1, AllottedGeometry, Digital, FVector2f(Centre.X, Centre.Y + Radius * 0.24f),
		FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp(FMath::RoundToInt(Radius * 0.1f), 14, 40)), DigitalColour);
	

	// The status line, under the dial while it lasts.
	if (!Status.IsEmpty() && GetWorld() && GetWorld()->GetRealTimeSeconds() < StatusUntil)
	{
		L2HClock::Text(OutDrawElements, Layer + 1, AllottedGeometry, Status, FVector2f(Centre.X, Centre.Y + Radius * 1.2f),
			FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp(FMath::RoundToInt(Radius * 0.055f), 11, 20)), FLinearColor(1.0f, 0.9f, 0.6f, 1.0f));
	}

	return Layer + 2;
}
