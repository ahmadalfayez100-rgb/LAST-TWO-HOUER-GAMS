// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "L2H_LevelTimeConfig.generated.h"

class AL2H_TimeShard;
class UL2H_ItemDataAsset;
class UGameplayEffect;
class USoundBase;

/**
 * Per-level tuning for the TimeCore loop.
 *
 * One asset per level, referenced by that level's AL2H_TimeDirector. Nothing here is written in
 * C++: the director only ever reads these values, so a level can be retuned without a rebuild.
 *
 * The four GameplayEffect slots are the only way time is mutated. Every one of them is a
 * SetByCaller effect, so a single asset per duration policy covers every cost, every hit and every
 * transfer in the level, and the magnitude stays a gameplay-side number.
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Level Time Config"))
class LASTTWOHOURS_API UL2H_LevelTimeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UL2H_LevelTimeConfig();

	/** Seconds every player starts the level with. Also the value a run resets to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "1.0"))
	float StartingTime = 120.0f;

	/** Ceiling the Time attribute is clamped to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "1.0"))
	float MaxTime = 120.0f;

	/**
	 * The time the designer knows the level can be finished in.
	 *
	 * Offered in the terminal menu to a player who has run short, as a replacement for their own time.
	 * Deliberately one fixed design number rather than a per-player estimate: the offer only works if
	 * the player is told what the level actually costs and then chooses, which requires the number to
	 * be the same every time they look at it. The exchange is optional and the player keeps their own
	 * time by doing nothing. 0 hides the offer entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "0.0"))
	float DesignerTime = 0.0f;

	/** Always-on drain, in seconds per second. 1.0 means one second of life per real second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Time", meta = (ClampMin = "0.0"))
	float TimeDrainRate = 1.0f;

	/**
	 * Extra seconds per second the carrier burns while moving a downed partner. Carrying has to
	 * cost something, otherwise there is no pressure to spend time on a transfer instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0"))
	float CarryPartnerDrainRate = 0.5f;

	/** Shards a single player may hold at once. The design is one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0"))
	int32 MaxCarriedShards = 1;

	/** Spawns a second local player at level start, for split-screen couch co-op. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Co-op")
	bool bCreateSecondLocalPlayer = true;

	/** Seconds after level start at which the second local player joins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Co-op", meta = (ClampMin = "0.0"))
	float SecondLocalPlayerDelay = 0.25f;

	// -------------------------------------------------------------------------------------------
	// Hook and carry. Each object carries its own mass (UL2H_MassProfileComponent); the level
	// carries the thresholds. That split is what lets "what the hook can drag" be retuned for a
	// level without touching a single prop, and it keeps the abilities free of magic numbers.
	// -------------------------------------------------------------------------------------------

	/**
	 * Mass at or below which the hook drags the OBJECT to the player. Above it the object is too
	 * heavy to move, so the player travels to it instead -- which is the same button climbing a wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Hook", meta = (ClampMin = "0.0"))
	float HookPullCapacity = 40.0f;

	/** Mass one player can carry alone. Above it the load needs the second player standing nearby. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0"))
	float SingleCarryMass = 60.0f;

	/** Mass above which nothing moves the object at all, however many players are helping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0"))
	float TwoPlayerCarryMass = 120.0f;

	/** How close the second player must be to help with a two-player load, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry", meta = (ClampMin = "0.0"))
	float TwoPlayerAssistRadius = 250.0f;

	// -------------------------------------------------------------------------------------------
	// Shards. A single hit large enough to hurt drops the lost seconds back into the world, so a big
	// loss is an invitation to go and get them rather than a permanent subtraction.
	// -------------------------------------------------------------------------------------------

	/** Actor spawned when one hit takes ShardThreshold or more. Empty disables the spill entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards")
	TSubclassOf<AL2H_TimeShard> ShardClass;

	/** Size of a single hit, in seconds, that spills a shard. 0 disables the spill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardThreshold = 15.0f;

	/** Seconds a shard hands back when it is absorbed. How much of a big hit is recoverable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "1.0"))
	float ShardTimeValue = 10.0f;

	/** Seconds a shard survives before it fades. 0 means it never expires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardLifetime = 30.0f;

	/** Seconds a pawn is busy absorbing one shard. With MaxCarriedShards = 1 this is the whole cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardCarryDuration = 1.0f;

	/** How far a shard starts drifting toward a pawn with room for it, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardMagnetRange = 350.0f;

	/** Magnet travel speed in cm/s. 0 leaves the shard where it landed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardMagnetSpeed = 600.0f;

	/** Radius the spill is scattered over, so a shard does not land inside the victim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0.0"))
	float ShardSpawnRadius = 70.0f;

	// -------------------------------------------------------------------------------------------
	// Transfer, at any distance. A fixed packet of seconds; the first packet of a burst pays a flat
	// tax, and every further packet inside the free window after it is untaxed -- so topping a partner
	// up by a lot costs one tax, not one per press. A partner on the floor is stood up only once the
	// packets add up to RemoteReviveThreshold, which keeps the far-away revive dearer than the near
	// ones (elixir, carry to a station).
	// -------------------------------------------------------------------------------------------

	/** Seconds the partner receives per transfer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Transfer", meta = (ClampMin = "1.0"))
	float TransferAmount = 15.0f;

	/** Seconds the giver pays on top of the first packet of a burst. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Transfer", meta = (ClampMin = "0.0"))
	float TransferFirstTax = 5.0f;

	/** Seconds after a transfer during which the next one by the same giver is untaxed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Transfer", meta = (ClampMin = "0.0"))
	float TransferFreeWindow = 20.0f;

	/** Seconds a downed or dead partner has to receive before the transfers stand them up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Transfer", meta = (ClampMin = "0.0"))
	float RemoteReviveThreshold = 30.0f;

	// -------------------------------------------------------------------------------------------
	// Request. The same gift a transfer is, asked for by the player who needs the seconds. The
	// economics are deliberately identical -- the transfer rule above applies as it is -- so
	// the only new rules are how long the question stays open and how often it may be asked.
	// -------------------------------------------------------------------------------------------

	/** Seconds the partner has to answer before the request lapses unanswered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Request", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 12.0f;

	/** Minimum seconds between two requests from the same player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Request", meta = (ClampMin = "0.0"))
	float RequestCooldown = 5.0f;

	// -------------------------------------------------------------------------------------------
	// Save
	// -------------------------------------------------------------------------------------------

	/** Seconds the payer spends to write a checkpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Save", meta = (ClampMin = "0.0"))
	float SaveCostSeconds = 30.0f;

	/** Slot the checkpoint is written to. Empty falls back to L2H_Time_Checkpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Save")
	FString CheckpointSlotName = TEXT("L2H_Time_Checkpoint");

	/** Applies the stored checkpoint to the players registered right after level start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Save")
	bool bRestoreCheckpointOnStart = true;

	// -------------------------------------------------------------------------------------------
	// Effects
	// -------------------------------------------------------------------------------------------

	/** Infinite, Period 1.0, SetByCaller on Data.Time.Drain. Applied once per pawn at registration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> TimeDrainEffect;

	/** Instant, SetByCaller on Data.Time.Amount. Super abilities pay with this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> TimeCostEffect;

	/** Instant, SetByCaller on Data.Time.Amount. Every hazard routes through this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> TimeDamageEffect;

	/** Instant, SetByCaller on Data.Time.Amount. The transfer/recovery path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> TimeRestoreEffect;

	// -------------------------------------------------------------------------------------------
	// Health. The cushion in front of the clock. A hit takes Health, and every point of Health
	// actually lost ALSO takes TimePerHealthPoint seconds -- so surviving a mistake still costs the
	// run something, and a mistake that is survived is never free. Health running out is the one way
	// a run ends while the clock is still up.
	//
	// Attack amounts are health points, one for one. That keeps a level able to say "this gun is 20"
	// and separately say "a point of health is worth half a second" without either number being
	// baked into the other.
	// -------------------------------------------------------------------------------------------

	/**
	 * Ceiling Health, and what a revive and a run reset restore to. 0 disables the health layer
	 * entirely, which leaves every hit on the old pure-time path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	/** Seconds of Time lost per point of Health lost. 0 makes losing Health cost no seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float TimePerHealthPoint = 0.5f;

	/** Health returned per second once regeneration has started. 0 disables regeneration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float HealthRegenRate = 2.0f;

	/** Seconds without taking a hit before regeneration resumes. Every hit restarts this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float HealthRegenDelay = 4.0f;

	// -------------------------------------------------------------------------------------------
	// Free revive. A partner who falls to Health gets a short window in which the other player can
	// stand them up for nothing, by holding Interact next to the body. It gives Health back, never
	// Time: a free revive that also handed out seconds let a player run their clock to zero on purpose
	// and be stood up with a fresh minute, over and over. A body whose clock ran out needs real time
	// (a transfer, an elixir, a revive station).
	// -------------------------------------------------------------------------------------------

	/** Seconds after a Health death during which the free revive is offered. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "0.0"))
	float FreeReviveWindow = 15.0f;

	/** How close (cm, centre to centre) the reviver has to stand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "50.0"))
	float FreeReviveRange = 220.0f;

	/** Seconds Interact has to be held. A shorter press still picks the body up, as before. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "0.1"))
	float FreeReviveHoldTime = 1.5f;

	// -------------------------------------------------------------------------------------------
	// Segment end. When every player stands in a hub the stage is complete: each player's leftover
	// seconds go to their bank (Chrons, the shop currency) and their clock is set to the hub's
	// NextSegmentTime. While still inside, a player may move some of that leftover back onto the
	// clock for the next stage, in CarryStep steps, up to MaxCarryOver -- safety now or tools later.
	// -------------------------------------------------------------------------------------------

	/** Most leftover seconds a player may carry into the next stage instead of banking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint", meta = (ClampMin = "0.0"))
	float MaxCarryOver = 60.0f;

	/** Seconds moved per press of "carry more" / "bank back". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint", meta = (ClampMin = "1.0"))
	float CarryStep = 15.0f;

	/**
	 * Testing aid: a level with no hub of its own gets a hub, a save spot and a revive station spawned in
	 * a row behind the first PlayerStart, each with its coloured marker. Nothing is saved into the
	 * level; a level that places its own hub is left alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Testing")
	bool bSpawnTestStations = false;

	/** What a hub's shop sells, paid from the bank at each item's Price (banked seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shop")
	TArray<TObjectPtr<UL2H_ItemDataAsset>> ShopItems;

	/**
	 * A player standing in a hub may bring a partner who is down elsewhere straight to the hub, paid from
	 * their bank. Dearer than the field revives, because it also skips the rest of the stage's route.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shop", meta = (ClampMin = "0.0"))
	float HubRevivePrice = 60.0f;

	/** Seconds the partner brought to a hub stands up with (at least). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shop", meta = (ClampMin = "1.0"))
	float HubReviveSeconds = 60.0f;

	/** World speed while a time stop runs. Players are compensated back to normal speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeStopDilation = 0.03f;

	/** Price multiplier on a reload made inside a hub. 0 = free there, 1 = full price. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Checkpoint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HubReloadCostScale = 0.0f;

	/** Share of MaxHealth the free revive restores. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float FreeReviveHealthFraction = 0.5f;

	/** Landing speed (cm/s, downward) below which a fall is free. ~1100 is a drop of about 6 m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float FallDamageMinSpeed = 1100.0f;

	/** Landing speed at which a fall takes all of MaxHealth. Damage scales linearly in between. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Health", meta = (ClampMin = "0.0"))
	float FallDamageLethalSpeed = 2300.0f;

	// -------------------------------------------------------------------------------------------
	// Warnings. The run ends hard when both players fall, so the clock says so before it happens: it
	// estimates the walk to the next hub and turns red, with a sound, once the clock no longer covers it.
	// -------------------------------------------------------------------------------------------

	/** Speed the estimate assumes, in cm/s. A jog, not a sprint: the warning should come early, not late. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning", meta = (ClampMin = "50.0"))
	float HubTravelSpeed = 450.0f;

	/** Straight-line distance times this is the route length the estimate assumes (walls, climbs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning", meta = (ClampMin = "1.0"))
	float HubPathFactor = 1.3f;

	/** Warn when the clock is below estimate x HubWarnRatio + HubWarnMargin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning", meta = (ClampMin = "1.0"))
	float HubWarnRatio = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning", meta = (ClampMin = "0.0"))
	float HubWarnMargin = 10.0f;

	/** Played on the warned player's screen when the warning starts, and again every WarningRepeat seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning")
	TObjectPtr<USoundBase> WarningSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Warning", meta = (ClampMin = "1.0"))
	float WarningRepeat = 5.0f;

	// -------------------------------------------------------------------------------------------
	// The player on the floor keeps a part: their view moves to the partner, and they can mark things
	// for the partner (enemies, a route) with a ping both screens see.
	// -------------------------------------------------------------------------------------------

	/** Seconds after falling before the view moves to the partner (the fall itself is worth seeing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Down", meta = (ClampMin = "0.0"))
	float SpectateDelay = 2.0f;

	/** Seconds a ping stays in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Down", meta = (ClampMin = "0.5"))
	float PingLifetime = 6.0f;

	/** Seconds between two pings from one player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Down", meta = (ClampMin = "0.0"))
	float PingCooldown = 1.0f;

	/** How far a ping reaches along the view, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Down", meta = (ClampMin = "100.0"))
	float PingRange = 6000.0f;

	// -------------------------------------------------------------------------------------------
	// Enemies give back some of what a fight cost: each one drops shards when it dies.
	// -------------------------------------------------------------------------------------------

	/** Shards an enemy drops on death. 0 turns the drop off. Uses ShardClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "0"))
	int32 EnemyShardCount = 1;

	/** Seconds each enemy shard hands back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Shards", meta = (ClampMin = "1.0"))
	float EnemyShardValue = 5.0f;

	/** Instant, SetByCaller on Data.Health.Damage. Every hit funnels through this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> HealthDamageEffect;

	/** Instant, SetByCaller on Data.Health.Amount. Healing and the revive path pay with this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Effects")
	TSubclassOf<UGameplayEffect> HealthRestoreEffect;
};
