// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_TimeGameplayAbility.h"
#include "Character/L2H_BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Grab/L2H_GrabbableInterface.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2H_TimeAttributeSet.h"
#include "Time/L2HTimeLog.h"

UL2H_TimeGameplayAbility::UL2H_TimeGameplayAbility()
{
	// Every TimeCore ability mutates authoritative state, so it executes on the server and
	// is only ever triggered from the owning client.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

UL2H_TimeAttributeSet* UL2H_TimeGameplayAbility::GetTimeAttributeSet() const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		return const_cast<UL2H_TimeAttributeSet*>(ASC->GetSet<UL2H_TimeAttributeSet>());
	}

	return nullptr;
}

float UL2H_TimeGameplayAbility::GetCurrentTime() const
{
	const UL2H_TimeAttributeSet* TimeSet = GetTimeAttributeSet();
	return TimeSet ? TimeSet->GetTime() : 0.0f;
}

float UL2H_TimeGameplayAbility::GetMaxTime() const
{
	const UL2H_TimeAttributeSet* TimeSet = GetTimeAttributeSet();
	return TimeSet ? TimeSet->GetMaxTime() : 0.0f;
}

bool UL2H_TimeGameplayAbility::HasServerAuthority() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar && Avatar->HasAuthority();
}

AActor* UL2H_TimeGameplayAbility::FindGrabTarget(float Range, FVector& OutHitLocation, FVector& OutViewDirection, bool bFromCamera) const
{
	OutHitLocation = FVector::ZeroVector;
	OutViewDirection = FVector::ForwardVector;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	if (!Avatar || !World || Range <= 0.0f)
	{
		return nullptr;
	}

	// Eyes, not the actor origin. The player aims with the camera, so a trace from the capsule centre
	// would grab whatever is behind whatever the crosshair is actually on.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	const AL2H_BaseCharacter* AimingCharacter = bFromCamera ? Cast<AL2H_BaseCharacter>(Avatar) : nullptr;
	if (AimingCharacter)
	{
		AimingCharacter->GetAimViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	OutViewDirection = ViewRotation.Vector();

	// The avatar is ignored so a carried object cannot be grabbed again by the pawn holding it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2H_GrabTrace), /*bTraceComplex=*/false, Avatar);
	Params.bReturnPhysicalMaterial = false;

	const FVector TraceEnd = ViewLocation + OutViewDirection * Range;

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params);
	OutHitLocation = bHit ? Hit.ImpactPoint : TraceEnd;

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (HitActor && IsGrabCandidate(HitActor, Avatar))
	{
		return HitActor;
	}

	// The hook aims, so a miss is a miss. A hand-reach is different: in third person the camera looks
	// ahead, and a crate at the player's feet sits under the line. Without this the player has to stare
	// at the floor to pick anything up, which reads as "E does nothing".
	if (bFromCamera)
	{
		return nullptr;
	}

	const FVector Forward = Avatar->GetActorForwardVector().GetSafeNormal2D();
	const FVector ReachCentre = Avatar->GetActorLocation() + Forward * 90.0f;
	const float ReachRadius = 110.0f;

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, ReachCentre, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(ReachRadius), Params);

	AActor* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Candidate == Best || !IsGrabCandidate(Candidate, Avatar))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), Avatar->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	if (Best)
	{
		OutHitLocation = Best->GetActorLocation();
	}

	return Best;
}

bool UL2H_TimeGameplayAbility::IsGrabCandidate(const AActor* Candidate, const AActor* Avatar) const
{
	if (!Candidate || Candidate == Avatar)
	{
		return false;
	}

	// A fallen partner is the one thing with no profile that can still be picked up: the co-op carry.
	if (const AL2H_BaseCharacter* Pawn = Cast<AL2H_BaseCharacter>(Candidate))
	{
		return (Pawn->IsDowned() || Pawn->IsDead()) && !Pawn->IsBeingCarried();
	}

	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(Candidate);
	const bool bOptedIn = Candidate->Implements<UL2H_GrabbableInterface>();

	if (!Profile && !bOptedIn)
	{
		// Most of the world is scenery. Silent on purpose -- the hook and carry log their own outcome,
		// and logging every wall the player looks at would drown the interesting refusals.
		return false;
	}

	// The veto is data on the profile rather than a code path on the actor: a locked door is a
	// checkbox the level can flip at runtime, not a subclass somebody has to write.
	if (Profile && !Profile->bCanBeGrabbed)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Grab: %s is locked"), *GetNameSafe(Candidate)));
		return false;
	}

	// Something already in another player's hands is not up for grabs.
	if (const AActor* Parent = Candidate->GetAttachParentActor())
	{
		if (Cast<AL2H_BaseCharacter>(Parent))
		{
			return false;
		}
	}

	return true;
}

float UL2H_TimeGameplayAbility::ResolveCarryMass(const AActor* Target)
{
	if (const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(Target))
	{
		return Profile->ResolveMass();
	}

	return Cast<AL2H_BaseCharacter>(Target) ? CarriedPawnMass : 0.0f;
}
