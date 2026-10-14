// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "L2H_WeaponComponent.generated.h"

class AL2H_WeaponActor;
class UL2H_WeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FL2HOnWeaponChanged);

/**
 * What the pawn is holding, how much is left in it, and whether it is up.
 *
 * The loadout is a list of data assets and this component spawns one actor per entry, rather than the
 * pawn carrying a pre-placed weapon mesh. That is what makes \"switch weapons\" a change of index instead
 * of a change of attachment, and it is why a weapon can be added to a character by adding a row.
 *
 * Everything that changes state is server-only. A client asks; the server decides; the replicated
 * result is what every screen draws.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Weapon"))
class LASTTWOHOURS_API UL2H_WeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_WeaponComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fired on every machine whenever the weapon, its clip or the aim state changes. */
	UPROPERTY(BlueprintAssignable, Category = "L2H|Weapon")
	FL2HOnWeaponChanged OnWeaponChanged;

	// ---------------------------------------------------------------------------------------------
	// Queries
	// ---------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	AL2H_WeaponActor* GetActiveWeapon() const { return ActiveWeapon; }

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	UL2H_WeaponDataAsset* GetActiveWeaponData() const;

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	int32 GetActiveIndex() const { return ActiveIndex; }

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	int32 GetLoadoutSize() const { return Loadout.Num(); }

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	int32 GetAmmoInClip() const { return AmmoInClip; }

	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	bool IsAiming() const { return bAiming; }

	/**
	 * Seconds a reload of the active weapon would cost right now (rounds missing x SecondsPerRound,
	 * scaled inside a hub). 0 when full, magazine-less or free. For the HUD's "reload costs N s".
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	float GetReloadCost() const;

	/** True when something is held and it has a round to spend. The fire ability's single gate. */
	UFUNCTION(BlueprintPure, Category = "L2H|Weapon")
	bool CanFire() const;

	// ---------------------------------------------------------------------------------------------
	// Mutations. All server-only.
	// ---------------------------------------------------------------------------------------------

	/** Selects a loadout entry, spawning its actor the first time it is reached. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	bool ServerSelectWeapon(int32 Index);

	/** Steps to the next entry that can be selected, wrapping at the end. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	bool ServerCycleWeapon();

	/** Spends one round. True when a round was spent or the weapon needs none. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	bool ServerConsumeRound();

	/** Refills the active weapon's magazine. False when it has none, or is already full. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	bool ServerRefillClip();

	/** Server only. A refill nobody pays for (the prepaid magazine). False when there is nothing to refill. */
	bool RefillClipFree() { return RefillClip(true); }

	/** Sets the replicated aim state. The speed and FOV halves are applied by the aim ability. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	void ServerSetAiming(bool bNewAiming);

	/**
	 * Server only. Plays the authored feedback for a shot.
	 *
	 * Two halves, because they live on two different skeletons: the weapon's own animation belongs to the
	 * weapon mesh's skeleton and the holder's montage to the character's. A rigged weapon cannot drive the
	 * character's arms, and the character's montage cannot play on a weapon that shares no skeleton with it,
	 * which is why the authored data carries two fields per action rather than one.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	void PlayFireAnimation();

	/** Server only. The same two halves, for the reload. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Weapon")
	void PlayReloadAnimation();

protected:
	bool RefillClip(bool bFree);

	/**
	 * Every machine runs this, and each draws its own copy. An animation is local presentation, so a server
	 * that played it alone would leave every client's weapon and arms still.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayWeaponAnim(bool bReload);

	UFUNCTION()
	void OnRep_ActiveWeapon();

	UFUNCTION()
	void OnRep_AmmoInClip();

	UFUNCTION()
	void OnRep_Aiming();

	/** The spawned actor for a loadout entry, creating it on the authority if it does not exist yet. */
	AL2H_WeaponActor* ResolveWeaponActor(int32 Index);

	/** The authored loadout, in cycle order. Entry 0 is what the pawn starts holding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Weapon")
	TArray<TObjectPtr<UL2H_WeaponDataAsset>> Loadout;

	/** What the component spawns per entry. Authored so no Blueprint is named in C++. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Weapon")
	TSubclassOf<AL2H_WeaponActor> WeaponActorClass;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon, BlueprintReadOnly, Category = "L2H|Weapon")
	TObjectPtr<AL2H_WeaponActor> ActiveWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Weapon")
	int32 ActiveIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_AmmoInClip, BlueprintReadOnly, Category = "L2H|Weapon")
	int32 AmmoInClip = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Aiming, BlueprintReadOnly, Category = "L2H|Weapon")
	bool bAiming = false;

	/** Server-side only: the actor spawned for each loadout entry, indexed in step with Loadout. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AL2H_WeaponActor>> SpawnedWeapons;

	/**
	 * Server-side only: rounds left in each loadout entry's magazine, -1 for an entry never held. A
	 * switch used to refill the magazine for nothing, which made the reload price avoidable.
	 */
	TArray<int32> StoredClips;

	/** Price per round for the active weapon after the hub scale, and the rounds missing. */
	float GetRoundPrice() const;
};
