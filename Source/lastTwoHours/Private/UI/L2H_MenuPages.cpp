// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_MenuPages.h"

#include "GameFramework/GameUserSettings.h"

#define LOCTEXT_NAMESPACE "L2HMenu"

namespace L2HMenuPages
{
	namespace
	{
		FL2H_ClockOption Option(FName Id, const FText& Label, const FText& Value = FText::GetEmpty(), bool bDanger = false)
		{
			FL2H_ClockOption Result;
			Result.Id = Id;
			Result.Label = Label;
			Result.Value = Value;
			Result.bDanger = bDanger;
			return Result;
		}

		FText WindowModeText(EWindowMode::Type Mode)
		{
			switch (Mode)
			{
			case EWindowMode::Fullscreen: return LOCTEXT("Fullscreen", "Fullscreen");
			case EWindowMode::WindowedFullscreen: return LOCTEXT("Borderless", "Borderless");
			default: return LOCTEXT("Windowed", "Windowed");
			}
		}

		FText QualityText(int32 Level)
		{
			switch (Level)
			{
			case 0: return LOCTEXT("Low", "Low");
			case 1: return LOCTEXT("Medium", "Medium");
			case 2: return LOCTEXT("High", "High");
			case 3: return LOCTEXT("Epic", "Epic");
			default: return LOCTEXT("Custom", "Custom");
			}
		}

		FText FrameCapText(float Limit)
		{
			return Limit <= 0.0f ? LOCTEXT("Unlimited", "Unlimited") : FText::AsNumber(FMath::RoundToInt(Limit));
		}
	}

	FL2H_ClockPage SettingsPage()
	{
		FL2H_ClockPage Page;
		Page.Id = TEXT("Settings");
		Page.Title = LOCTEXT("SettingsTitle", "SETTINGS");
		const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
		if (Settings)
		{
			Page.Options.Add(Option(TEXT("Set_Window"), LOCTEXT("Window", "Display"), WindowModeText(Settings->GetFullscreenMode())));
			Page.Options.Add(Option(TEXT("Set_Quality"), LOCTEXT("Quality", "Quality"), QualityText(Settings->GetOverallScalabilityLevel())));
			Page.Options.Add(Option(TEXT("Set_VSync"), LOCTEXT("VSync", "VSync"), Settings->IsVSyncEnabled() ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off")));
			Page.Options.Add(Option(TEXT("Set_FrameCap"), LOCTEXT("FrameCap", "Frame cap"), FrameCapText(Settings->GetFrameRateLimit())));
		}
		Page.Options.Add(Option(TEXT("Back"), LOCTEXT("Back", "Back")));
		return Page;
	}

	bool ApplySetting(FName Id)
	{
		UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
		if (!Settings || !Id.ToString().StartsWith(TEXT("Set_")))
		{
			return false;
		}
		if (Id == TEXT("Set_Window"))
		{
			const EWindowMode::Type Mode = Settings->GetFullscreenMode();
			Settings->SetFullscreenMode(Mode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen
				: (Mode == EWindowMode::WindowedFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed));
		}
		else if (Id == TEXT("Set_Quality"))
		{
			const int32 Level = Settings->GetOverallScalabilityLevel();
			Settings->SetOverallScalabilityLevel(Level < 0 || Level >= 3 ? 0 : Level + 1);
		}
		else if (Id == TEXT("Set_VSync"))
		{
			Settings->SetVSyncEnabled(!Settings->IsVSyncEnabled());
		}
		else if (Id == TEXT("Set_FrameCap"))
		{
			static const float Caps[] = { 30.0f, 60.0f, 120.0f, 0.0f };
			const float Current = Settings->GetFrameRateLimit();
			int32 Next = 0;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Caps); ++Index)
			{
				if (FMath::IsNearlyEqual(Caps[Index], Current))
				{
					Next = (Index + 1) % UE_ARRAY_COUNT(Caps);
				}
			}
			Settings->SetFrameRateLimit(Caps[Next]);
		}
		Settings->ApplySettings(false);
		Settings->SaveSettings();
		return true;
	}

	FL2H_ClockPage ConfirmPage(FName PageId, const FText& Title, const FText& YesLabel, const FText& NoLabel)
	{
		FL2H_ClockPage Page;
		Page.Id = PageId;
		Page.Title = Title;
		// No first: an accidental press backs out.
		Page.Options.Add(Option(TEXT("No"), NoLabel));
		Page.Options.Add(Option(TEXT("Yes"), YesLabel, FText::GetEmpty(), /*bDanger=*/true));
		return Page;
	}

	FString CheckpointSlotFor(const FString& LevelName)
	{
		// UL2H_SaveSubsystem::ResolveSlotName: the default base, then the level's short name.
		return FString::Printf(TEXT("L2H_Time_Checkpoint_%s"), *LevelName);
	}
}

#undef LOCTEXT_NAMESPACE
