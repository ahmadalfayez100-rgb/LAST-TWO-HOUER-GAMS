// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/L2H_WeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Character/L2H_BaseCharacter.h"
#include "Item/L2H_InventoryComponent.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"
#include "World/L2H_CheckpointTerminal.h"
#include "Weapon/L2H_WeaponActor.h"
#include "Weapon/L2H_WeaponDataAsset.h"

UL2H_WeaponComponent::UL2H_WeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Replicated state, so the component opts in rather than relying on a Blueprint checkbox -- a pawn that
	// forgot it would show a rifle on the host and empty hands on a client.
	SetIsReplicatedByDefault(true);
}

void UL2H_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// Chosen on the authority so the index and the spawned actor agree from the first frame, instead of
	// every client re-deriving \"which weapon is this\" from a different starting guess.
	if (GetOwner() && GetOwner()->HasAuthority() && Loadout.Num() > 0)
	{
		ServerSelectWeapon(0);
	}
}

void UL2H_WeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UL2H_WeaponComponent, ActiveWeapon);
	DOREPLIFETIME(UL2H_WeaponComponent, ActiveIndex);
	DOREPLIFETIME(UL2H_WeaponComponent, AmmoInClip);
	DOREPLIFETIME(UL2H_WeaponComponent, bAiming);
	DOREPLIFETIME(UL2H_WeaponComponent, Loadout);
}

UL2H_WeaponDataAsset* UL2H_WeaponComponent::GetActiveWeaponData() const
{
	if (ActiveWeapon && ActiveWeapon->GetWeaponData())
	{
		return ActiveWeapon->GetWeaponData();
	}

	// The index is the fallback because the actor reference and its data arrive on separate properties,
	// and a query is allowed to answer correctly in the frames before either one lands.
	return Loadout.IsValidIndex(ActiveIndex) ? Loadout[ActiveIndex] : nullptr;
}

bool UL2H_WeaponComponent::CanFire() const
{
	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();

	if (!ActiveWeapon || !Data)
	{
		return false;
	}

	// ClipSize 0 is the pipe: a weapon with no magazine is never out of ammunition.
	return Data->Data.ClipSize <= 0 || AmmoInClip > 0;
}

AL2H_WeaponActor* UL2H_WeaponComponent::ResolveWeaponActor(int32 Index)
{
	if (!Loadout.IsValidIndex(Index) || !Loadout[Index])
	{
		return nullptr;
	}

	if (SpawnedWeapons.IsValidIndex(Index) && SpawnedWeapons[Index])
	{
		return SpawnedWeapons[Index];
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World || !Owner->HasAuthority())
	{
		// Clients never spawn. They receive the actor as a replicated actor, which is why this is a
		// quiet case and not a refusal worth logging.
		return nullptr;
	}

	if (!WeaponActorClass)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Weapon %s: no WeaponActorClass set, so %s cannot be spawned"),
			*GetName(),
			*GetNameSafe(Loadout[Index])));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AL2H_WeaponActor* Spawned = World->SpawnActor<AL2H_WeaponActor>(
		WeaponActorClass,
		Owner->GetActorTransform(),
		SpawnParams);

	if (!Spawned)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Weapon %s: could not spawn %s"),
			*GetName(),
			*GetNameSafe(WeaponActorClass)));
		return nullptr;
	}

	// Owner first, then the data. The actor attaches once with the owner and again when the data lands,
	// which is the same order-independence the client relies on.
	Spawned->SetWeaponOwner(Owner);
	Spawned->SetWeaponData(Loadout[Index]);
	Spawned->SetEquipped(false);

	// Grown to the loadout's size and then only written by index, so a loadout re-ordered in the editor
	// cannot shift an already-spawned actor onto a different entry at runtime.
	SpawnedWeapons.SetNum(FMath::Max(Loadout.Num(), Index + 1));
	SpawnedWeapons[Index] = Spawned;

	return Spawned;
}

