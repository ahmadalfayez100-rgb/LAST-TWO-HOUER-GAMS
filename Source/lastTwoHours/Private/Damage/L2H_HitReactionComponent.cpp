// Copyright Epic Games, Inc. All Rights Reserved.

#include "Damage/L2H_HitReactionComponent.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Damage/L2H_DamageTypeDataAsset.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2HTimeLog.h"

UL2H_HitReactionComponent::UL2H_HitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// A multicast RPC on a component needs the component to exist on every machine that is meant to play
	// the reaction, so it opts in rather than relying on a Blueprint checkbox being ticked.
	SetIsReplicatedByDefault(true);
}

const FL2H_DamageResponse* UL2H_HitReactionComponent::FindResponse(EL2H_DamageType DamageType) const
{
	const TObjectPtr<UL2H_DamageTypeDataAsset>* Found = DamageResponses.Find(DamageType);

	if (!Found || !(*Found))
	{
		return nullptr;
	}

	return &(*Found)->Response;
}

bool UL2H_HitReactionComponent::TryGetResponse(EL2H_DamageType DamageType, FL2H_DamageResponse& OutResponse) const
{
	if (const FL2H_DamageResponse* Found = FindResponse(DamageType))
	{
		OutResponse = *Found;
		return true;
	}

	return false;
}

void UL2H_HitReactionComponent::ApplyHitReaction(EL2H_DamageType DamageType, AActor* InstigatorActor, bool bHeadshot, FVector HitLocation, FVector ShotDirection)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// A null shake class is not an error: an actor authored without one, or one whose shake asset has not
	// been made yet, simply has no kick. Everything else about the hit still happens.
	if (const FL2H_DamageResponse* Response = FindResponse(DamageType))
	{
		if (Response->CameraShakeClass)
		{
			if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
			{
				if (APlayerController* ShooterPC = Cast<APlayerController>(InstigatorPawn->GetController()))
				{
					ShooterPC->ClientStartCameraShake(Response->CameraShakeClass, bHeadshot ? HeadshotShakeScale : 1.0f);
				}
			}
		}
	}

	MulticastPlayHitEffects(DamageType, bHeadshot, HitLocation, ShotDirection);
}

void UL2H_HitReactionComponent::MulticastPlayHitEffects_Implementation(EL2H_DamageType DamageType, bool bHeadshot, FVector HitLocation, FVector ShotDirection)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Each machine resolves the response from its own copy of the authored map rather than being sent the
	// assets: the defaults are identical everywhere, so sending two object references per hit would be
	// paying to transmit something both sides already hold.
	const FL2H_DamageResponse* Response = FindResponse(DamageType);
	if (!Response)
	{
		return;
	}

	// The reaction plays on the thing that was hit. A crate has no skeletal mesh and skips this; an actor
	// with no montage authored skips it too.
	if (Response->HitMontage)
	{
		if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Response->HitMontage);
			}
		}
	}

	if (Response->ImpactEffect)
	{
		// Rotated back along the shot, so a spark leaves the surface the way the round came in.
		UGameplayStatics::SpawnEmitterAtLocation(Owner, Response->ImpactEffect, HitLocation, (-ShotDirection).Rotation());
	}

	if (Response->ImpactSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(Owner, Response->ImpactSound, HitLocation);
	}

	L2HTime::LogVerbose(FString::Printf(
		TEXT("Hit reaction: %s took damage type %d%s"),
		*GetNameSafe(Owner),
		static_cast<int32>(DamageType),
		bHeadshot ? TEXT(" to the head") : TEXT("")));
}
