// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "L2H_AbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/** One grantable ability, at the level it arrives at. */
USTRUCT(BlueprintType)
struct FL2H_AbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Abilities")
	TSubclassOf<UGameplayAbility> Ability;

	/** Passed through to GiveAbility. 1 unless a character is meant to start stronger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Abilities", meta = (ClampMin = "1"))
	int32 Level = 1;
};

/**
 * A named bundle of abilities.
 *
 * The base abilities every player has (hook, carry) and the abilities one character has (Time Bend,
 * Chrono Ball) are the same kind of thing, so they share one asset type. That is what lets the fixed
 * base set be granted to everyone from the level config while a character set is granted per pawn,
 * without either side knowing about the other.
 *
 * Granting is idempotent: re-granting a set already on the component adds nothing, so a possess
 * happening twice cannot give a player two copies of the same ability firing on one button press.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Ability Set"))
class LASTTWOHOURS_API UL2H_AbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Abilities")
	TArray<FL2H_AbilityEntry> Abilities;

	/**
	 * Server-only. Returns how many abilities were newly granted, so a caller can tell "granted
	 * nothing" from "was already there".
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Abilities")
	int32 GrantTo(UAbilitySystemComponent* ASC, UObject* SourceObject) const;

	/**
	 * Server-only. Grants one class and returns whether it was newly added.
	 *
	 * GrantTo routes through here, so there is exactly one place that owns the authority gate and the
	 * already-present check. A caller that holds only a class -- the pawn's fixed base kit -- is
	 * therefore granted under the same rules as an entry in a set and cannot drift from it.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Abilities")
	static bool GrantAbilityClass(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, UObject* SourceObject);
};
