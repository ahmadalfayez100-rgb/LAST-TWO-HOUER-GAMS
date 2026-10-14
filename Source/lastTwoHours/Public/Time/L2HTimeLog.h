// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/** TimeCore log category. Every time mutation and every refusal is reported here. */
LASTTWOHOURS_API DECLARE_LOG_CATEGORY_EXTERN(LogL2HTime, Log, All);

namespace L2HTime
{
	/** True when the L2H.Time.Debug CVar is non-zero. */
	LASTTWOHOURS_API bool IsDebugEnabled();

	/** Logs to LogL2HTime only when L2H.Time.Debug is enabled. */
	LASTTWOHOURS_API void LogVerbose(const FString& Message);

	/** Always logs. Use for refusals and state transitions, which must be visible by default. */
	LASTTWOHOURS_API void LogImportant(const FString& Message);
}
