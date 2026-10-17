// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_ShrinkComponent.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Time/L2HTimeLog.h"
#include "VFX/L2H_VFXTypes.h"

UL2H_ShrinkComponent::UL2H_ShrinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// The shrunk flag has to reach clients; the visible resize is derived from it, never replicated itself.
	SetIsReplicatedByDefault(true);
}

void UL2H_ShrinkComponent::BeginPlay()
{
	Super::BeginPlay();

	CaptureBaseState();

	// A late joiner receives bShrunk without a transition, so the first apply has to happen here as well
	// as from OnRep.
	ApplyState();
}

void UL2H_ShrinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ShrinkRingComponent)
	{
		ShrinkRingComponent->Deactivate();
		ShrinkRingComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UL2H_ShrinkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UL2H_ShrinkComponent, bShrunk);
}

void UL2H_ShrinkComponent::CaptureBaseState()
{
	if (bBaseCaptured)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		// Unscaled on purpose: SetCapsuleSize also writes unscaled, so capturing the scaled values would
		// shrink him a little further every time the state was reapplied.
		BaseCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		BaseCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		BaseMeshRelativeZ = Mesh->GetRelativeLocation().Z;
		BaseMeshRelativeScale = Mesh->GetRelativeScale3D();
	}

	BaseEyeHeight = Character->BaseEyeHeight;

	bBaseCaptured = true;
}

void UL2H_ShrinkComponent::ApplyState()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	if (!bBaseCaptured)
	{
		CaptureBaseState();
	}

	const float Scale = bShrunk ? FMath::Clamp(ShrinkScale, 0.05f, 1.0f) : 1.0f;
	const bool bTransition = bAppliedShrunk != bShrunk;

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		// The capsule resizes around its centre. Without moving the actor by the change in half height,
		// shrinking leaves him hanging in the air and growing sinks the bottom of the capsule into the
		// floor. Only the authority moves him; clients get the location through movement replication.
		const float OldHalf = Capsule->GetScaledCapsuleHalfHeight();
		Capsule->SetCapsuleSize(BaseCapsuleRadius * Scale, BaseCapsuleHalfHeight * Scale, true);
		if (bTransition && Character->HasAuthority())
		{
			const float Delta = Capsule->GetScaledCapsuleHalfHeight() - OldHalf;
			Character->SetActorLocation(Character->GetActorLocation() + FVector(0.0f, 0.0f, Delta + (Delta > 0.0f ? 1.0f : 0.0f)));
		}
	}

	// The capsule shrinks around its centre, so the mesh has to follow the new half height and scale with
	// it; scaling the offset by the same factor keeps his feet exactly on the floor he was standing on.
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		FVector RelativeLocation = Mesh->GetRelativeLocation();
		RelativeLocation.Z = BaseMeshRelativeZ * Scale;
		Mesh->SetRelativeLocation(RelativeLocation);
		Mesh->SetRelativeScale3D(BaseMeshRelativeScale * Scale);
	}

	Character->BaseEyeHeight = BaseEyeHeight * Scale;

	// Multiplied in and divided out on the transition only: other systems (gait, carry, aim, injury)
	// write MaxWalkSpeed too, and an absolute write here would undo whatever they had set.
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (bTransition && Movement && !FMath::IsNearlyEqual(ShrinkSpeedScale, 1.0f))
	{
		Movement->MaxWalkSpeed *= bShrunk ? ShrinkSpeedScale : 1.0f / FMath::Max(ShrinkSpeedScale, 0.05f);
	}
	bAppliedShrunk = bShrunk;

	// The ring belongs to the small state, so the system is spawned once and then only switched on and off.
	if (ShrinkRingSystem)
	{
		if (bShrunk)
		{
			if (!ShrinkRingComponent)
			{
				ShrinkRingComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
					ShrinkRingSystem, Character->GetMesh(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget, /*bAutoDestroy=*/false);
			}

			if (ShrinkRingComponent && !ShrinkRingComponent->IsActive())
			{
				ShrinkRingComponent->Activate(true);
			}
		}
		else if (ShrinkRingComponent && ShrinkRingComponent->IsActive())
		{
			ShrinkRingComponent->Deactivate();
		}
	}
}

float UL2H_ShrinkComponent::GetCurrentCapsuleHalfHeight() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	return BaseCapsuleHalfHeight * (bShrunk ? FMath::Clamp(ShrinkScale, 0.05f, 1.0f) : 1.0f);
}

bool UL2H_ShrinkComponent::HasRoomToGrow() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !World)
	{
		return false;
	}
	// Feet stay where they are, so the full capsule's centre sits higher by the difference in half height.
	const float SmallHalf = Capsule->GetScaledCapsuleHalfHeight();
	const FVector GrownCentre = Character->GetActorLocation() + FVector(0.0f, 0.0f, BaseCapsuleHalfHeight - SmallHalf + 2.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HShrinkGrowRoom), false, Character);
	return !World->OverlapBlockingTestByChannel(GrownCentre, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(BaseCapsuleRadius * 0.95f, BaseCapsuleHalfHeight * 0.98f), Params);
}

float UL2H_ShrinkComponent::GetBodyScale(const AActor* Who)
{
	if (!Who)
	{
		return 1.0f;
	}
	float Scale = Who->GetActorScale3D().Z;
	if (const UL2H_ShrinkComponent* Shrink = Who->FindComponentByClass<UL2H_ShrinkComponent>())
	{
		Scale *= Shrink->IsShrunk() ? FMath::Clamp(Shrink->ShrinkScale, 0.05f, 1.0f) : 1.0f;
	}
	return Scale;
}

bool UL2H_ShrinkComponent::SetShrunk(bool bNewShrunk)
{
	if (bShrunk == bNewShrunk)
	{
		return false;
	}

	// Growing under a low ceiling would put the full capsule inside it.
	if (!bNewShrunk && !HasRoomToGrow())
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Shrink: %s has no room to grow back here"), *GetNameSafe(GetOwner())));
		return false;
	}

	bShrunk = bNewShrunk;

	// The server is the only caller, and it applies immediately so the press has visible weight; clients
	// arrive through OnRep.
	ApplyState();

	if (AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner()))
	{
		// One burst serves both directions: the same shards read as him collapsing or unfolding.
		Character->MulticastMomentVFX(EL2H_VFXMoment::Shrink, NAME_None, 1.0f);
	}

	L2HTime::LogVerbose(FString::Printf(TEXT("Shrink: %s -> %s (x%.2f)"),
		*GetNameSafe(GetOwner()), bShrunk ? TEXT("small") : TEXT("full size"),
		bShrunk ? ShrinkScale : 1.0f));

	return true;
}

void UL2H_ShrinkComponent::OnRep_Shrunk()
{
	ApplyState();
}