bool UL2H_WeaponComponent::ServerSelectWeapon(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Loadout.IsValidIndex(Index))
	{
		return false;
	}

	AL2H_WeaponActor* Selected = ResolveWeaponActor(Index);
	if (!Selected)
	{
		return false;
	}

	// The one going away is holstered before the new one comes up, so a switch reads as a swap rather
	// than as two weapons occupying the same hand for a frame.
	if (ActiveWeapon && ActiveWeapon != Selected)
	{
		ActiveWeapon->SetEquipped(false);
	}

	// Each magazine keeps its own count across switches; a weapon held for the first time comes loaded.
	while (StoredClips.Num() < Loadout.Num())
	{
		StoredClips.Add(-1);
	}
	if (StoredClips.IsValidIndex(ActiveIndex) && ActiveIndex != Index)
	{
		StoredClips[ActiveIndex] = AmmoInClip;
	}

	ActiveIndex = Index;
	ActiveWeapon = Selected;
	Selected->SetEquipped(true);

	const int32 FullClip = Loadout[Index] ? Loadout[Index]->Data.ClipSize : 0;
	AmmoInClip = StoredClips[Index] >= 0 ? StoredClips[Index] : FullClip;
	StoredClips[Index] = AmmoInClip;

	// Called directly as well as left for the OnRep: an OnRep does not fire for a locally-set value, so
	// on a listen server the authority's own UI would otherwise never hear that the weapon changed.
	OnRep_ActiveWeapon();
	OnRep_AmmoInClip();

	L2HTime::LogImportant(FString::Printf(
		TEXT("Weapon %s: equipped %s (clip %d)"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Loadout[Index]),
		AmmoInClip));

	return true;
}

bool UL2H_WeaponComponent::ServerCycleWeapon()
{
	if (Loadout.Num() == 0)
	{
		return false;
	}

	const int32 From = (ActiveIndex == INDEX_NONE) ? -1 : ActiveIndex;

	// Walks the whole loadout rather than stopping at the first empty row, so one un-authored entry does
	// not make every weapon after it unreachable.
	for (int32 Step = 1; Step <= Loadout.Num(); ++Step)
	{
		const int32 Candidate = (From + Step + Loadout.Num()) % Loadout.Num();

		if (ServerSelectWeapon(Candidate))
		{
			return true;
		}
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Weapon %s: nothing in the loadout could be equipped"), *GetName()));
	return false;
}

bool UL2H_WeaponComponent::ServerConsumeRound()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();
	if (!Data)
	{
		return false;
	}

	if (Data->Data.ClipSize <= 0)
	{
		// No magazine to spend from: the shot is free and the caller should proceed.
		return true;
	}

	if (AmmoInClip <= 0)
	{
		return false;
	}

	--AmmoInClip;
	OnRep_AmmoInClip();
	return true;
}

bool UL2H_WeaponComponent::ServerRefillClip()
{
	return RefillClip(false);
}

bool UL2H_WeaponComponent::RefillClip(bool bFree)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();

	if (!Data || Data->Data.ClipSize <= 0)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Reload refused: %s has no magazine"), *GetNameSafe(Data)));
		return false;
	}

	if (AmmoInClip >= Data->Data.ClipSize)
	{
		return false;
	}

	// Rounds are bought with the clock. Never so many that the purchase itself would end the player: a
	// reload buys what the clock can pay for while leaving at least a second, and nothing when it cannot.
	int32 Rounds = Data->Data.ClipSize - AmmoInClip;
	const float Price = bFree ? 0.0f : GetRoundPrice();

	// A prepaid magazine in the bag is spent before the clock. It comes back through the bag's use path
	// (which calls RefillClipFree), so the cell is emptied the same way any tool's is.
	if (Price > 0.0f)
	{
		if (UL2H_InventoryComponent* Bag = GetOwner()->FindComponentByClass<UL2H_InventoryComponent>())
		{
			const int32 Cell = Bag->FindToolCell(EL2H_ItemUseEffect::PrepaidMagazine);
			if (Cell != INDEX_NONE)
			{
				const int32 Before = AmmoInClip;
				Bag->UseOnServer(Cell);
				if (AmmoInClip > Before)
				{
					return true;
				}
			}
		}
	}
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (Price > 0.0f && Director)
	{
		const float Clock = AL2H_TimeDirector::GetTimeFor(GetOwner());
		if (Clock >= 0.0f)
		{
			Rounds = FMath::Min(Rounds, FMath::FloorToInt(FMath::Max(Clock - 1.0f, 0.0f) / Price));
		}
		if (Rounds <= 0)
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Reload refused: %s cannot afford a round of %s (%.1fs each)"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Data), Price));
			return false;
		}
		Director->ApplyTimeCost(GetOwner(), Rounds * Price);
	}

	AmmoInClip += Rounds;
	if (StoredClips.IsValidIndex(ActiveIndex))
	{
		StoredClips[ActiveIndex] = AmmoInClip;
	}
	OnRep_AmmoInClip();

	L2HTime::LogImportant(FString::Printf(TEXT("Weapon %s: reloaded %s to %d (%d rounds, %.1fs)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Data), AmmoInClip, Rounds, Rounds * Price));
	return true;
}

float UL2H_WeaponComponent::GetRoundPrice() const
{
	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();
	if (!Data)
	{
		return 0.0f;
	}

	float Price = FMath::Max(Data->Data.SecondsPerRound, 0.0f);

	// A hub is the rest stop: reloading there is cheap (free by default), which is one more reason to plan
	// the next stage while standing in one.
	const AL2H_BaseCharacter* Owner = Cast<AL2H_BaseCharacter>(GetOwner());
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (Owner && Config && Owner->GetCurrentTerminal() && Owner->GetCurrentTerminal()->SavesOnArrival())
	{
		Price *= Config->HubReloadCostScale;
	}
	return Price;
}

float UL2H_WeaponComponent::GetReloadCost() const
{
	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();
	if (!Data || Data->Data.ClipSize <= 0)
	{
		return 0.0f;
	}
	return FMath::Max(Data->Data.ClipSize - AmmoInClip, 0) * GetRoundPrice();
}

void UL2H_WeaponComponent::ServerSetAiming(bool bNewAiming)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bAiming == bNewAiming)
	{
		return;
	}

	bAiming = bNewAiming;
	OnRep_Aiming();

	L2HTime::LogVerbose(FString::Printf(
		TEXT("Weapon %s: %s"),
		*GetNameSafe(GetOwner()),
		bAiming ? TEXT("aiming") : TEXT("lowered")));
}

