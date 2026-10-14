// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"

#include "L2H_TimeClockWidget.generated.h"

class AL2H_BaseCharacter;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;

/**
 * The per-player time clock. One instance per local player, added with AddToPlayerScreen so
 * every split-screen view gets its own.
 *
 * Read-only by design: it listens to the replicated Time / MaxTime attributes and never writes
 * them. Updates are event-driven (attribute-change delegates), not per-frame bindings.
 *
 * The Widget Blueprint child must contain a ProgressBar named "TimeBar" and a TextBlock named
 * "TimeText". A ProgressBar named "HealthBar" and a TextBlock named "StateText" are optional, so a
 * level that never turns the health layer on still gets a working clock.
 */
UCLASS(Abstract, meta = (DisplayName = "L2H Time Clock Widget"))
class LASTTWOHOURS_API UL2H_TimeClockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Points the clock at a pawn. Safe to call again; the previous binding is released first. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void SetTimeSource(AL2H_BaseCharacter* InCharacter);

	/** Re-reads the attributes and the downed flag and repaints. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Time")
	void RefreshClock();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Bank, partner clock and reload price, refreshed a few times a second. Created in code when the layout has none. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InfoText;

	/** "-12 s" / "+15 s" next to the clock for a moment after any change bigger than the drain. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeltaText;

	/** Fired after every repaint, for Blueprint-side polish (animations, sounds). */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Time")
	void OnClockUpdated(float Time, float MaxTime, bool bDowned);

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", meta = (BindWidget))
	TObjectPtr<UProgressBar> TimeBar;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Time", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	/**
	 * The health fill: Health / MaxHealth. Optional, because MaxHealth of 0 means the level never
	 * turned the health layer on and the bar then collapses rather than showing a pool of hit points
	 * that no hit could ever move.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Health", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	/** At or below this many seconds the clock switches to LowColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "0.0"))
	float LowTimeThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time")
	FLinearColor NormalColor = FLinearColor(0.25f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time")
	FLinearColor LowColor = FLinearColor(1.0f, 0.55f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time")
	FLinearColor DownedColor = FLinearColor(0.9f, 0.15f, 0.15f, 1.0f);

	/** Health fill at or above LowHealthFraction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health")
	FLinearColor HealthColor = FLinearColor(0.35f, 0.85f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health")
	FLinearColor LowHealthColor = FLinearColor(1.0f, 0.35f, 0.15f, 1.0f);

	/** Fraction of MaxHealth at or below which the fill switches to LowHealthColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthFraction = 0.3f;

	/** State line colour for a dead pawn. Darker than DownedColor: over, not merely waiting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health")
	FLinearColor DeadColor = FLinearColor(0.6f, 0.0f, 0.0f, 1.0f);

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshInfo();
	UTextBlock* MakeTextIfMissing(const TCHAR* Name, int32 FontSize);

	/**
	 * The hub this player is walking to: the nearest hub other than the one whose stage they last finished.
	 * Null when the level has none, or while the player stands in one.
	 */
	const class AL2H_CheckpointTerminal* FindNextHub(const AL2H_BaseCharacter* Character) const;

	/** True while the clock no longer covers the estimated walk to the next hub. */
	bool bHubWarning = false;
	float NextWarningSoundAt = 0.0f;
	FText WarningHubName;
	float LastSeenTime = -1.0f;
	float DeltaShownUntil = 0.0f;
	float InfoRefreshIn = 0.0f;
	void Unbind();

	TWeakObjectPtr<AL2H_BaseCharacter> Source;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle TimeChangedHandle;
	FDelegateHandle MaxTimeChangedHandle;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};
