// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_Interact.generated.h"

/**
 * Interact: the one contextual button.
 *
 * A pad has fewer buttons than this game has verbs, so one press has to mean carry, put down, or hook
 * depending on what the player is looking at rather than on which button they found. This ability is
 * that decision and nothing else. It owns no rules of its own and forwards to whichever ability's rules
 * already fit the target, so the mass bands, the anchor refusal and the interface veto stay in exactly
 * one place each.
 *
 * The order matters:
 *   1. Already holding something -- the press is the drop half of the carry toggle. A trace here would
 *      only re-find the object already attached to the hand and ask the mass bands a question that has
 *      already been answered.
 *   2. Anything else -- one probe at the widest reach, then the light/heavy split.
 *
 * Anchors, locked props and the two-player band are deliberately not decided here: the forwarded
 * ability refuses them and says why in the log.
 *
 * The checkpoint terminal is also not here. Opening it is a UI act on the owning machine, so
 * AL2H_BaseCharacter::Input_Interact short-circuits to the menu before this ability is ever reached.
 *
 * Free by design, like both abilities it forwards to.
 */
UCLASS(meta = (DisplayName = "L2H Ability Interact"))
class LASTTWOHOURS_API UL2H_Ability_Interact : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_Interact();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/**
	 * How far the context probe looks. This has to reach at least as far as the widest verb it forwards
	 * to, otherwise a heavy object the hook could have reached reads as empty air.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact", meta = (ClampMin = "50.0"))
	float InteractRange = 600.0f;

	/** Forwarded to when the target is light enough to lift, or when the player is already holding one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TSubclassOf<UGameplayAbility> CarryAbilityClass;

	/** Forwarded to when the target is too heavy to lift: the hook moves the player to it instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Interact")
	TSubclassOf<UGameplayAbility> HookAbilityClass;
};
