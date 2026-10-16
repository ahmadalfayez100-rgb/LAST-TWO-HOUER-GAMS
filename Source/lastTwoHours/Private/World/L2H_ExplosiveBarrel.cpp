// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_ExplosiveBarrel.h"

#include "Camera/CameraShakeBase.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "TimerManager.h"

AL2H_ExplosiveBarrel::AL2H_ExplosiveBarrel()
{
	// MSHF's barrel and its four pieces; the explosion sound and shake from ResidentHorrorV2.
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModernSurvivalHorrorFramework/Asset/Mesh/Barrel/SM_Barrel.SM_Barrel"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	PieceMeshes.Reset();
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/ModernSurvivalHorrorFramework/Asset/Mesh/Barrel/SM_Barrel-p%d.SM_Barrel-p%d"), Index, Index);
		if (UStaticMesh* Piece = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			PieceMeshes.Add(Piece);
		}
	}
	BreakSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/ResidentHorrorV2/Audio/Cue/CUE_Explosion.CUE_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	ExplosionFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	ExplosionShake = LoadClass<UCameraShakeBase>(nullptr, TEXT("/Game/ResidentHorrorV2/Blueprints/BP_CameraShake/CS_Explosive.CS_Explosive_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	MaxHealth = 10.0f;
	LootShards = 0;
	PieceSpeed = 900.0f;
}

void AL2H_ExplosiveBarrel::Break(AActor* Breaker)
{
	if (!HasAuthority() || bBroken)
	{
		return;
	}
	Super::Break(Breaker);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Centre = Mesh->Bounds.Origin;
	MulticastExplosionFX();
	AL2H_EnemyRobot::ReportNoise(this, Centre, ExplosionRadius * 6.0f);

	auto Falloff = [this, Centre](const FVector& Where)
	{
		return FMath::Clamp(1.0f - FVector::Dist(Where, Centre) / ExplosionRadius, 0.0f, 1.0f);
	};

	// Players: through the director, like any hit (Health first, and the seconds it was worth).
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	for (TActorIterator<AL2H_BaseCharacter> It(World); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), Centre);
		const float Scale = Falloff(It->GetActorLocation());
		if (Director && Scale > 0.0f && !It->IsDowned() && !It->IsDead())
		{
			Director->ApplyTimeDamage(*It, PlayerDamage * Scale);
		}
		if (ExplosionShake && Dist < ShakeRadius)
		{
			It->PlayCameraShakeOnOwner(ExplosionShake, FMath::Lerp(1.5f, 0.3f, Dist / ShakeRadius));
		}
	}

	// Enemies: hurt and stunned.
	AController* BlastInstigator = Cast<APawn>(Breaker) ? Cast<APawn>(Breaker)->GetController() : nullptr;
	for (TActorIterator<AL2H_EnemyRobot> It(World); It; ++It)
	{
		const float Scale = Falloff(It->GetActorLocation());
		if (Scale > 0.0f && !It->IsDead())
		{
			UGameplayStatics::ApplyDamage(*It, EnemyDamage * Scale, BlastInstigator, this, nullptr);
			It->Stun(EnemyStunSeconds * Scale);
		}
	}

	// Other breakables: a moment later, so a row of barrels goes off one after another.
	TArray<TWeakObjectPtr<AL2H_Breakable>> Caught;
	for (TActorIterator<AL2H_Breakable> It(World); It; ++It)
	{
		if (*It != this && !It->IsBroken() && Falloff(It->GetActorLocation()) > 0.0f)
		{
			Caught.Add(*It);
		}
	}
	if (Caught.Num() > 0)
	{
		FTimerHandle Chain;
		World->GetTimerManager().SetTimer(Chain, FTimerDelegate::CreateWeakLambda(this, [this, Caught]()
		{
			for (const TWeakObjectPtr<AL2H_Breakable>& Weak : Caught)
			{
				if (AL2H_Breakable* Other = Weak.Get())
				{
					UGameplayStatics::ApplyDamage(Other, 1000.0f, nullptr, this, nullptr);
				}
			}
		}), FMath::Max(ChainDelay, 0.01f), false);
	}

	// Loose physics bodies are thrown.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity, FCollisionObjectQueryParams(ECC_PhysicsBody),
		FCollisionShape::MakeSphere(ExplosionRadius));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UPrimitiveComponent* Body = Overlap.GetComponent(); Body && Body->IsSimulatingPhysics())
		{
			Body->AddRadialImpulse(Centre, ExplosionRadius, 1200.0f, RIF_Linear, true);
		}
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Barrel: %s exploded (%.0f cm)"), *GetNameSafe(this), ExplosionRadius));
}

void AL2H_ExplosiveBarrel::MulticastExplosionFX_Implementation()
{
	if (ExplosionFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, Mesh->Bounds.Origin, FRotator::ZeroRotator, FVector(1.5f));
	}
}
