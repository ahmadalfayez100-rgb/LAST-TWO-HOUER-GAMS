// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "Ball/L2H_BallTypes.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "L2H_Ability_ChronoBall.generated.h"

class AL2H_TimeBall;

/**
 * Samer's fixed super ability: take the orb out, then commit to where it is.
 *
 * The ability is deliberately short-lived. It decides, pays the flat cost, spawns the orb and ends; the
 * orb then waits in the hand for the launch button -- which is what makes the throw aimable -- and owns
 * the flight and the resolution after that. A second press is therefore a decision about the orb that is
 * already out: put it away, recall it, or teleport to it now. It is never a second throw, and the ability
 * stays free of any long-running waiting state.
 *
 * The release itself is a separate class (UL2H_Ability_ChronoBallLaunch). A row of AbilityInputBindings
 * carries one action and one tag and no verb, so one class per meaning is the only way the same orb can
 * answer two different buttons.
 *
 * Nothing here trusts the client: the ball is spawned on the server and the direction comes from the
 * server's copy of the control rotation.
 */
UCLASS(meta = (DisplayName = "L2H Ability Chrono Ball"))
class LASTTWOHOURS_API UL2H_Ability_ChronoBall : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_ChronoBall();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** The actor to spawn. Point this at a Blueprint child to give the orb a mesh and a trail. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall")
	TSubclassOf<AL2H_TimeBall> BallClass;

	/** Every tunable number of the ability. Editable on the Blueprint child, so tuning needs no rebuild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|ChronoBall")
	FL2H_BallLimits Limits;

	/** World time of the last successful throw, for the cooldown. Server-side only, so it is not replicated. */
	float LastThrowTime = -1000.0f;
};
