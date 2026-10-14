// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/Crc.h"

/**
 * The identity a PLACED actor is remembered under.
 *
 * The save layer keys world progress on a GUID, not on an actor pointer, because a pickup or an
 * unlocked door has to still be remembered after a level reload and a pointer does not survive one. A
 * placed actor has no natural GUID, so this derives a stable one from its own full path.
 *
 * The path rather than the name, and a hash rather than a random id, are both deliberate: two props may
 * share a name in different folders, and NewGuid() would hand out a different id on every load, which
 * would mean no door was ever remembered as unlocked. Four salted CRCs of the same string fill the four
 * fields of the GUID.
 *
 * Deliberately a plain header with no reflected types, so it stays a utility rather than another
 * UCLASS that has to be referenced from Blueprint to be useful.
 */
inline FGuid L2H_MakeWorldStateId(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return FGuid();
	}

	const FString Path = Actor->GetPathName();

	const uint32 A = FCrc::StrCrc32(*Path);
	const uint32 B = FCrc::StrCrc32(*Path, A);
	const uint32 C = FCrc::StrCrc32(*Path, B);
	const uint32 D = FCrc::StrCrc32(*Path, C);

	return FGuid(A, B, C, D);
}
