// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_TimeClockWidget.h"

#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Time/L2H_TimeAttributeSet.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "World/L2H_CheckpointTerminal.h"

#define LOCTEXT_NAMESPACE "L2HTimeClock"

namespace
{
	/**
	 * Seconds as H:MM:SS.
	 *
	 * Truncated rather than rounded, because a clock that rounds up shows a second that does not
	 * exist -- and the last second is the one being watched. Hours are unpadded, so a 300 second run
	 * reads 0:05:00 rather than 00:05:00.
	 */
	FText FormatClockText(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		const int32 Hours = Total / 3600;
		const int32 Minutes = (Total % 3600) / 60;
		const int32 RemainingSeconds = Total % 60;

		// Digits and separators only, so there is nothing here for a translator to reorder.
		return FText::FromString(FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, RemainingSeconds));
	}

	/**
	 * What a body on the floor needs, in words: which revive still works and how much is missing. The two
	 * ways down need different help (a hand for Health, real seconds for the clock), and the partner reads
	 * this line to know which one to bring.
	 */
	FText DescribeDown(const AL2H_BaseCharacter* Character, bool bDead)
	{
		const UL2H_LevelTimeConfig* Config = Character ? AL2H_TimeDirector::GetLevelConfig(Character) : nullptr;
		const float Missing = Config ? FMath::Max(Config->RemoteReviveThreshold - Character->PendingReviveSeconds, 0.0f) : 0.0f;

		if (bDead && Character && Character->IsFreeReviveOpen())
		{
			return FText::Format(LOCTEXT("DeadWindow", "DOWN (health) -- partner: hold Interact to revive, {0}s left"),
				FText::AsNumber(FMath::CeilToInt(Character->GetFreeReviveSecondsLeft())));
		}
		return FText::Format(bDead
				? LOCTEXT("DeadNeedsTime", "DEAD -- needs {0}s more by transfer, an elixir, a revive station or a hub")
				: LOCTEXT("OutOfTime", "OUT OF TIME -- needs {0}s more by transfer, an elixir, a revive station or a hub"),
			FText::AsNumber(FMath::CeilToInt(Missing)));
	}
}

void UL2H_TimeClockWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The layout asset may predate these lines; they are added to its root panel rather than requiring an
	// edit to the widget asset.
	if (!InfoText)
	{
		InfoText = MakeTextIfMissing(TEXT("InfoText"), 13);
	}
	if (!DeltaText)
	{
		DeltaText = MakeTextIfMissing(TEXT("DeltaText"), 20);
	}
	if (DeltaText)
	{
		DeltaText->SetText(FText::GetEmpty());
	}
}

UTextBlock* UL2H_TimeClockWidget::MakeTextIfMissing(const TCHAR* Name, int32 FontSize)
{
	UPanelWidget* Root = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (!Root)
	{
		return nullptr;
	}

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	// Placed against the clock block (the canvas's first child): the info lines just above it, the delta
	// flash just to its right, on the clock's own anchor so they follow it wherever the layout puts it.
	const UCanvasPanelSlot* ClockSlot = Root->GetChildrenCount() > 0 ? Cast<UCanvasPanelSlot>(Root->GetChildAt(0)->Slot) : nullptr;
	if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Text)))
	{
		TextSlot->SetAutoSize(true);
		if (ClockSlot)
		{
			const FVector2D Position = ClockSlot->GetPosition();
			const FVector2D Size = ClockSlot->GetSize();
			TextSlot->SetAnchors(ClockSlot->GetAnchors());
			TextSlot->SetAlignment(ClockSlot->GetAlignment());
			const bool bInfo = FCString::Strcmp(Name, TEXT("InfoText")) == 0;
			TextSlot->SetPosition(bInfo ? FVector2D(Position.X, Position.Y - Size.Y - 8.0f)
				: FVector2D(Position.X + Size.X + 12.0f, Position.Y - Size.Y * 0.5f));
		}
	}
	return Text;
}