void UL2H_WeaponComponent::PlayFireAnimation()
{
	MulticastPlayWeaponAnim(/*bReload=*/false);
}

void UL2H_WeaponComponent::PlayReloadAnimation()
{
	MulticastPlayWeaponAnim(/*bReload=*/true);
}

void UL2H_WeaponComponent::MulticastPlayWeaponAnim_Implementation(bool bReload)
{
	const UL2H_WeaponDataAsset* Data = GetActiveWeaponData();

	if (!Data)
	{
		return;
	}

	const UAnimationAsset* WeaponAnim =
		bReload ? Data->Data.Animations.WeaponReload.Get() : Data->Data.Animations.WeaponFire.Get();

	// The weapon's own animation, on the weapon's own skeleton. A single-node play rather than a montage:
	// a weapon has one moving part and nothing to blend against, so a slot would only add a setup step.
	if (WeaponAnim)
	{
		if (AL2H_WeaponActor* Weapon = GetActiveWeapon())
		{
			if (USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh())
			{
				WeaponMesh->PlayAnimation(const_cast<UAnimationAsset*>(WeaponAnim), /*bLooping=*/false);
			}
		}
	}

	// The holder's montage, on the holder's skeleton, from here rather than from the ability: the fire and
	// reload abilities are ServerOnly, so a montage they played would exist on the host and nowhere else.
	UAnimMontage* Montage =
		bReload ? Data->Data.Animations.CharacterReload.Get() : Data->Data.Animations.CharacterFire.Get();

	if (!Montage)
	{
		return;
	}

	if (const ACharacter* Holder = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Body = Holder->GetMesh())
		{
			if (UAnimInstance* Anim = Body->GetAnimInstance())
			{
				Anim->Montage_Play(Montage);
			}
		}
	}
}

void UL2H_WeaponComponent::OnRep_ActiveWeapon()
{
	// Attachment is not replicated, so the machine that received the new reference is the machine that has
	// to point the actor at its owner and let it attach itself.
	if (ActiveWeapon)
	{
		ActiveWeapon->SetWeaponOwner(GetOwner());
	}

	OnWeaponChanged.Broadcast();
}

void UL2H_WeaponComponent::OnRep_AmmoInClip()
{
	OnWeaponChanged.Broadcast();
}

void UL2H_WeaponComponent::OnRep_Aiming()
{
	OnWeaponChanged.Broadcast();
}

void UL2H_WeaponComponent::ServerAddWeapon(UL2H_WeaponDataAsset* Data)
{
	if (!Data || !GetOwner() || !GetOwner()->HasAuthority() || Loadout.Contains(Data))
	{
		return;
	}
	Loadout.Add(Data);

	// A weapon whose shot is its own ability (the pipe, the hook) gets it now: granted later, from inside the
	// fire ability, the grant is deferred by the ability list lock and the first press is refused.
	if (const TSubclassOf<UGameplayAbility> Override = Data->Data.FireAbilityOverride)
	{
		const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner());
		if (UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr)
		{
			if (!ASC->FindAbilitySpecFromClass(Override))
			{
				ASC->GiveAbility(FGameplayAbilitySpec(Override, 1));
			}
		}
	}
	L2HTime::LogImportant(FString::Printf(TEXT("Weapon %s: picked up %s (loadout %d)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Data), Loadout.Num()));
}
