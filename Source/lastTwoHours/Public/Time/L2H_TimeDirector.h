// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Item/L2H_ItemTypes.h"

#include "L2H_TimeDirector.generated.h"

class AL2H_BaseCharacter;
class AL2H_CheckpointTerminal;
class UAbilitySystemComponent;
class UGameplayEffect;
class UL2H_LevelTimeConfig;
class UL2H_TimeAttributeSet;
class ULocalPlayer;

/** Why a pawn's clock is stopped. Several can hold at once; the drain resumes when none do. */
UENUM(BlueprintType)
enum class EL2H_DrainPause : uint8
{
	/** The pawn is dead or out of time. */
	Down = 0,
	/** The pawn stands inside a checkpoint. */
	Checkpoint = 1,
};

/**
 * The level's time authority. One placed instance per level, next to a DA_L2H_LevelTimeConfig.
 *
 * It owns exactly four things:
 *	1. Registration -- every player pawn gets its starting time and the always-on drain effect.
 *	2. Mutation -- ApplyTimeDamage / ApplyTimeCost / ApplyTimeRestore are the ONLY entry points
 *	   that change time, and all three go through a GameplayEffect, never a raw attribute write.
 *	3. The zero crossing -- 0 seconds is Downed, not death, and rising back above 0 is recovery.
 *	4. Couch co-op -- the second local player is created here, behind the config flag.
 *
 * Every entry point is gated by HasAuthority(). A client holds a copy of this actor in a listen
 * server session, and that copy must do nothing.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Time Director"))
class LASTTWOHOURS_API AL2H_TimeDirector : public AActor
{
	GENERATED_BODY()

public:
	AL2H_TimeDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The level's tuning. Assign the level's DA_L2H_LevelTimeConfig on the placed instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time")
	TObjectPtr<UL2H_LevelTimeConfig> Config;

	/**
	 * The level's director, resolved from any world-context object.
	 *
	 * The abilities and the shards need to reach the one authority that may change time, and all they
	 * hold at that moment is an avatar. Same rule as the config lookup below: no director means null,
	 * and every caller reads that as "nothing to do" rather than as an error.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Time", meta = (WorldContext = "WorldContextObject"))
	static AL2H_TimeDirector* Get(const UObject* WorldContextObject);

	/**
	 * The level's tuning asset, resolved from the placed director.
	 *
	 * A static lookup rather than a reference handed out at registration, because the abilities need
	 * the thresholds at the moment of a press and a player can be possessed before any other system
	 * has had a chance to hand them anything. Null when the level has no director, which every caller
	 * reads as "use the shipped defaults" rather than as an error.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Time", meta = (WorldContext = "WorldContextObject"))
	static UL2H_LevelTimeConfig* GetLevelConfig(const UObject* WorldContextObject);

	// ---------------------------------------------------------------------------------------
	// Mutation. Server only.
	// ---------------------------------------------------------------------------------------

	/** Removes time. Hazards, falls and impacts all call this. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	void ApplyTimeDamage(AActor* Target, float Amount);

	/** Adds time. This is the transfer / recovery path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	void ApplyTimeRestore(AActor* Target, float Amount);

	/** Removes time as a deliberate payment, not as damage. Super abilities use this. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	void ApplyTimeCost(AActor* Target, float Amount);

	/**
	 * Brings Target to exactly TargetSeconds, by whichever of the two is needed.
	 *
	 * The only way to land on a number rather than to add or subtract one, which is what a checkpoint
	 * restore and the designer-time offer both need. Still a GameplayEffect underneath -- one restore
	 * when the target is above what the pawn has, one cost when it is below -- so the "time only ever
	 * changes through a GameplayEffect" rule survives the operation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	void ApplyTimeTo(AActor* Target, float TargetSeconds);

	// ---------------------------------------------------------------------------------------
	// Health mutation. Server only.
	//
	// This is not a second damage pathway: ApplyTimeDamage routes here whenever the target has a
	// health layer, so a hit still always ends up costing Time -- it just has to get through Health
	// to do it. The attack amount is health points, one for one, and the Time a hit is worth is
	// derived from the Health actually lost, which is what lets a level dial how expensive a mistake
	// is without retuning a gun.
	// ---------------------------------------------------------------------------------------

	/** Removes Health, and the Time the Health loss is worth. Hits, falls and impacts use this. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void ApplyHealthDamage(AActor* Target, float Amount);

	/** Adds Health. Healing and the revive path use this. Costs no Time. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void ApplyHealthRestore(AActor* Target, float Amount);

	/**
	 * Brings a dead pawn back with this much Health and this much Time.
	 *
	 * The only way out of death, and the reason the death flag is kept separate from Downed: a revive
	 * and a time transfer are different promises, and neither must be able to satisfy the other.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void RevivePawn(AL2H_BaseCharacter* Character, float HealthAmount, float TimeSeconds);

	/**
	 * The free revive: Reviver stood next to Target and held Interact inside Target's window.
	 * Restores FreeReviveHealthFraction of MaxHealth and leaves Time where it was. False (and a log
	 * line saying why) when Target is not dead, the window is over, or Reviver is too far or is down.
	 * Past the window, or for a partner down to the clock, the hold spends FindReviveToolCell instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	bool TryFreeRevive(AL2H_BaseCharacter* Reviver, AL2H_BaseCharacter* Target);

	/** The bag cell whose tool would stand Target up from Reviver's hands: injector first for a fall to damage, else elixir. INDEX_NONE when none. */
	static int32 FindReviveToolCell(const AL2H_BaseCharacter* Reviver, const AL2H_BaseCharacter* Target);

	// ---------------------------------------------------------------------------------------
	// Drain pause. The clock stops for a pawn while any reason holds: a body on the floor is not
	// spending anything, and a checkpoint is a rest. The drain effect is removed and re-applied rather
	// than zeroed, so a paused pawn has no periodic effect running at all.
	// ---------------------------------------------------------------------------------------

	/** Server only. Adds or clears one pause reason for Character; the drain runs only with none set. */
	void SetDrainPaused(AL2H_BaseCharacter* Character, EL2H_DrainPause Reason, bool bPaused);

	/**
	 * Server only. One packet of time from Giver to Receiver, at any distance: TransferAmount, plus
	 * TransferFirstTax when Giver has not given inside TransferFreeWindow. A receiver on the floor banks
	 * the packets until they reach RemoteReviveThreshold, then stands up with all of them. False (and a
	 * log line) when refused -- the giver cannot pay and keep a second, or nobody valid.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	bool TransferTime(AL2H_BaseCharacter* Giver, AL2H_BaseCharacter* Receiver);

	/** What Giver would pay for one packet right now (the gift plus the tax, if one is due). */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	float GetTransferCost(const AL2H_BaseCharacter* Giver) const;

	// ---------------------------------------------------------------------------------------
	// Shop and tools. Server only.
	// ---------------------------------------------------------------------------------------

	/** Buys ShopItems[Index] for Buyer from their bank, inside a hub. False (logged) when refused. */
	bool BuyShopItem(AL2H_BaseCharacter* Buyer, int32 Index);

	/**
	 * Applies a used item's effect for User. False when it cannot take effect (no partner in range for
	 * an elixir, a time stop already running), in which case the item is not spent.
	 */
	bool ApplyItemEffect(AL2H_BaseCharacter* User, const FL2H_ItemInstance& Item);

	/**
	 * Buyer stands in a hub and their partner is down anywhere: pays HubRevivePrice from Buyer's bank,
	 * brings the partner to Buyer's side and stands them up with at least HubReviveSeconds. The stage can
	 * then end as a pair. False (logged) when refused.
	 */
	bool BuyPartnerReviveAtHub(AL2H_BaseCharacter* Buyer);

	/** True while a time stop is running. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool IsTimeStopped() const { return bTimeStopped; }

	/**
	 * Server only. Spawns a shard beside Target. ApplyTimeDamage calls it once a hit is heavy enough (so
	 * "a big hit is recoverable" is a rule of the authority, not something each hazard remembers), and a
	 * dying enemy calls it to give back part of what the fight cost. TimeValue 0 uses ShardTimeValue.
	 */
	void SpillShardAt(AActor* Target, float TimeValue = 0.0f);

	// ---------------------------------------------------------------------------------------
	// Hubs. Server only, called by the hub's trigger.
	// ---------------------------------------------------------------------------------------

	/** Stops Character's clock and ends the stage if every player is now standing in this hub. */
	void NotifyHubEntered(AL2H_BaseCharacter* Character, AL2H_CheckpointTerminal* Hub);

	/** Starts Character's clock again and closes their carry/bank choice. */
	void NotifyHubExited(AL2H_BaseCharacter* Character, AL2H_CheckpointTerminal* Hub);

	/**
	 * Moves CarryStep seconds between Character's bank and clock, inside a hub whose stage just ended.
	 * +1 carries more onto the clock, -1 banks it back. Re-records the hub's checkpoint so a later
	 * rewind returns the choice as made. False when refused.
	 */
	bool AdjustHubCarry(AL2H_BaseCharacter* Character, int32 Direction);

	/** True while any pause reason is set for Character. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool IsDrainPaused(const AL2H_BaseCharacter* Character) const;

	// ---------------------------------------------------------------------------------------
	// Queries
	// ---------------------------------------------------------------------------------------

	/** Seconds of time left for anything carrying the TimeCore attribute set, or -1 if it has none. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	static float GetTimeFor(const AActor* Target);

	/** Health left for anything carrying the TimeCore attribute set, or -1 if it has no health layer. */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	static float GetHealthFor(const AActor* Target);

	/** True while this director's config says the level has a health layer at all. */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	bool HasHealthLayer() const;

	/**
	 * True while at least one registered pawn is neither downed nor dead -- someone who can still carry,
	 * revive or hand over time. The run rewinds only once this goes false.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Health")
	bool IsAnyPlayerStanding() const;

	/** Server only. Health back to the level's MaxHealth, as a base write. Checkpoint returns use it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Health")
	void ResetHealth(AL2H_BaseCharacter* Character);

	/** Server only. Health to exactly this value, as a base write (clamped to MaxHealth). A rewind uses it. */
	void SetHealthTo(AL2H_BaseCharacter* Character, float Value);

	/** True once at least one pawn is registered and every registered pawn is downed. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	bool AreAllPlayersDowned() const;

	/** How many player pawns this director has registered. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	int32 GetRegisteredPawnCount() const { return RegisteredPawns.Num(); }

	/**
	 * The registered player pawns. Shards pick a collector from here, transfers look for a downed
	 * partner here, and a checkpoint is written from here -- so all three see the same set the
	 * director drains, and none of them has to re-derive "who is a player".
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	TArray<AL2H_BaseCharacter*> GetRegisteredPawns() const;

	// ---------------------------------------------------------------------------------------
	// Time request. A transfer asked for by the player who needs the seconds, answered by the
	// partner. The transfer itself is unchanged -- this is the same gift with the initiative
	// reversed -- so it reaches the same two effects and pays the same tax.
	// ---------------------------------------------------------------------------------------

	/** The other registered pawn, or null in a single-player run. */
	UFUNCTION(BlueprintPure, Category = "L2H|Time")
	AL2H_BaseCharacter* FindPartnerOf(const AL2H_BaseCharacter* Requester) const;

	/**
	 * Server only. Posts a pending request on Requester's partner and arms the lapse timer.
	 * False when there is no partner, or the same requester asked again inside RequestCooldown.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	bool PostTimeRequest(AL2H_BaseCharacter* Requester);

	/**
	 * Server only. The partner's answer. Only the pawn the request was posted on may resolve it,
	 * so nobody can answer a question that was not put to them.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	bool ResolveTimeRequest(AL2H_BaseCharacter* Responder, bool bAccept);

	/** Fired when a request is posted or resolved, for logging and level-side polish. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Time")
	void OnTimeRequestChanged(AL2H_BaseCharacter* Requester, AL2H_BaseCharacter* Responder, bool bAccepted);

	// ---------------------------------------------------------------------------------------
	// Run control
	// ---------------------------------------------------------------------------------------

	/** Returns every registered pawn to StartingTime and clears Downed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Time")
	void ResetRun();

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Time")
	void OnPlayerDowned(AL2H_BaseCharacter* Character);

	/** Fired the moment Health reaches 0, before the respawn delay starts running. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Health")
	void OnPlayerDied(AL2H_BaseCharacter* Character);

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Time")
	void OnPlayerRevived(AL2H_BaseCharacter* Character);

	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Time")
	void OnRunReset();

protected:
	/** Repeating registration scan, so a pawn that did not exist at BeginPlay is still picked up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "0.1"))
	float PlayerScanInterval = 0.5f;

	/**
	 * Seconds between "every player is downed" and the restart. Deferred on purpose: the check runs
	 * inside the attribute-change broadcast, and rewriting the attribute from there is re-entrant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "0.0"))
	float AllDownedResetDelay = 2.0f;

	/**
	 * Seconds between a death and the respawn. Deferred for the same reason the all-downed rewind is:
	 * this runs inside an attribute-change broadcast, and reloading the world from there is re-entrant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float DeathRespawnDelay = 2.0f;

	/** Removes State.Downed only if it is actually present, so the loose count never goes negative. */
	static void ClearDownedTag(UAbilitySystemComponent* ASC);

	void ScanForPlayers();
	void RegisterPawn(AL2H_BaseCharacter* Character);
	void ApplyPassiveDrain(AL2H_BaseCharacter* Character);

	void HandleTimeChangedFor(AL2H_BaseCharacter* Character, float NewValue);
	void CreateSecondLocalPlayer();

	/** Logs which physical device each local player ended up on, so couch co-op is verifiable. */
	void LogLocalPlayerBindings() const;

	void ResetPawn(AL2H_BaseCharacter* Character);
	void CheckAllDowned();

	/**
	 * What "every player is downed" actually does: rewinds to the run's checkpoint, or falls back to
	 * a full restart when the run never reached one. Deferred off the attribute broadcast for the
	 * same reason ResetRun is -- rewinding from inside the change that caused it is re-entrant.
	 */
	void ReturnToCheckpointOrReset();

	/** Shared body of ApplyTimeDamage / ApplyTimeRestore / ApplyTimeCost. */
	void ApplyTimeEffect(AActor* Target, float Magnitude, TSubclassOf<UGameplayEffect> EffectClass, const FName& MagnitudeName);

	/**
	 * The one place a hit actually becomes seconds: the Time damage effect plus the shard spill.
	 *
	 * Both ApplyTimeDamage (a target with no health layer) and ApplyHealthDamage (the Time a health
	 * loss is worth) end here, so there is exactly one spill rule and no way for a caller to reach the
	 * clock while skipping it. Invulnerability is checked once, by ApplyTimeDamage, before either
	 * branch -- re-checking here would be a second rule that could disagree.
	 */
	void ApplyTimedDamageInner(AActor* Target, float Amount);

	/** Health reaching 0. Marks the pawn dead and arms the respawn. */
	void HandleDeath(AL2H_BaseCharacter* Character);

	/** Clears the death timer and then rewinds, so the two paths cannot stack. */
	void RespawnAfterDeath();

	/** Interface first, then a component search, so a non-TimeCore actor can still be hit. */
	static UAbilitySystemComponent* ResolveAbilitySystem(AActor* Actor);

	static const FGameplayTag& GetDownedTag();

	/** SetByCaller data name on UL2H_GE_TimeDrain. */
	static FName GetDrainMagnitudeName();

	/** SetByCaller data name on the cost / damage / restore effects. */
	static FName GetAmountMagnitudeName();

	/** SetByCaller data name on UL2H_GE_HealthDamage. */
	static FName GetHealthDamageMagnitudeName();

	/** SetByCaller data name on UL2H_GE_HealthRestore. */
	static FName GetHealthRestoreMagnitudeName();

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AL2H_BaseCharacter>> RegisteredPawns;

	/** Per pawn: the live drain effect (invalid while paused) and the set of pause reasons, as bits. */
	struct FDrainState
	{
		FActiveGameplayEffectHandle Handle;
		uint8 PauseMask = 0;
	};
	TMap<TWeakObjectPtr<AL2H_BaseCharacter>, FDrainState> DrainStates;

	FTimerHandle PlayerScanTimer;
	FTimerHandle SecondPlayerTimer;
	FTimerHandle ResetRunTimer;
	FTimerHandle DeathRespawnTimer;
	FTimerHandle RequestLapseTimer;

	/** The requester of the live request, so ExpireTimeRequest knows whose pawn to clear. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AL2H_BaseCharacter> PendingRequester;

	/**
	 * Who asked last, and when. A pair rather than a map: a couch co-op run has two players, so this
	 * is enough to enforce RequestCooldown honestly without a container that only ever holds two rows.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AL2H_BaseCharacter> LastRequester;

	float LastRequestPostTime = -1000.0f;

	bool bTimeStopped = false;
	FTimerHandle TimeStopTimer;
	TWeakObjectPtr<AL2H_BaseCharacter> TimeStopCaster;
	TArray<TWeakObjectPtr<AActor>> TimeStopExempt;
	void EndTimeStop();

	/** The testing aid behind Config->bSpawnTestStations. */
	void SpawnTestStations();

	/** The couch second player this director made, so it can step aside when a networked player joins. */
	TWeakObjectPtr<ULocalPlayer> CreatedLocalPlayer;

	/** True while this director has switched on "skip assigning gamepad to player 1"; EndPlay switches it back. */
	bool bOffsetGamepadsForCouch = false;

	/**
	 * Re-decides "skip assigning gamepad to player 1" from the pads that are connected right now.
	 *
	 * The offset is the engine's single-pad mode: UGameViewportClient::RemapControllerInput rewrites
	 * every gamepad event to the device of local player ControllerId + 1. That is correct while one pad
	 * has to serve player 2, and wrong the moment a second pad exists -- then both pads drive the second
	 * player and the first player is left with the keyboard. It is therefore re-decided while the run
	 * goes, rather than latched once at startup when the pads may not all have been reported yet.
	 */
	void UpdateGamepadOffsetForCouch();

	/** Banks every player's leftover, sets the next stage's clock and records the hub. */
	void CompleteSegment(AL2H_CheckpointTerminal* Hub);

	/** Lapses the live request when nobody answered in time. */
	void ExpireTimeRequest();

	/** Set once the startup checkpoint has been offered, so it is applied at most once per session. */
	bool bStartupCheckpointHandled = false;
};
