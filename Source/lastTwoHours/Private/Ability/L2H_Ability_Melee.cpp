// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_Melee.h"

#include "Animation/AnimMontage.h"
#include "Character/L2H_BaseCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "World/L2H_Breakable.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2HTimeLog.h"
#include "TimerManager.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"

UL2H_Ability_Melee::UL2H_Ability_Melee()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;
}

void UL2H_Ability_Melee::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	const UL2H_WeaponComponent* Weapons = Character ? Character->FindComponentByClass<UL2H_WeaponComponent>() : nullptr;
	const UL2H_WeaponDataAsset* Weapon = Weapons ? Weapons->GetActiveWeaponData() : nullptr;
	UWorld* World = GetWorld();
	if (!HasServerAuthority() || !Character || !Weapon || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float Now = World->GetTimeSeconds();
	const TArray<TObjectPtr<UAnimMontage>>& Swings = Weapon->Data.Animations.CharacterAttack;
	if (Now < BusyUntil || Swings.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// The next clip of the combo while the last one has just finished, otherwise the first again.
	ComboIndex = (Now <= LastSwingEnd + ComboWindow) ? (ComboIndex + 1) % Swings.Num() : 0;
	UAnimMontage* Swing = Swings[ComboIndex];
	const float Length = Swing ? FMath::Max(Swing->GetPlayLength(), 0.2f) : 0.6f;
	BusyUntil = Now + Length * CommitFraction;
	LastSwingEnd = Now + Length;

	// Planted for the committed part of the swing; the follow-through can be walked out of.
	Character->PlayInteractAnim(Swing, FVector::ZeroVector, 0.0f, Length * CommitFraction);

	// The blow lands partway through the clip. The swinger is held weakly: a pawn gone by then hits nothing.
	FTimerHandle HitTimer;
	TWeakObjectPtr<AL2H_BaseCharacter> WeakSwinger(Character);
	TWeakObjectPtr<const UL2H_WeaponDataAsset> WeakWeapon(Weapon);
	const float Reach = HitReach, Radius = HitRadius, Stun = StunSeconds;
	World->GetTimerManager().SetTimer(HitTimer, FTimerDelegate::CreateWeakLambda(Character, [WeakSwinger, WeakWeapon, Reach, Radius, Stun]()
	{
		ApplyHit(WeakSwinger.Get(), WeakWeapon.Get(), Reach, Radius, Stun);
	}), FMath::Max(Length * HitFraction, 0.05f), false);

	L2HTime::LogVerbose(FString::Printf(TEXT("Melee: %s swing %d (%s)"), *GetNameSafe(Character), ComboIndex, *GetNameSafe(Swing)));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UL2H_Ability_Melee::ApplyHit(AL2H_BaseCharacter* Swinger, const UL2H_WeaponDataAsset* Weapon, float Reach, float Radius, float Stun)
{
	UWorld* World = Swinger ? Swinger->GetWorld() : nullptr;
	if (!World || !Weapon || Swinger->IsDowned() || Swinger->IsDead())
	{
		return;
	}

	// The pipe upgrades like a gun: firepower levels raise every blow.
	const UL2H_WeaponComponent* Weapons = Swinger->FindComponentByClass<UL2H_WeaponComponent>();
	const float Damage = Weapons ? Weapons->GetDamageFor(Weapon) : Weapon->Data.Damage;

	const FVector Centre = Swinger->GetActorLocation() + Swinger->GetActorForwardVector() * Reach;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HMelee), false, Swinger);
	World->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Struck;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AL2H_EnemyRobot* Enemy = Cast<AL2H_EnemyRobot>(Overlap.GetActor());
		if (!Enemy || Enemy->IsDead() || Struck.Contains(Enemy))
		{
			continue;
		}
		Struck.Add(Enemy);
		UGameplayStatics::ApplyDamage(Enemy, Damage, Swinger->GetController(), Swinger, nullptr);
		Enemy->Stun(Stun);
	}

	// Crates and barrels in reach take the blow too.
	TArray<FOverlapResult> Props;
	FCollisionObjectQueryParams PropTypes;
	PropTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	PropTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	World->OverlapMultiByObjectType(Props, Centre, FQuat::Identity, PropTypes, FCollisionShape::MakeSphere(Radius), Params);
	for (const FOverlapResult& Overlap : Props)
	{
		AL2H_Breakable* Prop = Cast<AL2H_Breakable>(Overlap.GetActor());
		if (Prop && !Prop->IsBroken() && !Struck.Contains(Prop))
		{
			Struck.Add(Prop);
			UGameplayStatics::ApplyDamage(Prop, Damage, Swinger->GetController(), Swinger, nullptr);
		}
	}

	if (Struck.Num() > 0)
	{
		// The impact is felt on the swinger's camera too, lightly.
		Swinger->PlayCameraShakeOnOwner(Swinger->HitCameraShake, 0.35f);
		L2HTime::LogImportant(FString::Printf(TEXT("Melee: %s struck %d target(s) for %.0f"), *GetNameSafe(Swinger), Struck.Num(), Damage));
	}
}
