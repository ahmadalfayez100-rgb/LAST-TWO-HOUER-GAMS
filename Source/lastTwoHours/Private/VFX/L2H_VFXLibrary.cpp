// Copyright Epic Games, Inc. All Rights Reserved.

#include "VFX/L2H_VFXLibrary.h"

#include "Time/L2HTimeLog.h"
#include "UObject/Class.h"

const FL2H_VFXEntry* UL2H_VFXLibrary::Resolve(EL2H_VFXMoment Moment, EPhysicalSurface Surface) const
{
	if (Moment == EL2H_VFXMoment::None)
	{
		return nullptr;
	}

	FL2H_VFXKey Key;
	Key.Moment = Moment;

	Key.Surface = Surface;
	if (const FL2H_VFXEntry* Exact = Entries.Find(Key))
	{
		return Exact;
	}

	// Nothing authored for this surface, so the character's universal row plays instead. This is what
	// keeps an untextured floor from silently producing no effect at all.
	Key.Surface = SurfaceType_Default;
	if (const FL2H_VFXEntry* Universal = Entries.Find(Key))
	{
		return Universal;
	}

	// Rows that arrived from outside the editor are read after the map, so one a designer authored by
	// hand is never shadowed by one a tool wrote. Two passes, because a surface-specific row has to beat
	// the universal row here exactly as it does in the map.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FL2H_VFXKeyedEntry& Added : AddedEntries)
		{
			if (Added.Key.Moment != Moment)
			{
				continue;
			}

			const EPhysicalSurface AddedSurface = Added.Key.Surface.GetValue();
			if (Pass == 0 ? AddedSurface == Surface : AddedSurface == SurfaceType_Default)
			{
				return &Added.Entry;
			}
		}
	}

	return nullptr;
}

bool UL2H_VFXLibrary::TryResolve(EL2H_VFXMoment Moment, EPhysicalSurface Surface, FL2H_VFXEntry& OutEntry) const
{
	const FL2H_VFXEntry* Found = Resolve(Moment, Surface);
	if (!Found)
	{
		return false;
	}

	OutEntry = *Found;
	return true;
}

void UL2H_VFXLibrary::LogEntries() const
{
	L2HTime::LogImportant(FString::Printf(
		TEXT("VFXLibrary %s: %d map rows, %d added rows, identity primary %s"),
		*GetPathName(), Entries.Num(), AddedEntries.Num(), *IdentityLook.Primary.ToString()));

	auto LogRow = [this](const TCHAR* Source, const FL2H_VFXKey& Key, const FL2H_VFXEntry& Row)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("VFXLibrary %s: [%s] %s on surface %d -> %s (scale %.2f, %s, socket %s, life %.2fs)"),
			*GetPathName(),
			Source,
			*StaticEnum<EL2H_VFXMoment>()->GetNameStringByValue(static_cast<int64>(Key.Moment)),
			static_cast<int32>(Key.Surface.GetValue()),
			*Row.System.ToString(),
			Row.Scale,
			Row.bAttachToSocket ? TEXT("socket") : TEXT("world"),
			*Row.SocketName.ToString(),
			Row.LifetimeSeconds));
	};

	for (const TPair<FL2H_VFXKey, FL2H_VFXEntry>& Pair : Entries)
	{
		LogRow(TEXT("map"), Pair.Key, Pair.Value);
	}

	for (const FL2H_VFXKeyedEntry& Added : AddedEntries)
	{
		LogRow(TEXT("added"), Added.Key, Added.Entry);
	}
}
