// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_ChronoBall.h"

#include "Ball/L2H_TimeBall.h"
#include "Character/L2H_BaseCharacter.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "VFX/L2H_VFXTypes.h"

UL2H_Ability_ChronoBall::UL2H_Ability_ChronoBall()
{
	// The ball is spawned and resolved on the server, so this never runs on a client.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// Instanced per actor so the cooldown survives between throws, and so a second press reaches the
	// same instance rather than a fresh one.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Time is spent through the director's GameplayEffects, never through an ability cost effect.
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	BallClass = AL2H_TimeBall::StaticClass();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Registered by the project's tag list. Skipped rather than asserted when it is missing, so a
		// tag list edit can never break the ability from loading.
		const FGameplayTag ChronoTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.ChronoBall")), false);
		if (ChronoTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(ChronoTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_ChronoBall::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();

	if (!Character || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A second press while a ball is out is a decision about that ball, not a new throw. Recall is the
	// baseline, so the ball comes back and the thrower never moves without the flight.
	// Only this ability's own kind of orb: the travel, slow and haste orbs are three abilities, and a
	// press of one must never recall or close another.
	const UClass* OwnClass = BallClass ? BallClass.Get() : AL2H_TimeBall::StaticClass();
	if (AL2H_TimeBall* Existing = AL2H_TimeBall::FindForOwner(this, Character, OwnClass))
	{
		Existing->HandleRepeatPress();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// One orb in the hand at a time; two would hang on the same palm and share one launch button.
	if (const AL2H_TimeBall* Other = AL2H_TimeBall::FindForOwner(this, Character))
	{
		if (Other->IsHeld())
		{
			L2HTime::LogImportant(TEXT("ChronoBall: refused -- another orb is already in the hand"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (Character->IsDowned())
	{
		L2HTime::LogImportant(TEXT("ChronoBall: refused -- at zero time there is nothing to throw"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Cooldown is a gameplay number on the ability rather than a cooldown effect, so a refused press
	// can report exactly how long is left.
	const float CooldownRemaining = FMath::Max(Limits.Cooldown - (World->GetTimeSeconds() - LastThrowTime), 0.0f);
	if (CooldownRemaining > 0.0f)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: refused -- %.1fs of cooldown left"), CooldownRemaining));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float CurrentTime = GetCurrentTime();
	if (CurrentTime <= Limits.CostFlat)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("ChronoBall: refused -- %.1fs of time left, the throw costs %.1fs"), CurrentTime, Limits.CostFlat));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AL2H_TimeDirector* TimeDirector = AL2H_TimeDirector::Get(Character);

	const FVector Direction = Character->GetControlRotation().Vector();
	// The orb appears where it will be held rather than at a throw point in front of the chest: it is
	// taken out, not thrown, so the first frame the player sees is already the right one. The old throw
	// point stays as the fallback for a skeleton with no HoldSocket.
	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	const FVector SpawnLocation = (CharacterMesh && !Limits.HoldSocket.IsNone())
		? CharacterMesh->GetSocketLocation(Limits.HoldSocket)
		: Character->GetActorLocation() + Direction * FMath::Max(Limits.ThrowForwardOffset, 0.0f)
			+ FVector(0.0f, 0.0f, Character->BaseEyeHeight * 0.5f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AL2H_TimeBall* Ball = World->SpawnActor<AL2H_TimeBall>(
		BallClass ? BallClass.Get() : AL2H_TimeBall::StaticClass(),
		SpawnLocation, Direction.Rotation(), SpawnParams);

	if (!Ball)
	{
		L2HTime::LogImportant(TEXT("ChronoBall: refused -- the ball could not be spawned"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Taking the orb out is paid for, not the release. The airborne part is settled when the orb
	// resolves, and putting it away again straight back at the hand refunds the flat part.
	if (TimeDirector)
	{
		TimeDirector->ApplyTimeCost(Character, Limits.CostFlat);
	}

	// Held rather than thrown: the orb waits in the hand until the launch button, which is what makes
	// the throw aimable. The ability still owns the spawn and the cost, so the orb is never live with
	// default limits.
	Ball->HoldInHand(Character, Limits, TimeDirector);

	// The moment the orb becomes real. Its flight and its arrival are the orb's to show, so this is the
	// one pulse the ability owns.
	Character->MulticastMomentVFX(EL2H_VFXMoment::ChronoBall, NAME_None);
	LastThrowTime = World->GetTimeSeconds();

	L2HTime::LogImportant(FString::Printf(TEXT("ChronoBall: %s took the orb out for %.1fs"),
		*GetNameSafe(Character), Limits.CostFlat));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
