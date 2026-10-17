// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_RecallAnchor.h"

#include "Ability/L2H_RecallAnchor.h"
#include "Character/L2H_BaseCharacter.h"
#include "Character/L2H_StruggleComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "Time/L2HTimeLog.h"
#include "VFX/L2H_VFXTypes.h"

UL2H_Ability_RecallAnchor::UL2H_Ability_RecallAnchor()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Placing and travelling both move replicated state, so the server decides and the client sees it.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Free by design, like the hook and the carry: coming back is not a way to buy time.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag RecallTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.RecallAnchor")), false);
		if (RecallTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(RecallTag);
			SetAssetTags(Tags);
		}
	}
}

AL2H_RecallAnchor* UL2H_Ability_RecallAnchor::FindAnchorFor(const APawn* Character) const
{
	UWorld* World = GetWorld();
	if (!World || !Character)
	{
		return nullptr;
	}

	// Scanning rather than caching a pointer: an anchor can be destroyed by a level reset or by the
	// character dying, and a stale pointer would send him to a mark that is no longer drawn on the floor.
	for (TActorIterator<AL2H_RecallAnchor> It(World); It; ++It)
	{
		AL2H_RecallAnchor* Anchor = *It;
		if (IsValid(Anchor) && Anchor->GetAnchorOwner() == Character)
		{
			return Anchor;
		}
	}

	return nullptr;
}

void UL2H_Ability_RecallAnchor::ActivateAbility(
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
	UWorld* World = GetWorld();

	if (!Character || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Character->IsDowned())
	{
		L2HTime::LogImportant(TEXT("Recall: refused -- at zero time there is no way back"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Held by the robot, a recall would be a free escape from the grab and leave the grab half-ended.
	if (UL2H_StruggleComponent::IsGrabbed(Character))
	{
		L2HTime::LogImportant(TEXT("Recall: refused -- grabbed"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AL2H_RecallAnchor* Existing = FindAnchorFor(Character))
	{
		const FVector Destination = Existing->GetReturnLocation();

		// bNoCheck stays false so TeleportTo is allowed to slide the spot somewhere he actually fits, and so
		// its return value is the only proof the move happened.
		const bool bMoved = Character->TeleportTo(Destination, Character->GetActorRotation(), false, false);

		// The mark is spent either way: a refused return must not leave a marker behind that the player
		// believes he can still use.
		Existing->Destroy();

		if (!bMoved)
		{
			L2HTime::LogImportant(TEXT("Recall: refused -- the mark is not somewhere he can stand"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}

		Character->MulticastMomentVFX(EL2H_VFXMoment::RecallReturn, NAME_None, 1.0f);

		L2HTime::LogImportant(FString::Printf(TEXT("Recall: %s returned to the mark at %s"),
			*GetNameSafe(Character), *Destination.ToCompactString()));

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!AnchorClass)
	{
		L2HTime::LogImportant(TEXT("Recall: refused -- no anchor class is set on the ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The mark goes slightly ahead of him and is dropped to the floor, so it reads as drawn on the ground
	// rather than worn on his feet.
	const FVector Forward = Character->GetActorForwardVector();
	FVector MarkLocation = Character->GetActorLocation()
		+ Forward * FMath::Max(PlaceForwardOffset, 0.0f);

	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HRecallAnchorFloor), false, Character);

		// Facing a wall, the point ahead is on its far side, and the floor found there is another room:
		// the return would walk him through the wall. The mark stops short of whatever is in the way.
		FHitResult Wall;
		if (World->LineTraceSingleByChannel(Wall, Character->GetActorLocation(), MarkLocation, ECC_Visibility, Params))
		{
			MarkLocation = Wall.Location - Forward * 25.0f;
		}
		FHitResult Hit;

		const FVector TraceStart = MarkLocation + FVector(0.0f, 0.0f, 50.0f);
		const FVector TraceEnd = MarkLocation - FVector(0.0f, 0.0f, FMath::Max(FloorTraceDistance, 10.0f));

		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			MarkLocation = Hit.ImpactPoint;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AL2H_RecallAnchor* Anchor = World->SpawnActor<AL2H_RecallAnchor>(
		AnchorClass, MarkLocation, FRotator::ZeroRotator, SpawnParams);

	if (!Anchor)
	{
		L2HTime::LogImportant(TEXT("Recall: refused -- the mark could not be placed"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Anchor->SetAnchorOwner(Character);

	L2HTime::LogImportant(FString::Printf(TEXT("Recall: %s placed a mark at %s"),
		*GetNameSafe(Character), *MarkLocation.ToCompactString()));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
