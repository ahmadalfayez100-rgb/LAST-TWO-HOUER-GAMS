// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/L2H_TimeGameplayAbility.h"
#include "CoreMinimal.h"

#include "L2H_Ability_TimeTransfer.generated.h"

class AL2H_BaseCharacter;

/**
 * Time transfer: hand one packet of seconds to the partner, at any distance.
 *
 * The rule lives in AL2H_TimeDirector::TransferTime, shared with an accepted request: TransferAmount
 * per packet, TransferFirstTax on the first packet of a burst and none inside TransferFreeWindow after
 * it, and a partner on the floor stood up only once RemoteReviveThreshold seconds have arrived. Every
 * refusal is logged, because a silent no-op reads as a broken button.
 */
UCLASS(meta = (DisplayName = "L2H Ability Time Transfer"))
class LASTTWOHOURS_API UL2H_Ability_TimeTransfer : public UL2H_TimeGameplayAbility
{
	GENERATED_BODY()

public:
	UL2H_Ability_TimeTransfer();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

};
