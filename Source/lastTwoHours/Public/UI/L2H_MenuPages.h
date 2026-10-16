// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/L2H_ClockMenuWidget.h"

/**
 * The faces the clock menus share. The settings face reads and writes the engine's user settings
 * (window mode, quality, vsync, frame cap) and keeps them on disk.
 */
namespace L2HMenuPages
{
	/** The settings face, showing each setting's current value. */
	LASTTWOHOURS_API FL2H_ClockPage SettingsPage();

	/** Applies a settings choice (cycles it to its next value). True when Id was a setting. */
	LASTTWOHOURS_API bool ApplySetting(FName Id);

	/** A yes/no face: Yes is drawn red. */
	LASTTWOHOURS_API FL2H_ClockPage ConfirmPage(FName PageId, const FText& Title, const FText& YesLabel, const FText& NoLabel);

	/** The save slot a level's checkpoint lives in (as the save subsystem names it). */
	LASTTWOHOURS_API FString CheckpointSlotFor(const FString& LevelName);
}
