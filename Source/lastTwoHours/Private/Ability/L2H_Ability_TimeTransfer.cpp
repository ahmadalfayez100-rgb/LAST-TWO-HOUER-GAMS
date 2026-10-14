// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_TimeTransfer.h"

#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_TimeTransfer::UL2H_Ability_TimeTransfer()
{
	// Both the payment and the payment's arrival are server-owned.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag TransferTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.TimeTransfer")), false);
		if (TransferTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(TransferTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_TimeTransfer::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AL2H_BaseCharacter* Caster = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);

	if (!HasServerAuthority() || !Caster || !Director)
	{
		L2HTime::LogImportant(TEXT("TimeTransfer: refused -- the level has no time director"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The partner, wherever they are. The director owns the rule (packet, tax, free window, remote revive
	// threshold) so this button and an accepted request pay exactly the same way.
	AL2H_BaseCharacter* Partner = Director->FindPartnerOf(Caster);
	const bool bGiven = Partner && Director->TransferTime(Caster, Partner);
	if (!Partner)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("TimeTransfer: %s refused -- no partner in this run"), *GetNameSafe(Caster)));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bGiven);
}
