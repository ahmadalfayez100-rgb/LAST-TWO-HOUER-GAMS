// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Shrink.h"

#include "Character/L2H_BaseCharacter.h"
#include "Character/L2H_ShrinkComponent.h"
#include "Character/L2H_StruggleComponent.h"
#include "GameplayTagContainer.h"
#include "Time/L2HTimeLog.h"

UL2H_Ability_Shrink::UL2H_Ability_Shrink()
{
	// Instanced so a second press reaches the same instance rather than building a new one.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// The capsule, the mesh and the speed all change, and every one of them is decided once on the server.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Free by design, like the hook and the carry: shrinking is a way around the level, not a purchase.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Registered by the project's tag list. Skipped rather than asserted when it is missing, so a tag
		// list edit can never stop the ability from loading.
		const FGameplayTag ShrinkTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Shrink")), false);
		if (ShrinkTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(ShrinkTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_Shrink::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!HasServerAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Character->IsDowned())
	{
		L2HTime::LogImportant(TEXT("Shrink: refused -- at zero time there is nothing to shrink"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The carried actor is attached to the mesh, so resizing the mesh underneath it would leave the object
	// hanging in mid air at its old offset. Putting it down first is the only honest order.
	if (Character->IsCarrying())
	{
		L2HTime::LogImportant(TEXT("Shrink: refused -- put the carried object down first"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UL2H_ShrinkComponent* Shrink = Character->FindComponentByClass<UL2H_ShrinkComponent>();
	if (!Shrink)
	{
		L2HTime::LogImportant(TEXT("Shrink: refused -- the character has no shrink component"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Held by the robot the body is posed by the grab; resizing it there would tear the two apart.
	if (UL2H_StruggleComponent::IsGrabbed(Character))
	{
		L2HTime::LogImportant(TEXT("Shrink: refused -- grabbed"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bWasShrunk = Shrink->IsShrunk();
	if (!Shrink->SetShrunk(!bWasShrunk))
	{
		// Growing back with no room for the full body: he stays small.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Shrink: %s is now %s (x%.2f)"),
		*GetNameSafe(Character), bWasShrunk ? TEXT("full size") : TEXT("small"), Shrink->GetShrinkScale()));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
