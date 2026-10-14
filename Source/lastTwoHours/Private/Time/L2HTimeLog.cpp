// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2HTimeLog.h"

DEFINE_LOG_CATEGORY(LogL2HTime);

static TAutoConsoleVariable<int32> CVarL2HTimeDebug(
	TEXT("L2H.Time.Debug"),
	0,
	TEXT("Enables verbose TimeCore logging. 0 = off, 1 = on."),
	ECVF_Default);

bool L2HTime::IsDebugEnabled()
{
	return CVarL2HTimeDebug.GetValueOnAnyThread() != 0;
}

void L2HTime::LogVerbose(const FString& Message)
{
	if (IsDebugEnabled())
	{
		UE_LOG(LogL2HTime, Log, TEXT("%s"), *Message);
	}
}

void L2HTime::LogImportant(const FString& Message)
{
	UE_LOG(LogL2HTime, Log, TEXT("%s"), *Message);
}
