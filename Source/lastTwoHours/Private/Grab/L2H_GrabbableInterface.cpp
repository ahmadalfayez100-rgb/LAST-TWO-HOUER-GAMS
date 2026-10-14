// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grab/L2H_GrabbableInterface.h"

// Nothing to define here on purpose.
//
// For a UINTERFACE, UnrealHeaderTool generates the default _Implementation body for every
// BlueprintNativeEvent, so writing one here is a C2084 "already has a body". That generated default
// is exactly what both functions want anyway: they are void, so the default is a no-op, and Execute_
// stays safe on an actor that implements the interface but overrides nothing.
//
// The veto that used to live here as a bool is now UL2H_MassProfileComponent::bCanBeGrabbed. A
// generated bool default would have been false, which would have made every interface actor refuse
// the grab silently -- and a lock is level data, not a code path.
