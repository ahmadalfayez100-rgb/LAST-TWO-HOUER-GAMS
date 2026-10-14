// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grab/L2H_MassProfileComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UL2H_MassProfileComponent::UL2H_MassProfileComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Mass is design data read from the same asset on every machine, so there is nothing to send.
	SetIsReplicated(false);
}

float UL2H_MassProfileComponent::ResolveMass() const
{
	if (Mass > 0.0f)
	{
		return Mass;
	}

	// No authored mass, so fall back to the physics body. GetMass() answers in kg, and returns 0 for
	// a body with no physics -- which is the honest answer for static set dressing.
	const AActor* Owner = GetOwner();
	const UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	return Root ? Root->GetMass() : 0.0f;
}

UL2H_MassProfileComponent* UL2H_MassProfileComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UL2H_MassProfileComponent>() : nullptr;
}
