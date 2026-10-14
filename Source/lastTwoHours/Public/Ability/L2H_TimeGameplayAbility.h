// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"

#include "L2H_TimeGameplayAbility.generated.h"

class UL2H_TimeAttributeSet;

/**
 * Common base for every TimeCore ability: the shared accessors and the authority check
 * that every time mutation goes through.
 *
 * Tags are set with SetAssetTags()/GetAssetTags(). AbilityTags is deprecated from 5.5.
 */
UCLASS(Abstract)
class LASTTWOHOURS_API UL2H_TimeGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_TimeGameplayAbility();

	/** The owner's time attribute set, or null before actor info is set. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	UL2H_TimeAttributeSet* GetTimeAttributeSet() const;

	/** Current replicated Time value; 0 when the attribute set is unavailable. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	float GetCurrentTime() const;

	/** Current replicated MaxTime value; 0 when the attribute set is unavailable. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	float GetMaxTime() const;

	/** True on standalone, a listen server, or the server in a client session. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool HasServerAuthority() const;

protected:
	/**
	 * Traces forward from the avatar's eyes for the first actor that can be grabbed.
	 *
	 * An actor qualifies when it carries a UL2H_MassProfileComponent or implements
	 * UL2H_GrabbableInterface -- the component is how a designer says "this has a weight", the
	 * interface is how a Blueprint says "and it may only be grabbed when unlocked". An interface actor
	 * is asked to veto first, so a refusal never reaches the mass code and cannot be overridden by
	 * being light enough.
	 *
	 * @param Range            How far ahead to look, in cm.
	 * @param OutHitLocation   Impact point. This is what the hook aims at, not the actor origin.
	 * @param OutViewDirection Normalised trace direction, for logging and fallbacks.
	 * @param bFromCamera      Trace from the player's camera (long-range aiming, the hook) instead of the
	 *                         eyes (reach-range grabs, where a trace from behind the head would fall short).
	 * @return The grabbable actor, or null when the trace found nothing grabbable. OutHitLocation is
	 *         filled either way (the impact, or the end of the trace), so a miss can still be drawn.
	 */
	AActor* FindGrabTarget(float Range, FVector& OutHitLocation, FVector& OutViewDirection, bool bFromCamera = false) const;

	/**
	 * Mass the carry rules use for Target: the mass profile when there is one, CarriedPawnMass for a
	 * downed or dead partner (who has no profile), 0 otherwise.
	 */
	static float ResolveCarryMass(const AActor* Target);

	/** A partner's body counts as this heavy: under the solo band, so one player can always carry it. */
	static constexpr float CarriedPawnMass = 50.0f;

private:
	/** True for anything the grab may return: a profile, the grabbable interface, or a fallen partner. */
	bool IsGrabCandidate(const AActor* Candidate, const AActor* Avatar) const;
};