void UL2H_TimeClockWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetRealTimeSeconds() : 0.0f;

	// The delta flash: any change bigger than the drain's one second, shown for a moment beside the clock,
	// so a hit, a reload or a gift reads as a price rather than as the clock jumping.
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	const UL2H_TimeAttributeSet* TimeSet = ASC ? ASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;
	if (TimeSet && DeltaText)
	{
		const float Time = TimeSet->GetTime();
		if (LastSeenTime >= 0.0f)
		{
			const float Delta = Time - LastSeenTime;
			if (FMath::Abs(Delta) > 1.5f)
			{
				DeltaText->SetText(FText::FromString(FString::Printf(TEXT("%+.0f s"), Delta)));
				DeltaText->SetColorAndOpacity(FSlateColor(Delta < 0.0f ? DownedColor : HealthColor));
				DeltaShownUntil = Now + 1.6f;
			}
		}
		LastSeenTime = Time;
		if (DeltaShownUntil > 0.0f && Now > DeltaShownUntil)
		{
			DeltaText->SetText(FText::GetEmpty());
			DeltaShownUntil = 0.0f;
		}
	}

	// A warned clock pulses between the low and the downed colour, so it is noticed without being read.
	if (bHubWarning && TimeText)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 2.0f * PI * 1.5f);
		TimeText->SetColorAndOpacity(FSlateColor(FMath::Lerp(LowColor, DownedColor, Pulse)));
	}

	InfoRefreshIn -= InDeltaTime;
	if (InfoRefreshIn <= 0.0f)
	{
		InfoRefreshIn = 0.2f;
		RefreshInfo();

		// The down line counts down, which no attribute change announces.
		const AL2H_BaseCharacter* Character = Source.Get();
		if (Character && (Character->IsDead() || Character->IsDowned()))
		{
			RefreshClock();
		}
	}
}

void UL2H_TimeClockWidget::RefreshInfo()
{
	const AL2H_BaseCharacter* Character = Source.Get();
	if (!InfoText || !Character)
	{
		return;
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Bank %d"), FMath::RoundToInt(Character->GetChrons())));
	if (Character->GetShieldHits() > 0)
	{
		Lines.Add(FString::Printf(TEXT("Shield %d"), Character->GetShieldHits()));
	}

	// The partner's clock, and their state, because the run only moves on as a pair.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AL2H_BaseCharacter> It(const_cast<UWorld*>(World)); It; ++It)
		{
			const AL2H_BaseCharacter* Other = *It;
			if (!Other || Other == Character)
			{
				continue;
			}
			const float OtherTime = AL2H_TimeDirector::GetTimeFor(Other);
			const TCHAR* State = Other->IsDead() ? TEXT(" DOWN (health)") : (Other->IsDowned() ? TEXT(" OUT OF TIME") : TEXT(""));
			Lines.Add(FString::Printf(TEXT("Partner %s%s"), *FormatClockText(OtherTime).ToString(), State));
			break;
		}
	}

	if (const UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>())
	{
		const float Reload = Weapons->GetReloadCost();
		if (Reload > 0.0f)
		{
			Lines.Add(FString::Printf(TEXT("Reload %.0f s"), Reload));
		}
	}

	// The walk to the next hub against the clock. The run ends hard when both fall, so the clock says
	// it will not make it while there is still time to choose: hurry, transfer, or spend a tool.
	const bool bWasWarning = bHubWarning;
	bHubWarning = false;
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(Character);
	const AL2H_CheckpointTerminal* Hub = Character->IsDead() || Character->IsDowned() ? nullptr : FindNextHub(Character);
	if (Hub && Config)
	{
		const float Distance = FVector::Dist(Character->GetActorLocation(), Hub->GetActorLocation());
		const float Needed = Distance * Config->HubPathFactor / FMath::Max(Config->HubTravelSpeed, 1.0f);
		const float Time = AL2H_TimeDirector::GetTimeFor(Character);
		bHubWarning = Time < Needed * Config->HubWarnRatio + Config->HubWarnMargin;
		WarningHubName = Hub->DisplayName.IsEmpty() ? LOCTEXT("HubFallbackName", "the hub") : Hub->DisplayName;
		Lines.Add(FString::Printf(TEXT("%s  %.0f m  ~%.0f s"), *(Hub->DisplayName.IsEmpty() ? FString(TEXT("Hub")) : Hub->DisplayName.ToString()),
			Distance / 100.0f, Needed));

		const UWorld* World = GetWorld();
		const float Now = World ? World->GetRealTimeSeconds() : 0.0f;
		if (bHubWarning && (!bWasWarning || Now >= NextWarningSoundAt))
		{
			if (Config->WarningSound)
			{
				UGameplayStatics::PlaySound2D(this, Config->WarningSound);
			}
			NextWarningSoundAt = Now + Config->WarningRepeat;
		}
	}
	if (bHubWarning != bWasWarning)
	{
		RefreshClock();
	}

	if (Character->IsHubBankOpen())
	{
		Lines.Add(FString::Printf(TEXT("Stage done -- carrying %.0f/%.0f s into the next (time menu)"),
			Character->GetHubCarried(), Character->GetHubCarryAllowance()));
	}

	InfoText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

const AL2H_CheckpointTerminal* UL2H_TimeClockWidget::FindNextHub(const AL2H_BaseCharacter* Character) const
{
	const UWorld* World = GetWorld();
	if (!World || !Character)
	{
		return nullptr;
	}

	const AL2H_CheckpointTerminal* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AL2H_CheckpointTerminal> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AL2H_CheckpointTerminal* Hub = *It;
		if (!Hub || Hub->Tier != EL2H_CheckpointTier::Hub)
		{
			continue;
		}
		if (Hub->IsPawnInRange(Character))
		{
			// Standing in one: the clock is stopped and there is nothing to warn about.
			return nullptr;
		}
		if (Hub->GetCheckpointId() == Character->GetLastHubId())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Hub->GetActorLocation(), Character->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Hub;
		}
	}
	return Best;
}

