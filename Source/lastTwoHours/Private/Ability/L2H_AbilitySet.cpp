// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_AbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Time/L2HTimeLog.h"

bool UL2H_AbilitySet::GrantAbilityClass(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, UObject* SourceObject)
{
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	// Abilities are granted once, on the authority. A client that grants its own copy ends up with a
	// spec the server never hears about, and its input fires into nothing.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	// Already present: GiveAbility would add a second spec for the same class, and the input
	// binding activates by class -- so both copies would fire on one press.
	if (ASC->FindAbilitySpecFromClass(AbilityClass))
	{
		return false;
	}

	// A caller that leaves Level defaulted must still land on 1; GiveAbility treats 0 as unlevelled.
	const int32 GrantLevel = FMath::Max(1, Level);

	FGameplayAbilitySpec Spec(AbilityClass, GrantLevel, INDEX_NONE, SourceObject);
	ASC->GiveAbility(Spec);

	L2HTime::LogVerbose(FString::Printf(TEXT("Granted %s at level %d"),
		*GetNameSafe(AbilityClass), GrantLevel));

	return true;
}

int32 UL2H_AbilitySet::GrantTo(UAbilitySystemComponent* ASC, UObject* SourceObject) const
{
	int32 GrantedCount = 0;

	for (const FL2H_AbilityEntry& Entry : Abilities)
	{
		if (GrantAbilityClass(ASC, Entry.Ability, Entry.Level, SourceObject))
		{
			++GrantedCount;
		}
	}

	return GrantedCount;
}
