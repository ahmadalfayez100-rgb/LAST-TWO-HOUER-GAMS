// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "L2H_ClockMenuWidget.generated.h"

/** One choice on the dial. */
USTRUCT(BlueprintType)
struct FL2H_ClockOption
{
	GENERATED_BODY()

	/** What the caller is told when it is chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	FText Label;

	/** A second line under the label (a setting's current value, a price). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	FText Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	bool bEnabled = true;

	/** Drawn red: quitting, starting over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	bool bDanger = false;
};

/** One face of the clock: a title in the middle and its choices around the dial. */
USTRUCT(BlueprintType)
struct FL2H_ClockPage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L2H|Menu")
	TArray<FL2H_ClockOption> Options;
};

DECLARE_DELEGATE_OneParam(FL2HClockChosen, FName /*OptionId*/);
DECLARE_DELEGATE(FL2HClockBack);

/**
 * Every menu in the game is a clock face. Its choices sit in a ring of segments round the dial; the
 * MINUTE hand follows the mouse or a stick, and the segment it points at is the one chosen. Opening the
 * next page moves the HOUR hand on two hours (back two on Back), and the SECOND hand sweeps once round,
 * wiping the old face away behind it and leaving the new one -- a circular wipe that follows the hand.
 * The centre shows a digital clock: the time of day in the main menu, the player's time left in game.
 *
 * Drawn in code (NativePaint), so it needs no art. Input: mouse/stick aim, click / A / Enter confirm,
 * right click / B / Esc / Backspace back, arrows and D-pad step round the dial.
 */
UCLASS(meta = (DisplayName = "L2H Clock Menu"))
class LASTTWOHOURS_API UL2H_ClockMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UL2H_ClockMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows Page. bForward plays the forward transition (hour hand +2h), otherwise the way back. */
	void ShowPage(const FL2H_ClockPage& Page, bool bForward = true);

	/** Replaces the options of the page on screen without a transition (a setting changed its value). */
	void RefreshPage(const FL2H_ClockPage& Page);

	const FL2H_ClockPage& GetPage() const { return Current; }

	/** The centre shows this actor's time left instead of the time of day. */
	void SetTimeSource(AActor* InSource) { TimeSource = InSource; }

	/** A line under the clock for a few seconds: what the last choice did, or why it was refused. */
	void SetStatus(const FText& InStatus, float Seconds = 3.5f);

	/** Dims the world behind the dial (the main menu has nothing else to show). */
	void SetBackdropOpacity(float InOpacity) { BackdropOpacity = InOpacity; }

	/** Gives the dial keyboard / pad focus for its owning player. */
	void TakeFocus();

	/** Points the minute hand at Degrees (clockwise from 12) and selects what it lands on. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void AimAtDegrees(float Degrees) { AimAt(Degrees, true); }

	/** Steps the selection round the dial (+1 clockwise). */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void StepSelection(int32 Direction) { Step(Direction); }

	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void ConfirmSelection() { Confirm(); }

	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void GoBack() { Back(); }

	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	FName GetPageId() const { return Current.Id; }

	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	FName GetSelectedId() const { return Current.Options.IsValidIndex(Selected) ? Current.Options[Selected].Id : NAME_None; }

	/** 0..1 while the second hand wipes one face into the next. */
	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	float GetTransition() const { return Transition; }

	FL2HClockChosen OnChosen;
	FL2HClockBack OnBack;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	/** Seconds the second hand takes to sweep round in a transition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu", meta = (ClampMin = "0.1"))
	float TransitionSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	FLinearColor AccentColor = FLinearColor(0.35f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	FLinearColor DangerColor = FLinearColor(1.0f, 0.3f, 0.25f, 1.0f);

private:
	/** Clockwise degrees from 12 o'clock of option Index's centre on a page of Count. */
	static float OptionAngle(int32 Index, int32 Count) { return Count > 0 ? 360.0f * Index / Count : 0.0f; }
	int32 OptionAtAngle(float Degrees, int32 Count) const;
	void AimAt(float Degrees, bool bSnap);
	void Step(int32 Direction);
	void Confirm();
	void Back();

	void PaintPage(const FL2H_ClockPage& Page, int32 Selected, float FromDeg, float ToDeg, const FGeometry& Geo, FVector2D Centre,
		float Radius, FSlateWindowElementList& Out, int32 Layer) const;

	FL2H_ClockPage Current;
	FL2H_ClockPage Previous;
	int32 Selected = 0;
	int32 PreviousSelected = 0;

	/** Hands, in clockwise degrees from 12. The minute hand eases toward AimDeg. */
	float MinuteDeg = 0.0f;
	float AimDeg = 0.0f;
	float HourDeg = 0.0f;
	float HourFromDeg = 0.0f;
	float HourToDeg = 0.0f;

	/** 0..1 while the second hand sweeps; 1 when settled. */
	float Transition = 1.0f;
	bool bHasPage = false;
	float BackdropOpacity = 0.55f;
	FVector2D LastStick = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D(-1.0f);

	TWeakObjectPtr<AActor> TimeSource;
	FText Status;
	float StatusUntil = 0.0f;
};