void UL2H_TimeClockWidget::SetTimeSource(AL2H_BaseCharacter* InCharacter)
{
	Unbind();

	Source = InCharacter;
	UAbilitySystemComponent* ASC = InCharacter ? InCharacter->GetAbilitySystemComponent() : nullptr;

	if (ASC)
	{
		BoundASC = ASC;
		TimeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetTimeAttribute())
			.AddUObject(this, &UL2H_TimeClockWidget::HandleAttributeChanged);
		MaxTimeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetMaxTimeAttribute())
			.AddUObject(this, &UL2H_TimeClockWidget::HandleAttributeChanged);
		HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetHealthAttribute())
			.AddUObject(this, &UL2H_TimeClockWidget::HandleAttributeChanged);
		MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &UL2H_TimeClockWidget::HandleAttributeChanged);
	}

	RefreshClock();
}

void UL2H_TimeClockWidget::RefreshClock()
{
	const AL2H_BaseCharacter* Character = Source.Get();
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	const UL2H_TimeAttributeSet* TimeSet = ASC ? ASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;

	const float Time = TimeSet ? TimeSet->GetTime() : 0.0f;
	const float MaxTime = TimeSet ? FMath::Max(TimeSet->GetMaxTime(), 1.0f) : 1.0f;

	// Time and bDowned replicate separately, so a client can see 0 a frame before the flag.
	const bool bDowned = (Character && Character->IsDowned()) || (TimeSet && Time <= 0.0f);

	// Death outranks Downed in the readout. A corpse is limp too, but the two say very different
	// things to the player deciding whether to walk over and pick someone up.
	const bool bDead = Character && Character->IsDead();

	const FLinearColor Color = bDead
		? DeadColor
		: (bDowned ? DownedColor : (Time <= LowTimeThreshold ? LowColor : NormalColor));

	if (TimeBar)
	{
		TimeBar->SetPercent(FMath::Clamp(Time / MaxTime, 0.0f, 1.0f));
		TimeBar->SetFillColorAndOpacity(Color);
	}

	if (TimeText)
	{
		TimeText->SetText(FormatClockText(Time));
		TimeText->SetColorAndOpacity(FSlateColor(Color));
	}

	// MaxHealth of 0 means the level never turned the health layer on. Collapsing is the honest
	// reading of that: GetHealthPercent() answers 1.0 there, so a visible bar would sit permanently
	// full and read as "unhurt" no matter what landed on the player.
	const bool bHasHealth = TimeSet && TimeSet->GetMaxHealth() > 0.0f;

	if (HealthBar)
	{
		HealthBar->SetVisibility(bHasHealth ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		if (bHasHealth)
		{
			const float HealthPercent = TimeSet->GetHealthPercent();
			HealthBar->SetPercent(HealthPercent);
			HealthBar->SetFillColorAndOpacity(HealthPercent <= LowHealthFraction ? LowHealthColor : HealthColor);
		}
	}

	if (StateText)
	{
		if (!TimeSet)
		{
			StateText->SetText(LOCTEXT("NoSource", "NO TIME SOURCE"));
			StateText->SetColorAndOpacity(FSlateColor(DownedColor));
		}
		else if (bDead || bDowned)
		{
			StateText->SetText(DescribeDown(Character, bDead));
			StateText->SetColorAndOpacity(FSlateColor(bDead ? DeadColor : DownedColor));
		}
		else if (bHubWarning)
		{
			StateText->SetText(FText::Format(LOCTEXT("HubWarning", "NOT ENOUGH TIME TO REACH {0}"), WarningHubName));
			StateText->SetColorAndOpacity(FSlateColor(DownedColor));
		}
		else
		{
			StateText->SetText(FText::GetEmpty());
		}
	}

	OnClockUpdated(Time, MaxTime, bDowned);
}

void UL2H_TimeClockWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshClock();
}

void UL2H_TimeClockWidget::Unbind()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetTimeAttribute()).Remove(TimeChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetMaxTimeAttribute()).Remove(MaxTimeChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}

	TimeChangedHandle.Reset();
	MaxTimeChangedHandle.Reset();
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	BoundASC.Reset();
	Source.Reset();
}

void UL2H_TimeClockWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
