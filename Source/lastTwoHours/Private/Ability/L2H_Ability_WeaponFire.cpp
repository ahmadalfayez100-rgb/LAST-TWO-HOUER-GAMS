// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ability/L2H_Ability_WeaponFire.h"

#include "AbilitySystemComponent.h"
#include "Character/L2H_BaseCharacter.h"
#include "Damage/L2H_DamageTypeDataAsset.h"
#include "Damage/L2H_HitReactionComponent.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "Character/L2H_StruggleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "Weapon/L2H_Projectile.h"
#include "Weapon/L2H_WeaponActor.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"

UL2H_Ability_WeaponFire::UL2H_Ability_WeaponFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	CostGameplayEffectClass = nullptr;
	CooldownGameplayEffectClass = nullptr;

	// The input row that fires this ability matches on a tag rather than on the class, so a pawn reaches
	// it through ServerTryActivateAbilityByTag like every other ability. Set on the CDO only, because
	// that is the object the ability system reads the tags from.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FGameplayTag FireTag =
			FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Weapon.Fire")), false);
		if (FireTag.IsValid())
		{
			FGameplayTagContainer Tags;
			Tags.AddTag(FireTag);
			SetAssetTags(Tags);
		}
	}
}

void UL2H_Ability_WeaponFire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Every path out of this ability ends it, so the next press can activate it again. An instanced
	// ability whose activation is left open would swallow every shot after the first.
	const auto Finish = [this, Handle, ActorInfo, ActivationInfo](bool bWasCancelled)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, bWasCancelled);
	};

	if (!HasServerAuthority())
	{
		Finish(true);
		return;
	}

	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		L2HTime::LogImportant(TEXT("Fire refused: the avatar is not an L2H character"));
		Finish(true);
		return;
	}

	// Holding something turns the fire button into the throw. This is the same hand-over the hook uses
	// through its weapon data, and it is the whole reason carry, throw and shooting never contend for an
	// input: the pawn's state decides what the press means, not a second binding.
	if (Character->IsCarrying())
	{
		UAbilitySystemComponent* ThrowASC = GetAbilitySystemComponentFromActorInfo();
		const TSubclassOf<UGameplayAbility> ThrowClass = Character->GetThrowAbilityClass();

		if (!ThrowClass)
		{
			L2HTime::LogImportant(FString::Printf(
				TEXT("Throw refused: %s has no ThrowAbilityClass"), *GetNameSafe(Character)));
			Finish(false);
			return;
		}

		// Granted on the pawn already, but given here too so a loadout cannot leave the throw unreachable.
		if (ThrowASC && !ThrowASC->FindAbilitySpecFromClass(ThrowClass))
		{
			ThrowASC->GiveAbility(FGameplayAbilitySpec(ThrowClass, 1));
		}

		const bool bThrown = ThrowASC && ThrowASC->TryActivateAbilityByClass(ThrowClass);

		// The throw draws no weapon line and spends no round, so the ability ends here rather than
		// falling through to the shot below.
		Finish(!bThrown);
		return;
	}

	UL2H_WeaponComponent* Weapons = Character->FindComponentByClass<UL2H_WeaponComponent>();
	if (!Weapons)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Fire refused: %s has no weapon component"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// The weapon only comes out while the aim button is held, so a shot from the hip would leave a
	// gun that is not even drawn. A weapon whose "shot" is another ability is exempt: the hook is a
	// traversal tool, not a shot, and it answers the press with the gun down. In mid-air the aim state is
	// blocked by the fall, so without this exemption the gate below refused the hook outright while airborne.
	const UL2H_WeaponDataAsset* ActiveData = Weapons->GetActiveWeaponData();
	const bool bIsTool = ActiveData && ActiveData->Data.FireAbilityOverride;
	// Held by an enemy: the hands are busy breaking free. Shrunk: too small to hold the gun.
	if (UL2H_StruggleComponent::IsGrabbed(Character) || Character->GetActorScale3D().Z < 0.8f)
	{
		Finish(false);
		return;
	}

	if (!Weapons->IsAiming() && !bIsTool)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Fire refused: %s is not aiming"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// The weapon's FireRate is a ceiling on shots per second. It was declared and never read, so a pistol
	// (5/s) and a shotgun (1.2/s) fired as fast as the button could be pressed. Held automatic fire is
	// requested every frame and relies on this to come out at the authored rate.
	const float FireRate = Weapons->GetFireRateFor(ActiveData);
	if (!bIsTool && ActiveData && FireRate > 0.0f && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastShotTime < 1.0f / FireRate - 0.005f)
		{
			Finish(false);
			return;
		}
		LastShotTime = Now;
	}

	// A weapon whose "shot" is another ability (the hook) hands the press over and stops here. The ability
	// is granted on first use so a loadout entry is enough; nobody has to remember a matching ability row.
	if (bIsTool)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const TSubclassOf<UGameplayAbility> Override = ActiveData->Data.FireAbilityOverride;
		if (ASC && !ASC->FindAbilitySpecFromClass(Override))
		{
			ASC->GiveAbility(FGameplayAbilitySpec(Override, 1));
		}
		const bool bActivated = ASC && ASC->TryActivateAbilityByClass(Override);
		L2HTime::LogImportant(FString::Printf(
			TEXT("Fire: %s's %s activates %s (%s)"),
			*GetNameSafe(Character), *GetNameSafe(ActiveData), *GetNameSafe(Override.Get()),
			bActivated ? TEXT("ok") : TEXT("refused")));

		// A weapon whose shot is another ability still fires visibly. The flash leaving the barrel is the only
		// thing telling the player the press registered before the hook lands.
		if (bActivated)
		{
			Weapons->PlayFireAnimation();
		}

		Finish(!bActivated);
		return;
	}

	if (!Weapons->CanFire())
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Fire refused: %s has %s"),
			*GetNameSafe(Character),
			Weapons->GetActiveWeaponData() ? TEXT("an empty magazine") : TEXT("nothing in hand")));
		Finish(false);
		return;
	}

	UL2H_WeaponDataAsset* WeaponData = Weapons->GetActiveWeaponData();
	AL2H_WeaponActor* Weapon = Weapons->GetActiveWeapon();
	UWorld* World = GetWorld();

	if (!WeaponData || !Weapon || !World)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Fire refused: %s is missing its weapon actor or world"), *GetNameSafe(Character)));
		Finish(false);
		return;
	}

	// Direction from the eyes, origin from the muzzle. Firing straight down the barrel would make the
	// crosshair a lie, and firing from the eyes alone would let the weapon poke through a wall the
	// character is standing against.
	// The camera, not the eyes: in third person the camera sits behind and beside the head, so a trace from
	// the eyes runs parallel to the crosshair and misses what the player is pointing at.
	FVector EyesLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	Character->GetAimViewPoint(EyesLocation, ViewRotation);

	const FVector MuzzleLocation = Weapon->GetFireSocketTransform().GetLocation();
	const FVector ShotDirection = ViewRotation.Vector();
	const FVector TraceEnd = EyesLocation + ShotDirection * WeaponData->Data.FireRange;

	FCollisionQueryParams TraceParams(FName(TEXT("L2HWeaponFire")), /*bTraceComplex=*/false, Character);

	// The weapon hangs off the character, which is already ignored, but it is named here too so that a
	// weapon held by someone else cannot be the thing this shot hits.
	TraceParams.AddIgnoredActor(Weapon);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, EyesLocation, TraceEnd, TraceChannel, TraceParams);

	// Spent whether or not it connected. A missed shot that costs nothing makes the magazine pointless.
	Weapons->ServerConsumeRound();

	// A shot is heard: robots within earshot come to look where it was fired from.
	AL2H_EnemyRobot::ReportNoise(Character, Character->GetActorLocation(), ShotNoiseRadius);

	// Played here rather than after the hit test, because a miss is still a shot and the gun has to move
	// either way.
	Weapons->PlayFireAnimation();

	// A physical round: it leaves the muzzle aimed at whatever the crosshair is on and flies under
	// gravity, so the trace above only chose the aim point -- the round decides what it actually hits.
	if (WeaponData->Data.ProjectileClass)
	{
		const FVector AimPoint = bHit ? Hit.ImpactPoint : TraceEnd;
		const FVector LaunchDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AL2H_Projectile* Round = World->SpawnActor<AL2H_Projectile>(
			WeaponData->Data.ProjectileClass, MuzzleLocation, LaunchDirection.Rotation(), SpawnParams))
		{
			Round->InitShot(Character, Weapons->GetDamageFor(WeaponData), WeaponData->Data.DamageType, HeadshotMultiplier,
				LaunchDirection, WeaponData->Data.ProjectileSpeed);
		}

		Finish(false);
		return;
	}

	// A visible line from the muzzle to whatever the crosshair was on, so a shot reads even without a
	// hit reaction on the target.
	Character->MulticastShotFX(MuzzleLocation, bHit ? Hit.ImpactPoint : TraceEnd, /*bHook=*/false);

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (!HitActor)
	{
		L2HTime::LogVerbose(FString::Printf(TEXT("Fire: %s missed"), *GetNameSafe(WeaponData)));
		Finish(false);
		return;
	}

	const bool bHeadshot = HeadBoneName != NAME_None && Hit.BoneName == HeadBoneName;

	// The response belongs to the thing being hit, not to the gun: the same pistol round has to be able to
	// mean something different to a crate and to a creature, so the target is asked rather than assumed.
	UL2H_HitReactionComponent* Reactions = HitActor->FindComponentByClass<UL2H_HitReactionComponent>();
	const FL2H_DamageResponse* Response = Reactions ? Reactions->FindResponse(WeaponData->Data.DamageType) : nullptr;

	// Falling back to the weapon's own numbers keeps an un-authored target hittable rather than immune.
	const float BaseDamage = Response ? Response->TimeDamage : WeaponData->Data.Damage;
	const float Multiplier = Response ? Response->HeadshotMultiplier : HeadshotMultiplier;
	// Upgrades scale whatever the base was, the target's own response included.
	const float Firepower = WeaponData->Data.Damage > 0.0f ? Weapons->GetDamageFor(WeaponData) / WeaponData->Data.Damage : 1.0f;
	const float DamageToApply = BaseDamage * Firepower * (bHeadshot ? Multiplier : 1.0f);

	// The engine's own damage path as well as the director's: anything with its own TakeDamage -- the
	// test robot, a breakable -- hears the hit without the weapon having to know what it is.
	UGameplayStatics::ApplyPointDamage(HitActor, DamageToApply, ShotDirection, Hit, Character->GetController(), Character, nullptr);

	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(Character);
	if (!Director)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Fire: no director here, so %s took no time damage"),
			*GetNameSafe(HitActor)));
		Finish(false);
		return;
	}

	// Time only ever moves through the director. A weapon that wrote the attribute itself would be a
	// second pathway to the one number the whole game is built on.
	Director->ApplyTimeDamage(HitActor, DamageToApply);

	// Damage first, reaction second, so a target that goes down from the hit is already down by the time
	// the montage and the shake are asked for.
	if (Reactions)
	{
		Reactions->ApplyHitReaction(WeaponData->Data.DamageType, Character, bHeadshot, Hit.ImpactPoint, ShotDirection);
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("Fire: %s hit %s for %.1f s%s"),
		*GetNameSafe(WeaponData),
		*GetNameSafe(HitActor),
		DamageToApply,
		bHeadshot ? TEXT(" (headshot)") : TEXT("")));

	Finish(false);
}
