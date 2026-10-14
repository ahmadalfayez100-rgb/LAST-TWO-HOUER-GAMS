// Copyright Epic Games, Inc. All Rights Reserved.

#include "VFX/L2H_VFXLibrary.h"

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
	return Entries.Find(Key);
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
