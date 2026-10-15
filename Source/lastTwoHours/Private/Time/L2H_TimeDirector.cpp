// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_TimeDirector.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Character/L2H_BaseCharacter.h"
#include "Character/L2H_CoverComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffect.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Save/L2H_SaveSubsystem.h"
#include "World/L2H_CheckpointTerminal.h"
#include "World/L2H_ReviveStation.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Item/L2H_InventoryComponent.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Time/L2H_WorldTimeSubsystem.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeAttributeSet.h"
#include "Time/L2H_TimeShard.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "TimerManager.h"

namespace
{
	/**
	 * Native tag lookup. ErrorIfNotFound is false so a tag missing from the ini is a logged no-op
	 * rather than a startup check() -- the director already logs when it has to skip work.
	 */
	FGameplayTag LookupL2HTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), /*ErrorIfNotFound=*/false);
	}
}

AL2H_TimeDirector::AL2H_TimeDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	// The director has no components, so it never renders; logic is driven entirely by the
	// registration scan and the attribute-change delegate.
	SetReplicates(false);
}

const FGameplayTag& AL2H_TimeDirector::GetDownedTag()
{
	static const FGameplayTag Tag = LookupL2HTag(TEXT("State.Downed"));
	return Tag;
}

FName AL2H_TimeDirector::GetDrainMagnitudeName()
{
	// Must match the SetByCaller data name on UL2H_GE_TimeDrain.
	return FName(TEXT("Data.Time.Drain"));
}

FName AL2H_TimeDirector::GetAmountMagnitudeName()
{
	// Must match the SetByCaller data name on the cost, damage and restore effects.
	return FName(TEXT("Data.Time.Amount"));
}

FName AL2H_TimeDirector::GetHealthDamageMagnitudeName()
{
	// Must match the SetByCaller data name on UL2H_GE_HealthDamage.
	return FName(TEXT("Data.Health.Damage"));
}

FName AL2H_TimeDirector::GetHealthRestoreMagnitudeName()
{
	// Must match the SetByCaller data name on UL2H_GE_HealthRestore.
	return FName(TEXT("Data.Health.Amount"));
}

// -----------------------------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------------------------

AL2H_TimeDirector* AL2H_TimeDirector::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	// One director per level, so the first one found is the one. This runs on a button press rather
	// than per frame, so the lookup cost never shows up anywhere.
	return Cast<AL2H_TimeDirector>(
		UGameplayStatics::GetActorOfClass(WorldContextObject, AL2H_TimeDirector::StaticClass()));
}

UL2H_LevelTimeConfig* AL2H_TimeDirector::GetLevelConfig(const UObject* WorldContextObject)
{
	if (const AL2H_TimeDirector* Director = Get(WorldContextObject))
	{
		return Director->Config.Get();
	}

	// No director means no per-level tuning. Callers fall back to their own defaults rather than
	// refusing to act, which keeps a test map usable without a director placed in it.
	return nullptr;
}

void AL2H_TimeDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The settings object outlives a PIE session; leave the project's own value behind.
	if (bOffsetGamepadsForCouch)
	{
		GetMutableDefault<UGameMapsSettings>()->bOffsetPlayerGamepadIds = false;
		bOffsetGamepadsForCouch = false;
	}

	Super::EndPlay(EndPlayReason);
}

void AL2H_TimeDirector::BeginPlay()
{
	Super::BeginPlay();

	// The time loop is server-only. A client holds its own copy of every level actor in a listen
	// server session, and that copy must never run any of this.
	if (!HasAuthority())
	{
		return;
	}

	if (!Config)
	{
		L2HTime::LogImportant(TEXT("TimeDirector: no Config assigned -- the time loop will not run"));
	}
	else if (!Config->TimeDrainEffect || !Config->TimeCostEffect || !Config->TimeDamageEffect || !Config->TimeRestoreEffect)
	{
		L2HTime::LogImportant(TEXT("TimeDirector: Config is missing one or more effect classes"));
	}

	// Health is optional in the sense that MaxHealth 0 simply means "this level has no health
	// layer". A config that asks for one and names no effect, though, would silently cost Health
	// nothing -- which is exactly the sort of failure that still looks like working code.
	if (Config && Config->MaxHealth > 0.0f
		&& (!Config->HealthDamageEffect || !Config->HealthRestoreEffect))
	{
		L2HTime::LogImportant(TEXT("TimeDirector: Config wants a health layer but is missing HealthDamageEffect / HealthRestoreEffect"));
	}

	if (!GetDownedTag().IsValid())
	{
		L2HTime::LogImportant(TEXT("TimeDirector: State.Downed is not registered -- Downed will not gate abilities"));
	}

	if (Config && Config->bSpawnTestStations && GetNetMode() != NM_Client)
	{
		SpawnTestStations();
	}

	// Immediate pass so the first player is registered in the same frame, then a slow repeating
	// scan: the second local player is created after this runs, and a networked join can happen at
	// any time, so registration cannot be a one-shot.
	ScanForPlayers();
	GetWorldTimerManager().SetTimer(PlayerScanTimer, this, &AL2H_TimeDirector::ScanForPlayers, PlayerScanInterval, true);

	if (Config && Config->bCreateSecondLocalPlayer)
	{
		GetWorldTimerManager().SetTimer(SecondPlayerTimer, this, &AL2H_TimeDirector::CreateSecondLocalPlayer, Config->SecondLocalPlayerDelay, false);
	}
}

// -----------------------------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------------------------

void AL2H_TimeDirector::ScanForPlayers()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	// The couch partner was only ever a stand-in for a second player. When a networked player joins (a
	// PIE session with two clients does this a moment after the couch player is made), the run would
	// have three players and could never end a stage as a pair -- so the couch player steps aside.
	if (ULocalPlayer* Couch = CreatedLocalPlayer.Get())
	{
		bool bRemotePlayer = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && !It->Get()->IsLocalController())
			{
				bRemotePlayer = true;
				break;
			}
		}
		if (bRemotePlayer)
		{
			if (APlayerController* CouchPC = Couch->GetPlayerController(World))
			{
				if (APawn* CouchPawn = CouchPC->GetPawn())
				{
					CouchPawn->Destroy();
				}
			}
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				GameInstance->RemoveLocalPlayer(Couch);
			}
			CreatedLocalPlayer = nullptr;
			L2HTime::LogImportant(TEXT("TimeDirector: a networked player joined -- the couch second player was removed"));
		}
	}

	// A destroyed or respawned pawn leaves a dead weak entry behind; drop it so the list stays exact.
	RegisteredPawns.RemoveAll([](const TWeakObjectPtr<AL2H_BaseCharacter>& Weak) { return !Weak.IsValid(); });

	bool bRegisteredPawnThisScan = false;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AL2H_BaseCharacter* Character = PC ? Cast<AL2H_BaseCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			continue;
		}

		const bool bAlreadyKnown = RegisteredPawns.ContainsByPredicate(
			[Character](const TWeakObjectPtr<AL2H_BaseCharacter>& Weak) { return Weak.Get() == Character; });

		if (!bAlreadyKnown)
		{
			RegisterPawn(Character);
			bRegisteredPawnThisScan = true;
		}
	}

	// The startup checkpoint is offered here rather than from BeginPlay, because a restore needs every
	// pawn present and the second couch player does not exist until SecondLocalPlayerDelay has elapsed.
	// It is attempted only on a scan that actually registered somebody, so a checkpoint whose player
	// count no longer matches the level is refused once and then left alone, rather than being retried
	// every PlayerScanInterval and flooding the log with the same refusal.
	if (!bRegisteredPawnThisScan || bStartupCheckpointHandled || !Config || !Config->bRestoreCheckpointOnStart)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	UL2H_SaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem || !SaveSubsystem->HasCheckpoint(this))
	{
		// Nothing was ever written for this level, so there is nothing to wait for.
		bStartupCheckpointHandled = true;
		return;
	}

	if (SaveSubsystem->RestoreCheckpoint(this))
	{
		bStartupCheckpointHandled = true;
		L2HTime::LogImportant(TEXT("TimeDirector: the startup checkpoint was applied"));
	}
	else
	{
		// Refused -- usually because the second couch player has not registered yet. The latch stays
		// clear, so the next scan that registers somebody tries again.
		L2HTime::LogVerbose(TEXT("TimeDirector: the startup checkpoint was deferred to the next scan"));
	}
}

void AL2H_TimeDirector::RegisterPawn(AL2H_BaseCharacter* Character)
{
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character);
	if (!ASC || !ASC->GetSet<UL2H_TimeAttributeSet>())
	{
		return;
	}

	RegisteredPawns.Add(Character);

	// Registration is the one moment a pawn's body state is known to be freshly spawned, so death is
	// cleared here as well as in ResetPawn: a checkpoint restore can hand the director a pawn that was
	// already registered and already flagged.
	Character->SetDead(false);

	// Starting time is written as a base value, not applied as an effect: it has to override
	// whatever the Blueprint authored rather than stack on top of it, and it must not become a
	// modifier that a later GameplayEffect can aggregate with.
	if (Config)
	{
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetMaxTimeAttribute(), Config->MaxTime);
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetTimeAttribute(), Config->StartingTime);

		// Health is re-asserted for the same reason time is: a pawn comes back with whatever its
		// Blueprint authored, and the level config is the source of truth for what a run starts with.
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetMaxHealthAttribute(), Config->MaxHealth);
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetHealthAttribute(), Config->MaxHealth);
	}

	ApplyPassiveDrain(Character);

	// The watch is bound with the pawn captured. FOnAttributeChangeData does not carry the actor for
	// a base-value change, so one shared handler could not tell which player changed. Weak pointers
	// on both sides, because the delegate outlives neither guarantee.
	const TWeakObjectPtr<AL2H_BaseCharacter> WeakCharacter(Character);
	const TWeakObjectPtr<AL2H_TimeDirector> WeakThis(this);
	ASC->GetGameplayAttributeValueChangeDelegate(UL2H_TimeAttributeSet::GetTimeAttribute())
		.AddLambda([WeakThis, WeakCharacter](const FOnAttributeChangeData& Data)
		{
			AL2H_TimeDirector* Director = WeakThis.Get();
			AL2H_BaseCharacter* Pawn = WeakCharacter.Get();
			if (Director && Pawn)
			{
				Director->HandleTimeChangedFor(Pawn, Data.NewValue);
			}
		});

	L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s registered with %.1fs (drain %.2f/s)"),
		*GetNameSafe(Character), GetTimeFor(Character), Config ? Config->TimeDrainRate : 0.0f));
}

void AL2H_TimeDirector::ApplyPassiveDrain(AL2H_BaseCharacter* Character)
{
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character);
	if (!ASC || !Config || !Config->TimeDrainEffect)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Config->TimeDrainEffect, 1.0f, Context);
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		L2HTime::LogImportant(TEXT("TimeDirector: could not build the drain spec"));
		return;
	}

	// One drain per pawn: a pawn re-registered by a checkpoint restore, or resumed from a pause, keeps
	// the effect it already has instead of stacking a second clock on it.
	FDrainState& State = DrainStates.FindOrAdd(Character);
	if (State.PauseMask != 0 || (State.Handle.IsValid() && ASC->GetActiveGameplayEffect(State.Handle)))
	{
		return;
	}

	// Negative: drain removes time. SetByCaller rather than a baked magnitude, so the level's
	// TimeDrainRate stays the single source of truth for how fast the clock runs.
	Spec.Data->SetSetByCallerMagnitude(GetDrainMagnitudeName(), -FMath::Abs(Config->TimeDrainRate));
	State.Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void AL2H_TimeDirector::SetDrainPaused(AL2H_BaseCharacter* Character, EL2H_DrainPause Reason, bool bPaused)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	FDrainState& State = DrainStates.FindOrAdd(Character);
	const uint8 Bit = 1 << static_cast<uint8>(Reason);
	const uint8 OldMask = State.PauseMask;
	State.PauseMask = bPaused ? (OldMask | Bit) : (OldMask & ~Bit);

	if (OldMask == 0 && State.PauseMask != 0)
	{
		if (UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character))
		{
			if (State.Handle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(State.Handle);
			}
		}
		State.Handle = FActiveGameplayEffectHandle();
		L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s clock stopped (%s)"),
			*GetNameSafe(Character), *UEnum::GetValueAsString(Reason)));
	}
	else if (OldMask != 0 && State.PauseMask == 0)
	{
		// Only a registered pawn gets its clock back; one that was never registered has no drain to resume.
		if (RegisteredPawns.Contains(Character))
		{
			ApplyPassiveDrain(Character);
			L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s clock running again"), *GetNameSafe(Character)));
		}
	}
}

bool AL2H_TimeDirector::BuyShopItem(AL2H_BaseCharacter* Buyer, int32 Index)
{
	if (GetNetMode() == NM_Client || !Buyer || !Config || !Config->ShopItems.IsValidIndex(Index))
	{
		return false;
	}

	const UL2H_ItemDataAsset* Item = Config->ShopItems[Index];
	UL2H_InventoryComponent* Bag = Buyer->FindComponentByClass<UL2H_InventoryComponent>();
	const AL2H_CheckpointTerminal* Hub = Buyer->GetCurrentTerminal();
	const float Price = Item ? static_cast<float>(Item->Data.Price) : 0.0f;

	const TCHAR* Refusal = nullptr;
	if (!Item || !Bag)
	{
		Refusal = TEXT("nothing to sell, or no bag to put it in");
	}
	else if (!Hub || !Hub->SavesOnArrival())
	{
		Refusal = TEXT("the shop is only open in a hub");
	}
	else if (Buyer->GetChrons() < Price)
	{
		Refusal = TEXT("the bank is short");
	}
	else if (!Bag->CanAccept(Item, 1))
	{
		Refusal = TEXT("the bag is full");
	}
	if (Refusal)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Shop: %s could not buy %s -- %s"),
			*GetNameSafe(Buyer), *GetNameSafe(Item), Refusal));
		return false;
	}

	Buyer->AddChrons(-Price);
	Bag->ServerAddItem(Item, 1);

	// The purchase is part of what the hub's record holds: a death in the next stage returns the players
	// here with what they bought and the bank as it stood after paying.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr)
	{
		Save->RecordCurrentTime(Buyer, Hub->GetCheckpointId());
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Shop: %s bought %s for %.0f (bank %.0f)"),
		*GetNameSafe(Buyer), *GetNameSafe(Item), Price, Buyer->GetChrons()));
	return true;
}

void AL2H_TimeDirector::SpawnTestStations()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AL2H_CheckpointTerminal> It(World); It; ++It)
	{
		if (It->SavesOnArrival())
		{
			return; // The level has its own hub.
		}
	}

	const APlayerStart* Start = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = *It;
		break;
	}
	const FVector Origin = Start ? Start->GetActorLocation() : GetActorLocation();
	const FRotator Facing(0.0f, Start ? Start->GetActorRotation().Yaw : 0.0f, 0.0f);
	const FVector Forward = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);

	// On the floor under each spot, so the trigger box (120 cm half height) sits on the ground.
	auto Place = [World](const FVector& Where)
	{
		FHitResult Hit;
		const FVector From = Where + FVector(0.0f, 0.0f, 500.0f);
		const FVector To = Where - FVector(0.0f, 0.0f, 2000.0f);
		const float Floor = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility) ? Hit.ImpactPoint.Z : Where.Z - 90.0f;
		return FVector(Where.X, Where.Y, Floor + 120.0f);
	};

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform HubAt(Facing, Place(Origin - Forward * 700.0f));
	if (AL2H_CheckpointTerminal* Hub = World->SpawnActorDeferred<AL2H_CheckpointTerminal>(AL2H_CheckpointTerminal::StaticClass(), HubAt))
	{
		Hub->Tier = EL2H_CheckpointTier::Hub;
		Hub->CheckpointId = TEXT("TEST_HUB");
		Hub->DisplayName = NSLOCTEXT("L2HStation", "TestHub", "Test Hub");
		Hub->FinishSpawning(HubAt);
	}

	const FTransform SpotAt(Facing, Place(Origin - Forward * 700.0f + Right * 700.0f));
	if (AL2H_CheckpointTerminal* Spot = World->SpawnActorDeferred<AL2H_CheckpointTerminal>(AL2H_CheckpointTerminal::StaticClass(), SpotAt))
	{
		Spot->Tier = EL2H_CheckpointTier::Spot;
		Spot->CheckpointId = TEXT("TEST_SPOT");
		Spot->DisplayName = NSLOCTEXT("L2HStation", "TestSpot", "Test Save Spot");
		Spot->FinishSpawning(SpotAt);
	}

	World->SpawnActor<AL2H_ReviveStation>(AL2H_ReviveStation::StaticClass(), Place(Origin - Forward * 700.0f - Right * 700.0f), Facing, Params);

	L2HTime::LogImportant(TEXT("TimeDirector: spawned the test hub (green), save spot (blue) and revive station (red)"));
}

bool AL2H_TimeDirector::BuyPartnerReviveAtHub(AL2H_BaseCharacter* Buyer)
{
	if (GetNetMode() == NM_Client || !Buyer || !Config)
	{
		return false;
	}

	AL2H_BaseCharacter* Partner = FindPartnerOf(Buyer);
	AL2H_CheckpointTerminal* Hub = Buyer->GetCurrentTerminal();
	const TCHAR* Refusal = nullptr;
	if (!Hub || !Hub->SavesOnArrival())
	{
		Refusal = TEXT("not standing in a hub");
	}
	else if (!Partner || !(Partner->IsDead() || Partner->IsDowned()))
	{
		Refusal = TEXT("the partner is not down");
	}
	else if (Buyer->IsDead() || Buyer->IsDowned())
	{
		Refusal = TEXT("the buyer is down");
	}
	else if (Buyer->GetChrons() < Config->HubRevivePrice)
	{
		Refusal = TEXT("the bank is short");
	}
	if (Refusal)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("HubRevive: %s refused -- %s"), *GetNameSafe(Buyer), Refusal));
		return false;
	}

	Buyer->AddChrons(-Config->HubRevivePrice);

	// Put down first if someone was carrying the body, then moved beside the buyer: standing it up where it
	// fell would leave the pair split, and the stage only ends with both inside.
	for (AL2H_BaseCharacter* Pawn : GetRegisteredPawns())
	{
		if (Pawn && Pawn->GetCarriedActor() == Partner)
		{
			Pawn->EndCarry();
		}
	}
	const FVector Beside = Buyer->GetActorLocation() + Buyer->GetActorRightVector() * 90.0f;
	Partner->TeleportTo(Beside, Buyer->GetActorRotation(), false, true);

	RevivePawn(Partner, Partner->IsDead() ? Config->MaxHealth * 0.5f : 0.0f,
		FMath::Max(GetTimeFor(Partner), Config->HubReviveSeconds));

	L2HTime::LogImportant(FString::Printf(TEXT("HubRevive: %s paid %.0f to bring %s to %s"),
		*GetNameSafe(Buyer), Config->HubRevivePrice, *GetNameSafe(Partner), *Hub->GetCheckpointId().ToString()));

	// The overlap that would normally announce the arrival may not fire for a teleport inside the same
	// frame, so the hub is told directly; it ends the stage if both are now in.
	NotifyHubEntered(Partner, Hub);
	return true;
}

bool AL2H_TimeDirector::ApplyItemEffect(AL2H_BaseCharacter* User, const FL2H_ItemInstance& Item)
{
	if (GetNetMode() == NM_Client || !User || !Item.IsSet() || !Config)
	{
		return false;
	}

	const FL2H_ItemBaseData& Data = Item.Data->Data;
	switch (Data.UseEffect)
	{
	case EL2H_ItemUseEffect::None:
		// Not a tool: the inventory keeps whatever else it does with a use.
		return true;

	case EL2H_ItemUseEffect::HealthInjector:
	{
		// A partner who fell to damage, lying within reach, gets the injection: health is what they ran
		// out of. Their clock stays as it was. Otherwise it is the user's own heal.
		AL2H_BaseCharacter* Partner = FindPartnerOf(User);
		if (Partner && Partner->IsDead() && !Partner->IsDowned() && !Partner->IsBeingCarried()
			&& FVector::Dist(Partner->GetActorLocation(), User->GetActorLocation()) <= Data.EffectRange)
		{
			RevivePawn(Partner, FMath::Min(Data.EffectMagnitude, Config->MaxHealth), FMath::Max(GetTimeFor(Partner), 0.0f));
			L2HTime::LogImportant(FString::Printf(TEXT("Injector: %s revived %s"), *GetNameSafe(User), *GetNameSafe(Partner)));
			return true;
		}
		ApplyHealthRestore(User, Data.EffectMagnitude);
		return true;
	}

	case EL2H_ItemUseEffect::TimeElixir:
	{
		AL2H_BaseCharacter* Partner = FindPartnerOf(User);
		if (!Partner || FVector::Dist(Partner->GetActorLocation(), User->GetActorLocation()) > Data.EffectRange)
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Elixir: %s has no partner within %.0f cm -- kept"),
				*GetNameSafe(User), Data.EffectRange));
			return false;
		}
		if (Partner->IsDead() || Partner->IsDowned())
		{
			// Up on the spot, at any point after the fall: the near revive the elixir is bought for.
			RevivePawn(Partner, Partner->IsDead() ? Config->MaxHealth * 0.5f : 0.0f,
				FMath::Max(GetTimeFor(Partner), 0.0f) + Data.EffectMagnitude);
		}
		else
		{
			ApplyTimeRestore(Partner, Data.EffectMagnitude);
		}
		L2HTime::LogImportant(FString::Printf(TEXT("Elixir: %s gave %s %.0fs"),
			*GetNameSafe(User), *GetNameSafe(Partner), Data.EffectMagnitude));
		return true;
	}

	case EL2H_ItemUseEffect::TimeStop:
	{
		UL2H_WorldTimeSubsystem* WorldTime = GetWorld() ? GetWorld()->GetSubsystem<UL2H_WorldTimeSubsystem>() : nullptr;
		if (bTimeStopped || !WorldTime || WorldTime->IsWorldSlowed())
		{
			L2HTime::LogImportant(TEXT("TimeStop: refused -- time is already bent -- kept"));
			return false;
		}

		// The world nearly stops; both players are compensated back to full speed. Their own clocks run on
		// the world's timers, so they stop too: the seconds spent inside a time stop are free.
		const float Dilation = Config->TimeStopDilation;
		WorldTime->ApplyWorldSlow(Dilation, User);
		TimeStopCaster = User;
		TimeStopExempt.Reset();
		TArray<AActor*> Exempt;
		for (AL2H_BaseCharacter* Pawn : GetRegisteredPawns())
		{
			if (Pawn && Pawn != User)
			{
				Exempt.Add(Pawn);
				TimeStopExempt.Add(Pawn);
			}
		}
		WorldTime->ApplyActorSlow(Exempt, 1.0f / Dilation);
		bTimeStopped = true;

		// World timers run in world time, so the real duration is scaled down by the dilation.
		GetWorldTimerManager().SetTimer(TimeStopTimer, this, &AL2H_TimeDirector::EndTimeStop,
			FMath::Max(Data.EffectMagnitude * Dilation, 0.01f), false);
		L2HTime::LogImportant(FString::Printf(TEXT("TimeStop: %s stopped the world for %.1fs"),
			*GetNameSafe(User), Data.EffectMagnitude));
		return true;
	}

	case EL2H_ItemUseEffect::TimeShield:
	{
		if (User->GetShieldHits() > 0 || User->IsDead() || User->IsDowned())
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Shield: %s refused -- %s -- kept"), *GetNameSafe(User),
				User->GetShieldHits() > 0 ? TEXT("a shield is already up") : TEXT("on the floor")));
			return false;
		}
		User->SetShieldHits(FMath::Max(FMath::RoundToInt(Data.EffectMagnitude), 1));
		L2HTime::LogImportant(FString::Printf(TEXT("Shield: %s takes the next %d hits"), *GetNameSafe(User), User->GetShieldHits()));
		return true;
	}

	case EL2H_ItemUseEffect::RewindWatch:
	{
		// Back to a moment ago: the place and the Health. Not the clock -- the watch was paid for with the
		// bank, and giving seconds back would make it a second elixir. So it cannot raise a player whose
		// clock ran out, only one who fell to damage (a missed jump, a bad fight).
		AL2H_BaseCharacter::FRewindSample Sample;
		const bool bOutOfTime = User->IsDowned() && !User->IsDead();
		if (bOutOfTime || User->IsBeingCarried() || !User->FindRewindSample(Data.EffectMagnitude, Sample))
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Rewind: %s refused -- %s -- kept"), *GetNameSafe(User),
				bOutOfTime ? TEXT("the clock ran out") : TEXT("no safe moment to go back to")));
			return false;
		}
		User->EndCarry();
		const float Health = FMath::Max(Sample.Health, 1.0f);
		if (User->IsDead())
		{
			RevivePawn(User, Health, FMath::Max(GetTimeFor(User), 0.0f));
		}
		else
		{
			SetHealthTo(User, Health);
		}
		User->TeleportTo(Sample.Location, Sample.Rotation, false, true);
		User->ClearRewindHistory();
		L2HTime::LogImportant(FString::Printf(TEXT("Rewind: %s went back %.0fs to %s with %.0f hp"), *GetNameSafe(User),
			Data.EffectMagnitude, *Sample.Location.ToCompactString(), Health));
		return true;
	}

	case EL2H_ItemUseEffect::Beacon:
	{
		if (User->IsDead() || User->IsDowned() || !GetWorld())
		{
			return false;
		}
		// A revive station of one charge, put down where the user stands. Gone once used, at the end of the
		// stage, or on a rewind; it is the carry's answer to the elixir: cheap, but someone has to walk.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = User;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Where = User->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		AL2H_ReviveStation* Beacon = GetWorld()->SpawnActor<AL2H_ReviveStation>(AL2H_ReviveStation::StaticClass(), Where, FRotator::ZeroRotator, SpawnParams);
		if (!Beacon)
		{
			return false;
		}
		Beacon->MakePortable(Data.EffectMagnitude);
		L2HTime::LogImportant(FString::Printf(TEXT("Beacon: %s placed one at %s (+%.0fs)"), *GetNameSafe(User),
			*Where.ToCompactString(), Data.EffectMagnitude));
		return true;
	}

	case EL2H_ItemUseEffect::PrepaidMagazine:
	{
		UL2H_WeaponComponent* Weapons = User->FindComponentByClass<UL2H_WeaponComponent>();
		if (!Weapons || !Weapons->RefillClipFree())
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Magazine: %s has nothing to refill -- kept"), *GetNameSafe(User)));
			return false;
		}
		Weapons->PlayReloadAnimation();
		return true;
	}
	}
	return true;
}

void AL2H_TimeDirector::EndTimeStop()
{
	if (!bTimeStopped)
	{
		return;
	}
	bTimeStopped = false;
	if (UL2H_WorldTimeSubsystem* WorldTime = GetWorld() ? GetWorld()->GetSubsystem<UL2H_WorldTimeSubsystem>() : nullptr)
	{
		TArray<AActor*> Exempt;
		for (const TWeakObjectPtr<AActor>& Weak : TimeStopExempt)
		{
			if (AActor* Actor = Weak.Get())
			{
				Exempt.Add(Actor);
			}
		}
		WorldTime->RestoreActorSlow(Exempt);
		WorldTime->EndWorldSlow(TimeStopCaster.Get());
	}
	TimeStopExempt.Reset();
	TimeStopCaster.Reset();
	L2HTime::LogImportant(TEXT("TimeStop: the world runs again"));
}

float AL2H_TimeDirector::GetTransferCost(const AL2H_BaseCharacter* Giver) const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Config || !Giver || !GameState)
	{
		return 0.0f;
	}

	const float Now = static_cast<float>(GameState->GetServerWorldTimeSeconds());
	const bool bInFreeWindow = Now - Giver->LastTransferGiveTime <= Config->TransferFreeWindow;
	return Config->TransferAmount + (bInFreeWindow ? 0.0f : Config->TransferFirstTax);
}

bool AL2H_TimeDirector::TransferTime(AL2H_BaseCharacter* Giver, AL2H_BaseCharacter* Receiver)
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GetNetMode() == NM_Client || !Config || !Giver || !Receiver || Giver == Receiver || !GameState)
	{
		return false;
	}

	const TCHAR* Refusal = nullptr;

	// Never more than the receiver's clock can hold: the attribute clamp would swallow the rest after the
	// giver had already paid for it. A body on the floor holds its packets aside, so it has room for all.
	float Gift = Config->TransferAmount;
	if (!Receiver->IsDead() && !Receiver->IsDowned())
	{
		UAbilitySystemComponent* ReceiverASC = ResolveAbilitySystem(Receiver);
		const UL2H_TimeAttributeSet* Set = ReceiverASC ? ReceiverASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;
		if (Set)
		{
			Gift = FMath::Min(Gift, FMath::FloorToFloat(Set->GetMaxTime() - Set->GetTime()));
		}
	}
	const float Cost = GetTransferCost(Giver) - Config->TransferAmount + Gift;

	if (Gift < 1.0f)
	{
		Refusal = TEXT("the receiver's clock is full");
	}
	else if (Giver->IsDead() || Giver->IsDowned())
	{
		Refusal = TEXT("the giver is down");
	}
	else if (GetTimeFor(Giver) - Cost < 1.0f)
	{
		// The gift may not end the giver: one second has to be left on their own clock.
		Refusal = TEXT("the giver cannot pay and keep a second");
	}
	if (Refusal)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Transfer: %s -> %s refused -- %s"),
			*GetNameSafe(Giver), *GetNameSafe(Receiver), Refusal));
		return false;
	}

	ApplyTimeCost(Giver, Cost);
	Giver->LastTransferGiveTime = static_cast<float>(GameState->GetServerWorldTimeSeconds());

	if (Receiver->IsDead() || Receiver->IsDowned())
	{
		// Held, not applied: a body on the floor stands up only once enough has arrived, with all of it.
		Receiver->PendingReviveSeconds += Gift;
		if (Receiver->PendingReviveSeconds >= Config->RemoteReviveThreshold)
		{
			const float Seconds = Receiver->PendingReviveSeconds;
			const float Health = Receiver->IsDead() ? Config->MaxHealth * 0.5f : 0.0f;
			RevivePawn(Receiver, Health, FMath::Max(GetTimeFor(Receiver), 0.0f) + Seconds);
		}
	}
	else
	{
		ApplyTimeRestore(Receiver, Gift);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Transfer: %s gave %s %.0fs for %.0fs (waiting to revive: %.0fs)"),
		*GetNameSafe(Giver), *GetNameSafe(Receiver), Gift, Cost, Receiver->PendingReviveSeconds));
	return true;
}

void AL2H_TimeDirector::NotifyHubEntered(AL2H_BaseCharacter* Character, AL2H_CheckpointTerminal* Hub)
{
	if (GetNetMode() == NM_Client || !Character || !Hub)
	{
		return;
	}

	SetDrainPaused(Character, EL2H_DrainPause::Checkpoint, true);

	// The stage ends once, on the arrival that completes the pair. A hub already in the run's record was
	// completed on an earlier visit (or restored into); walking back in is a rest, not a second payout.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;
	const FName HubId = Hub->GetCheckpointId();
	if (Save && Save->GetCheckpoints(this).ContainsByPredicate([HubId](const FL2H_CheckpointRecord& R) { return R.Id == HubId; }))
	{
		return;
	}

	for (AL2H_BaseCharacter* Pawn : GetRegisteredPawns())
	{
		// Everybody, standing, inside. A partner on the floor elsewhere keeps the stage open: the run only
		// advances as a pair.
		if (!Pawn || Pawn->IsDead() || Pawn->IsDowned() || !Hub->IsPawnInRange(Pawn))
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Hub: %s is in %s, waiting for %s"),
				*GetNameSafe(Character), *HubId.ToString(), *GetNameSafe(Pawn)));
			return;
		}
	}

	CompleteSegment(Hub);
}

void AL2H_TimeDirector::NotifyHubExited(AL2H_BaseCharacter* Character, AL2H_CheckpointTerminal* Hub)
{
	if (GetNetMode() == NM_Client || !Character)
	{
		return;
	}

	// Leaving makes the carry choice final; the clock runs again from the number it now shows.
	Character->CloseHubBank();
	SetDrainPaused(Character, EL2H_DrainPause::Checkpoint, false);
}

void AL2H_TimeDirector::CompleteSegment(AL2H_CheckpointTerminal* Hub)
{
	if (!Hub || !Config)
	{
		return;
	}

	const float NextTime = Hub->NextSegmentTime > 0.0f ? Hub->NextSegmentTime : Config->StartingTime;

	for (AL2H_BaseCharacter* Pawn : GetRegisteredPawns())
	{
		if (!Pawn)
		{
			continue;
		}

		// Every leftover second goes to the bank first; carrying some back is the player's own move,
		// made afterwards from the menu, so doing nothing is the original rule: all of it is currency.
		const float Leftover = FMath::Max(GetTimeFor(Pawn), 0.0f);
		Pawn->AddChrons(Leftover);

		// The ceiling has to make room for the stage's clock plus whatever may be carried onto it, or the
		// attribute clamp would quietly eat the carried seconds after they had left the bank.
		if (UAbilitySystemComponent* ASC = ResolveAbilitySystem(Pawn))
		{
			const float Ceiling = FMath::Max(Config->MaxTime, NextTime + Config->MaxCarryOver);
			ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetMaxTimeAttribute(), Ceiling);
		}
		ApplyTimeTo(Pawn, NextTime);
		Pawn->OpenHubBank(FMath::Min(Leftover, Config->MaxCarryOver));
		Pawn->SetLastHubId(Hub->GetCheckpointId());

		L2HTime::LogImportant(FString::Printf(TEXT("Hub: %s banked %.0fs (bank %.0f), next stage starts at %.0fs"),
			*GetNameSafe(Pawn), Leftover, Pawn->GetChrons(), NextTime));
	}

	// A new stage, new revive charges.
	for (TActorIterator<AL2H_ReviveStation> It(GetWorld()); It; ++It)
	{
		It->Refill();
	}

	// Recorded after the bank and the new clock, so a death in the next stage returns the players here
	// with exactly what they walked out with.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr)
	{
		Save->RegisterCheckpoint(this, Hub->GetCheckpointId(), Hub->DisplayName);
	}
}

bool AL2H_TimeDirector::AdjustHubCarry(AL2H_BaseCharacter* Character, int32 Direction)
{
	if (GetNetMode() == NM_Client || !Character || !Config || Direction == 0 || !Character->IsHubBankOpen())
	{
		return false;
	}

	const float Step = Config->CarryStep;
	const float Carried = Character->GetHubCarried();

	if (Direction > 0)
	{
		const float Amount = FMath::Min3(Step, Character->GetHubCarryAllowance() - Carried, Character->GetChrons());
		if (Amount <= 0.0f)
		{
			return false;
		}
		Character->AddChrons(-Amount);
		ApplyTimeRestore(Character, Amount);
		Character->SetHubCarried(Carried + Amount);
	}
	else
	{
		const float Amount = FMath::Min(Step, Carried);
		if (Amount <= 0.0f)
		{
			return false;
		}
		ApplyTimeCost(Character, Amount);
		Character->AddChrons(Amount);
		Character->SetHubCarried(Carried - Amount);
	}

	if (const AL2H_CheckpointTerminal* Hub = Character->GetCurrentTerminal())
	{
		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr)
		{
			Save->RecordCurrentTime(Character, Hub->GetCheckpointId());
		}
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Hub: %s carries %.0fs into the next stage (bank %.0f, clock %.0fs)"),
		*GetNameSafe(Character), Character->GetHubCarried(), Character->GetChrons(), GetTimeFor(Character)));
	return true;
}

bool AL2H_TimeDirector::IsDrainPaused(const AL2H_BaseCharacter* Character) const
{
	for (const TPair<TWeakObjectPtr<AL2H_BaseCharacter>, FDrainState>& Pair : DrainStates)
	{
		if (Pair.Key.Get() == Character)
		{
			return Pair.Value.PauseMask != 0;
		}
	}
	return false;
}

int32 AL2H_TimeDirector::FindReviveToolCell(const AL2H_BaseCharacter* Reviver, const AL2H_BaseCharacter* Target)
{
	const UL2H_InventoryComponent* Bag = Reviver ? Reviver->FindComponentByClass<UL2H_InventoryComponent>() : nullptr;
	if (!Bag || !Target || !(Target->IsDead() || Target->IsDowned()))
	{
		return INDEX_NONE;
	}

	// The cheaper injector mends a fall to damage; a clock at zero needs the elixir's seconds.
	if (Target->IsDead() && !Target->IsDowned())
	{
		const int32 Injector = Bag->FindToolCell(EL2H_ItemUseEffect::HealthInjector);
		if (Injector != INDEX_NONE)
		{
			return Injector;
		}
	}
	return Bag->FindToolCell(EL2H_ItemUseEffect::TimeElixir);
}

bool AL2H_TimeDirector::TryFreeRevive(AL2H_BaseCharacter* Reviver, AL2H_BaseCharacter* Target)
{
	if (!HasAuthority() || !Reviver || !Target || !Config || Reviver == Target)
	{
		return false;
	}

	// Past the free window (or down to the clock, which a free revive cannot mend) the same hold spends a
	// tool from the reviver's bag on the body: the injector for a fall to damage, else the elixir.
	if (!Target->IsFreeReviveOpen() || !Target->IsDead())
	{
		if (Reviver->IsDead() || Reviver->IsDowned() || Reviver->IsBeingCarried())
		{
			return false;
		}
		const int32 Cell = FindReviveToolCell(Reviver, Target);
		UL2H_InventoryComponent* Bag = Reviver->FindComponentByClass<UL2H_InventoryComponent>();
		if (Cell == INDEX_NONE || !Bag)
		{
			L2HTime::LogImportant(FString::Printf(TEXT("Revive: %s could not revive %s -- the free window is over and the bag has no injector or elixir"),
				*GetNameSafe(Reviver), *GetNameSafe(Target)));
			return false;
		}
		Bag->RequestUse(Cell);
		return !Target->IsDead() && !Target->IsDowned();
	}

	const TCHAR* Refusal = nullptr;
	if (Reviver->IsDead() || Reviver->IsDowned() || Reviver->IsBeingCarried())
	{
		Refusal = TEXT("the reviver is down");
	}
	else if (FVector::Dist(Reviver->GetActorLocation(), Target->GetActorLocation()) > Config->FreeReviveRange + 100.0f)
	{
		// 100 cm of slack over the client's own check: the body is a ragdoll, and the replicated capsule
		// the client measured against trails the server's by a little.
		Refusal = TEXT("the reviver is too far");
	}

	if (Refusal)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Revive: %s could not free-revive %s -- %s"),
			*GetNameSafe(Reviver), *GetNameSafe(Target), Refusal));
		return false;
	}

	// Health only. The clock stays exactly where it was: a free revive that handed out seconds was a way
	// to mint time by dying on purpose.
	RevivePawn(Target, Config->MaxHealth * Config->FreeReviveHealthFraction, GetTimeFor(Target));
	L2HTime::LogImportant(FString::Printf(TEXT("Revive: %s free-revived %s"), *GetNameSafe(Reviver), *GetNameSafe(Target)));
	return true;
}

// -----------------------------------------------------------------------------------------------
// The zero crossing
// -----------------------------------------------------------------------------------------------

void AL2H_TimeDirector::HandleTimeChangedFor(AL2H_BaseCharacter* Character, float NewValue)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	// A dead pawn is already out of the fight, and AreAllPlayersDowned must not read a dead player as
	// merely downed -- that distinction is what keeps a wipe from being mistaken for a recoverable
	// pause. The pose is re-derived explicitly by RevivePawn instead of being driven from here.
	if (Character->IsDead())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character);
	const FGameplayTag DownedTag = GetDownedTag();
	const bool bShouldBeDowned = NewValue <= 0.0f;

	if (bShouldBeDowned && !Character->IsDowned())
	{
		// Tag first: abilities and the HUD gate on State.Downed, and they must see it in the same
		// frame the ragdoll starts, not after it lands.
		if (ASC)
		{
			if (DownedTag.IsValid())
			{
				ASC->AddLooseGameplayTag(DownedTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
			}

			// Zero seconds is not death, but it does end whatever the player was doing.
			ASC->CancelAllAbilities();
		}

		Character->SetDowned(true);

		L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s reached 0s -> Downed"), *GetNameSafe(Character)));
		OnPlayerDowned(Character);
		CheckAllDowned();
	}
	else if (!bShouldBeDowned && Character->IsDowned())
	{
		ClearDownedTag(ASC);

		Character->SetDowned(false);

		L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s recovered to %.1fs -> standing"),
			*GetNameSafe(Character), NewValue));
		OnPlayerRevived(Character);
	}
}

void AL2H_TimeDirector::CheckAllDowned()
{
	// Dead counts as out too: a downed player whose only partner is dead has nobody left to hand them
	// time, and waiting for that would wait forever.
	if (IsAnyPlayerStanding())
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(ResetRunTimer))
	{
		return;
	}

	L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: every player is downed -- rewinding to the checkpoint in %.1fs"), AllDownedResetDelay));

	// Never resolve this from inside the attribute broadcast that got us here -- defer at least a tick.
	if (AllDownedResetDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ResetRunTimer, this, &AL2H_TimeDirector::ReturnToCheckpointOrReset, AllDownedResetDelay, false);
	}
	else
	{
		ResetRunTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AL2H_TimeDirector::ReturnToCheckpointOrReset);
	}
}

void AL2H_TimeDirector::ClearDownedTag(UAbilitySystemComponent* ASC)
{
	const FGameplayTag DownedTag = GetDownedTag();
	if (ASC && DownedTag.IsValid() && ASC->GetTagCount(DownedTag) > 0)
	{
		ASC->RemoveLooseGameplayTag(DownedTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

// -----------------------------------------------------------------------------------------------
// Mutation
// -----------------------------------------------------------------------------------------------

UAbilitySystemComponent* AL2H_TimeDirector::ResolveAbilitySystem(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Actor))
	{
		if (UAbilitySystemComponent* FromInterface = Interface->GetAbilitySystemComponent())
		{
			return FromInterface;
		}
	}

	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

float AL2H_TimeDirector::GetHealthFor(const AActor* Target)
{
	// ResolveAbilitySystem is spelled for a mutable actor because the interface path has to be; a
	// pure query has no business holding a mutable pointer, so the constness stops at this line.
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(const_cast<AActor*>(Target));
	const UL2H_TimeAttributeSet* Set = ASC ? ASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;
	// -1 means "no health layer on this thing", which every caller reads as "use the pure time path".
	return Set ? Set->GetHealth() : -1.0f;
}

bool AL2H_TimeDirector::HasHealthLayer() const
{
	return Config && Config->MaxHealth > 0.0f;
}

float AL2H_TimeDirector::GetTimeFor(const AActor* Target)
{
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(const_cast<AActor*>(Target));
	if (!ASC)
	{
		return -1.0f;
	}

	const UL2H_TimeAttributeSet* TimeSet = ASC->GetSet<UL2H_TimeAttributeSet>();
	return TimeSet ? TimeSet->GetTime() : -1.0f;
}

TArray<AL2H_BaseCharacter*> AL2H_TimeDirector::GetRegisteredPawns() const
{
	// Flattened from the weak array on every call rather than cached: a pawn can be destroyed at any
	// moment, and a caller iterating a stale copy would hand a shard a null collector and the save a
	// null pawn.
	TArray<AL2H_BaseCharacter*> Pawns;
	Pawns.Reserve(RegisteredPawns.Num());

	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		if (AL2H_BaseCharacter* Character = Weak.Get())
		{
			Pawns.Add(Character);
		}
	}

	return Pawns;
}

void AL2H_TimeDirector::SpillShardAt(AActor* Target, float TimeValue)
{
	const float Value = TimeValue > 0.0f ? TimeValue : (Config ? Config->ShardTimeValue : 0.0f);
	UWorld* World = GetWorld();
	if (!HasAuthority() || !Target || !World || !Config || !Config->ShardClass)
	{
		return;
	}

	// Scattered off the victim's centre rather than dropped on it: a shard that spawns inside the
	// player is absorbed on the very next tick, and then the lost seconds were never really in the
	// world at all.
	const float Radius = FMath::Max(0.0f, Config->ShardSpawnRadius);
	const FVector Offset = FMath::VRand() * FMath::FRandRange(Radius * 0.5f, Radius);
	const FVector SpawnLocation = Target->GetActorLocation() + Offset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AL2H_TimeShard* Shard = World->SpawnActor<AL2H_TimeShard>(
		Config->ShardClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (!Shard)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeDirector: could not spill a shard for %s"), *GetNameSafe(Target)));
		return;
	}

	// Configured after the spawn rather than in the constructor, because a shard is also placeable in
	// a level by hand and a designer-placed one has to keep the values authored on it.
	Shard->InitialiseFromConfig(Value, Config->ShardLifetime);

	// The victim is inside the magnet's range by construction, so without a short window the seconds
	// were back in their pocket on the next tick and the hit cost a third of what it said.
	if (Target->IsA<AL2H_BaseCharacter>())
	{
		Shard->IgnoreCollectorFor(Target, 1.5f);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s spilled a %.1fs shard"),
		*GetNameSafe(Target), Value));
}

void AL2H_TimeDirector::ApplyTimeEffect(AActor* Target, float Magnitude, TSubclassOf<UGameplayEffect> EffectClass, const FName& MagnitudeName)
{
	if (!HasAuthority() || !Target || !EffectClass || FMath::IsNearlyZero(Magnitude))
	{
		return;
	}

	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Target);
	if (!ASC || !ASC->GetSet<UL2H_TimeAttributeSet>())
	{
		L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s has no time system -- change ignored"), *GetNameSafe(Target)));
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		L2HTime::LogImportant(TEXT("TimeDirector: could not build the time spec"));
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(MagnitudeName, Magnitude);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void AL2H_TimeDirector::ApplyTimeDamage(AActor* Target, float Amount)
{
	if (!Config || !Target || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	// Dodge i-frames (State.Invulnerable.*) swallow the whole hit, health loss and shard spill
	// included. The lookup goes through the component, so it also covers pawns that add the ASC in
	// Blueprint. Checked here, once, because this is the single entry every hazard uses.
	static const FGameplayTag InvulnerableTag = LookupL2HTag(TEXT("State.Invulnerable"));
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (InvulnerableTag.IsValid() && TargetASC && TargetASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return;
	}

	// Health sits INSIDE this funnel rather than beside it, which is the whole reason every existing
	// hazard -- the weapon ability, the damage volumes -- gained the health layer without a single one
	// of them being edited: they all already came through here. A target with a health layer takes the
	// hit as Health and pays the Time that Health was worth; a target without one keeps the old
	// pure-time behaviour, so a hazard pointed at a crate still works exactly as it did.
	if (Config->MaxHealth > 0.0f && GetHealthFor(Target) >= 0.0f)
	{
		ApplyHealthDamage(Target, Amount);
		return;
	}

	ApplyTimedDamageInner(Target, Amount);
}

void AL2H_TimeDirector::ApplyTimedDamageInner(AActor* Target, float Amount)
{
	ApplyTimeEffect(Target, -FMath::Abs(Amount), Config->TimeDamageEffect, GetAmountMagnitudeName());

	// A hit heavy enough to hurt leaves the seconds it took lying on the ground, so a big loss is
	// something to go and collect rather than a permanent subtraction. Threshold 0 disables the spill,
	// and the rule lives here rather than in each hazard, so no hazard has to remember to do it.
	// Only something that actually had seconds to lose can drop them. A wall, a crate or a door has no
	// time set, so the effect above did nothing -- and spilling anyway minted free seconds from every
	// shot into scenery.
	const UAbilitySystemComponent* TargetASC = ResolveAbilitySystem(Target);
	const bool bHasClock = TargetASC && TargetASC->GetSet<UL2H_TimeAttributeSet>() != nullptr;

	if (bHasClock && Config->ShardClass && Config->ShardThreshold > 0.0f
		&& FMath::Abs(Amount) >= Config->ShardThreshold)
	{
		SpillShardAt(Target);
	}
}

void AL2H_TimeDirector::ApplyTimeCost(AActor* Target, float Amount)
{
	if (!Config)
	{
		return;
	}

	ApplyTimeEffect(Target, -FMath::Abs(Amount), Config->TimeCostEffect, GetAmountMagnitudeName());
}

void AL2H_TimeDirector::ApplyTimeRestore(AActor* Target, float Amount)
{
	if (!Config)
	{
		return;
	}

	ApplyTimeEffect(Target, FMath::Abs(Amount), Config->TimeRestoreEffect, GetAmountMagnitudeName());
}

// -----------------------------------------------------------------------------------------------
// Run control
// -----------------------------------------------------------------------------------------------

bool AL2H_TimeDirector::AreAllPlayersDowned() const
{
	bool bSawAPawn = false;

	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		const AL2H_BaseCharacter* Character = Weak.Get();
		if (!Character)
		{
			continue;
		}

		bSawAPawn = true;
		if (!Character->IsDowned())
		{
			return false;
		}
	}

	return bSawAPawn;
}

// -----------------------------------------------------------------------------------------------
// Time request
// -----------------------------------------------------------------------------------------------

AL2H_BaseCharacter* AL2H_TimeDirector::FindPartnerOf(const AL2H_BaseCharacter* Requester) const
{
	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		AL2H_BaseCharacter* Candidate = Weak.Get();
		if (Candidate && Candidate != Requester)
		{
			return Candidate;
		}
	}

	return nullptr;
}

bool AL2H_TimeDirector::PostTimeRequest(AL2H_BaseCharacter* Requester)
{
	if (!HasAuthority() || !Requester)
	{
		return false;
	}

	const UL2H_LevelTimeConfig* TimeConfig = Config;
	if (!TimeConfig)
	{
		L2HTime::LogImportant(TEXT("Request: refused -- the director has no config"));
		return false;
	}

	AL2H_BaseCharacter* Partner = FindPartnerOf(Requester);
	if (!Partner)
	{
		L2HTime::LogImportant(TEXT("Request: refused -- there is no partner in this run to ask"));
		return false;
	}

	// One live request at a time: a second one would overwrite the first on the same screen, and the
	// partner would end up answering a question they never got to read.
	if (Partner->PendingRequest.bActive)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Request: refused -- %s already has an unanswered request"),
			*GetNameSafe(Partner)));
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// The cooldown is per asker, and it is the only brake on asking. The answer is the partner's to
	// give, so the point is not to ration generosity -- it is to stop a request button from mashing
	// the prompt until the partner answers on reflex.
	if (LastRequester.Get() == Requester && TimeConfig->RequestCooldown > 0.0f
		&& Now - LastRequestPostTime < TimeConfig->RequestCooldown)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Request: refused -- %s asked %.1fs ago and the cooldown is %.1fs"),
			*GetNameSafe(Requester), Now - LastRequestPostTime, TimeConfig->RequestCooldown));
		return false;
	}

	FL2H_TimeRequest Request;
	Request.Requester = Requester;
	Request.Amount = FMath::Max(0.0f, TimeConfig->TransferAmount);

	// What the partner would pay right now, tax included when one is due, so the answer is an informed one.
	Request.Cost = GetTransferCost(Partner);
	Request.ExpiresAtSeconds = Now + FMath::Max(1.0f, TimeConfig->RequestTimeoutSeconds);
	Request.bActive = true;

	Partner->SetPendingRequest(Request);

	PendingRequester = Requester;
	LastRequester = Requester;
	LastRequestPostTime = Now;

	GetWorldTimerManager().SetTimer(
		RequestLapseTimer, this, &AL2H_TimeDirector::ExpireTimeRequest,
		FMath::Max(1.0f, TimeConfig->RequestTimeoutSeconds), /*bLoop=*/false);

	L2HTime::LogImportant(FString::Printf(
		TEXT("Request: %s asked %s for %.1fs, costing the giver %.1fs"),
		*GetNameSafe(Requester), *GetNameSafe(Partner), Request.Amount, Request.Cost));

	OnTimeRequestChanged(Requester, Partner, /*bAccepted=*/false);

	return true;
}

bool AL2H_TimeDirector::ResolveTimeRequest(AL2H_BaseCharacter* Responder, bool bAccept)
{
	if (!HasAuthority() || !Responder)
	{
		return false;
	}

	// Only the pawn the question was put to may answer it, so nobody can accept on a partner's behalf
	// -- least of all the player who stands to gain the seconds.
	if (!Responder->PendingRequest.bActive
		|| Responder->PendingRequest.Requester != PendingRequester.Get())
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Request: refused -- %s has no live request of their own to answer"),
			*GetNameSafe(Responder)));
		return false;
	}

	const FL2H_TimeRequest Request = Responder->PendingRequest;

	// Cleared before the transfer, so a resolution can never be applied twice: the state that gates
	// this function is already gone by the time the effects are dispatched.
	Responder->ClearPendingRequest();
	PendingRequester = nullptr;
	GetWorldTimerManager().ClearTimer(RequestLapseTimer);

	AL2H_BaseCharacter* Requester = Request.Requester;

	if (!bAccept || !Requester)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("Request: %s declined %s"),
			*GetNameSafe(Responder), *GetNameSafe(Requester)));
		OnTimeRequestChanged(Requester, Responder, /*bAccepted=*/false);
		return false;
	}

	// The same packet a transfer is, through the same rule (tax, free window, revive threshold, and a
	// refusal with a reason when the responder cannot pay and keep a second).
	const bool bGiven = TransferTime(Responder, Requester);
	OnTimeRequestChanged(Requester, Responder, bGiven);
	return bGiven;
}

void AL2H_TimeDirector::ExpireTimeRequest()
{
	if (AL2H_BaseCharacter* Requester = PendingRequester.Get())
	{
		if (AL2H_BaseCharacter* Partner = FindPartnerOf(Requester))
		{
			if (Partner->PendingRequest.bActive)
			{
				Partner->ClearPendingRequest();

				L2HTime::LogImportant(FString::Printf(
					TEXT("Request: %s never answered, so %s's request lapsed"),
					*GetNameSafe(Partner), *GetNameSafe(Requester)));

				OnTimeRequestChanged(Requester, Partner, /*bAccepted=*/false);
			}
		}
	}

	PendingRequester = nullptr;
}

void AL2H_TimeDirector::ApplyTimeTo(AActor* Target, float TargetSeconds)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}

	const float Current = GetTimeFor(Target);
	if (Current < 0.0f)
	{
		// No TimeCore attribute set on this thing, so there is no number here to move.
		return;
	}

	const float Delta = TargetSeconds - Current;
	if (Delta > KINDA_SMALL_NUMBER)
	{
		ApplyTimeRestore(Target, Delta);
	}
	else if (Delta < -KINDA_SMALL_NUMBER)
	{
		ApplyTimeCost(Target, -Delta);
	}
}

// -----------------------------------------------------------------------------------------------
// Health
// -----------------------------------------------------------------------------------------------

void AL2H_TimeDirector::ApplyHealthDamage(AActor* Target, float Amount)
{
	if (!HasAuthority() || !Target || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	// Invulnerability is NOT re-checked here. ApplyTimeDamage already asked on behalf of every
	// gameplay caller, and a second check would be a second rule that could disagree with the first.
	if (!Config || !Config->HealthDamageEffect || Config->MaxHealth <= 0.0f)
	{
		return;
	}

	const float HealthBefore = GetHealthFor(Target);
	if (HealthBefore < 0.0f)
	{
		// No TimeCore attribute set on this thing, so there is no Health here to take.
		return;
	}

	// Behind cover and not exposed by aiming, a hit lands softened.
	if (const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(Target))
	{
		if (const UL2H_CoverComponent* Cover = Character->GetCoverComponent(); Cover && Cover->IsProtected() && Amount > 0.0f)
		{
			Amount *= Cover->DamageScale;
		}
	}

	if (HealthBefore <= 0.0f)
	{
		// Already dead. A corpse does not bleed twice, and the respawn the killing blow armed is
		// already on its way -- re-entering the death branch would only arm it a second time.
		return;
	}

	// A time shield takes the whole hit, so the hit costs neither Health nor seconds.
	if (AL2H_BaseCharacter* Shielded = Cast<AL2H_BaseCharacter>(Target); Shielded && Amount > 0.0f && Shielded->GetShieldHits() > 0)
	{
		Shielded->SetShieldHits(Shielded->GetShieldHits() - 1);
		L2HTime::LogImportant(FString::Printf(TEXT("Shield: %s blocked %.1f damage, %d hit(s) left"),
			*GetNameSafe(Target), Amount, Shielded->GetShieldHits()));
		return;
	}

	ApplyTimeEffect(Target, FMath::Abs(Amount), Config->HealthDamageEffect, GetHealthDamageMagnitudeName());

	// Measured rather than assumed. The attribute set clamps the loss against the current value, so a
	// 40-point hit on a player with 12 left costs 12 Health and -- crucially -- only 12 Health worth of
	// seconds. Charging the full 40 would make the killing blow cost more than the life it took.
	const float HealthAfter = GetHealthFor(Target);
	const float DamageTaken = FMath::Max(HealthBefore - HealthAfter, 0.0f);

	L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s took %.1f damage (%.1f -> %.1f hp)"),
		*GetNameSafe(Target), DamageTaken, HealthBefore, HealthAfter));

	if (DamageTaken <= 0.0f)
	{
		return;
	}

	// The translation lives here rather than in the attribute set because the ratio is level tuning,
	// and the attribute set has to stay ignorant of level config to remain reusable.
	const float TimeLost = DamageTaken * Config->TimePerHealthPoint;
	if (TimeLost > 0.0f)
	{
		ApplyTimedDamageInner(Target, TimeLost);
	}

	// The regeneration delay restarts on every hit, which is what stops a player under sustained fire
	// from healing straight through it.
	if (AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(Target))
	{
		Character->NotifyHealthDamaged();
	}

	if (HealthAfter <= KINDA_SMALL_NUMBER)
	{
		HandleDeath(Cast<AL2H_BaseCharacter>(Target));
	}
}

void AL2H_TimeDirector::ApplyHealthRestore(AActor* Target, float Amount)
{
	if (!HasAuthority() || !Target || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	if (!Config || !Config->HealthRestoreEffect)
	{
		return;
	}

	ApplyTimeEffect(Target, FMath::Abs(Amount), Config->HealthRestoreEffect, GetHealthRestoreMagnitudeName());
}

void AL2H_TimeDirector::RevivePawn(AL2H_BaseCharacter* Character, float HealthAmount, float TimeSeconds)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	// A body on someone's shoulders is put down first: standing it up while it is still attached would
	// leave a live player riding their partner's capsule.
	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		AL2H_BaseCharacter* Carrier = Weak.Get();
		if (Carrier && Carrier->GetCarriedActor() == Character)
		{
			Carrier->EndCarry();
		}
	}

	// The order is load-bearing twice over: Health first, so the pawn is never briefly alive at 0
	// Health, and the death flag cleared before the clock moves, so the Time crossing lands on a live
	// pawn instead of being swallowed by the dead early-out in HandleTimeChangedFor.
	ApplyHealthRestore(Character, HealthAmount);
	Character->SetDead(false);

	ApplyTimeTo(Character, TimeSeconds);

	// The crossing that would normally pose the pawn already happened while it was dead, and an
	// attribute that did not change value raises no event at all -- so the downed state is re-derived
	// here from the number now on the attribute rather than waited for.
	const bool bShouldBeDowned = GetTimeFor(Character) <= 0.0f;
	if (Character->IsDowned() != bShouldBeDowned)
	{
		if (UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character))
		{
			const FGameplayTag DownedTag = GetDownedTag();
			if (bShouldBeDowned)
			{
				if (DownedTag.IsValid())
				{
					ASC->AddLooseGameplayTag(DownedTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
				}
			}
			else
			{
				ClearDownedTag(ASC);
			}
		}

		Character->SetDowned(bShouldBeDowned);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("TimeDirector: %s revived (%.1f hp, %.1fs)"),
		*GetNameSafe(Character), GetHealthFor(Character), GetTimeFor(Character)));

	OnPlayerRevived(Character);

	// A respawn armed by the death that has just been undone must not fire on a live player.
	GetWorldTimerManager().ClearTimer(DeathRespawnTimer);
}

void AL2H_TimeDirector::HandleDeath(AL2H_BaseCharacter* Character)
{
	if (!HasAuthority() || !Character || Character->IsDead())
	{
		return;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("TimeDirector: %s ran out of Health -> Dead (waiting for a carry or a revive)"),
		*GetNameSafe(Character)));

	Character->SetDead(true);

	// Same cancellation the downed crossing does, for the same reason: an ability left running on a
	// body keeps spending a resource nobody can see or stop.
	if (UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character))
	{
		ASC->CancelAllAbilities();
	}

	OnPlayerDied(Character);

	// A death is a co-op state, not a restart: the partner can carry the body or revive it, so the run
	// only rewinds once nobody is left standing to do either. Rewinding on every death would make the
	// carry-and-revive half of the design unreachable.
	if (IsAnyPlayerStanding())
	{
		return;
	}

	L2HTime::LogImportant(FString::Printf(
		TEXT("TimeDirector: nobody is standing -- rewinding to the checkpoint in %.1fs"), DeathRespawnDelay));

	// Deferred for exactly the reason the all-downed rewind is: this runs inside an attribute-change
	// broadcast, and rewriting the world from there is re-entrant.
	if (DeathRespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(DeathRespawnTimer, this, &AL2H_TimeDirector::RespawnAfterDeath, DeathRespawnDelay, false);
	}
	else
	{
		DeathRespawnTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AL2H_TimeDirector::RespawnAfterDeath);
	}
}

bool AL2H_TimeDirector::IsAnyPlayerStanding() const
{
	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		const AL2H_BaseCharacter* Character = Weak.Get();
		if (Character && !Character->IsDowned() && !Character->IsDead())
		{
			return true;
		}
	}

	return false;
}

void AL2H_TimeDirector::ResetHealth(AL2H_BaseCharacter* Character)
{
	UAbilitySystemComponent* ASC = Character ? ResolveAbilitySystem(Character) : nullptr;
	if (!HasAuthority() || !ASC || !Config)
	{
		return;
	}

	// A base write like RegisterPawn's: a checkpoint return is a fresh start for the body, not a heal
	// that a modifier could later aggregate with.
	ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetMaxHealthAttribute(), Config->MaxHealth);
	ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetHealthAttribute(), Config->MaxHealth);
}

void AL2H_TimeDirector::SetHealthTo(AL2H_BaseCharacter* Character, float Value)
{
	UAbilitySystemComponent* ASC = Character ? ResolveAbilitySystem(Character) : nullptr;
	if (!HasAuthority() || !ASC || !Config || Config->MaxHealth <= 0.0f)
	{
		return;
	}

	ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetHealthAttribute(), FMath::Clamp(Value, 1.0f, Config->MaxHealth));
}

void AL2H_TimeDirector::RespawnAfterDeath()
{
	GetWorldTimerManager().ClearTimer(DeathRespawnTimer);

	// Death is a rewind like every other failure: the run goes back to the checkpoint it last wrote,
	// and only falls through to a full restart when the run never reached one.
	ReturnToCheckpointOrReset();
}

void AL2H_TimeDirector::ReturnToCheckpointOrReset()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ResetRunTimer);

	// A death that this rewind is already answering must not also fire its own, later.
	GetWorldTimerManager().ClearTimer(DeathRespawnTimer);

	// Falling is a rewind, not a restart. The checkpoint is the promise the run made when the players
	// last stood still, and the time recorded there is what they get back -- which is why the save
	// keeps the first arrival rather than the most recent visit. A run that never reached a
	// checkpoint has nothing to rewind to, and falls through to the old full restart.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (SaveSubsystem && SaveSubsystem->RestoreCheckpoint(this))
	{
		return;
	}

	ResetRun();
}

void AL2H_TimeDirector::ResetRun()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ResetRunTimer);

	// Carries end first, for the same reason the checkpoint restore does it: a body still attached to
	// its partner cannot be stood back up.
	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		if (AL2H_BaseCharacter* Character = Weak.Get())
		{
			Character->EndCarry();
		}
	}

	for (const TWeakObjectPtr<AL2H_BaseCharacter>& Weak : RegisteredPawns)
	{
		if (AL2H_BaseCharacter* Character = Weak.Get())
		{
			// A request that outlived the run it belonged to would sit on the partner's screen for a
			// world state that no longer exists, so it is cleared along with the timers and attributes.
			Character->ClearPendingRequest();
			ResetPawn(Character);
		}
	}

	GetWorldTimerManager().ClearTimer(RequestLapseTimer);
	PendingRequester = nullptr;

	L2HTime::LogImportant(TEXT("TimeDirector: run reset -- every player back to StartingTime"));
	OnRunReset();
}

void AL2H_TimeDirector::ResetPawn(AL2H_BaseCharacter* Character)
{
	UAbilitySystemComponent* ASC = ResolveAbilitySystem(Character);
	// GetSet() is const-only on UAbilitySystemComponent, and the attribute getters are const too.
	const UL2H_TimeAttributeSet* TimeSet = ASC ? ASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;
	if (!ASC || !TimeSet)
	{
		return;
	}

	// Clear the state before raising the attribute, so the change handler sees a standing pawn and
	// does not re-enter the downed branch on the way back up.
	ClearDownedTag(ASC);

	Character->SetDowned(false);

	// Death is cleared here too. A run that reset with a dead player still flagged would leave them
	// flagged with no respawn scheduled, since this reset is precisely what the death timer was
	// waiting to perform.
	Character->SetDead(false);

	// A full restart has no checkpoint to keep a bank from: it starts from nothing, like the clock does.
	Character->SetChrons(0.0f);
	Character->CloseHubBank();

	ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetTimeAttribute(),
		Config ? Config->StartingTime : TimeSet->GetMaxTime());

	// Health comes back with the clock, and for the same reason: whatever the pawn's Blueprint
	// authored, the level config is the source of truth for what a fresh start looks like.
	if (Config)
	{
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetMaxHealthAttribute(), Config->MaxHealth);
		ASC->SetNumericAttributeBase(UL2H_TimeAttributeSet::GetHealthAttribute(), Config->MaxHealth);
	}
}

// -----------------------------------------------------------------------------------------------
// Couch co-op
// -----------------------------------------------------------------------------------------------

void AL2H_TimeDirector::CreateSecondLocalPlayer()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_DedicatedServer || NetMode == NM_Client)
	{
		return;
	}

	int32 ExistingPlayers = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get())
		{
			++ExistingPlayers;
		}
	}

	// Exactly two: the design is a pair, and the test level ships two PlayerStarts to match.
	if (ExistingPlayers >= 2)
	{
		L2HTime::LogVerbose(TEXT("TimeDirector: a second local player is already present"));
		LogLocalPlayerBindings();
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();

	// Input is routed by DEVICE, not by index: UGameViewportClient::InputKey resolves the
	// FInputDeviceId to its owning platform user and then to the ULocalPlayer that holds that user.
	// A player created for a user that owns no device of its own is created successfully and then
	// receives nothing -- which is exactly what two-gamepad couch co-op looks like when it fails.
	//
	// On Windows the default InputPlatformSettings policy gives gamepad 0 to the primary user
	// (shared with the keyboard and mouse) and every later gamepad a user of its own, so the spare
	// device found below is the second physical pad.
	TArray<FInputDeviceId> ConnectedDevices;
	DeviceMapper.GetAllConnectedInputDevices(ConnectedDevices);

	const FInputDeviceId DefaultDevice = DeviceMapper.GetDefaultInputDevice();

	FPlatformUserId TargetUser = PLATFORMUSERID_NONE;

	for (const FInputDeviceId& Device : ConnectedDevices)
	{
		// The default device is the keyboard/mouse pair, which already serves player 1.
		if (Device == DefaultDevice)
		{
			continue;
		}

		const FPlatformUserId DeviceUser = DeviceMapper.GetUserForInputDevice(Device);
		if (!DeviceUser.IsValid())
		{
			continue;
		}

		// Already claimed by player 1, so this device is not spare.
		if (GameInstance && GameInstance->FindLocalPlayerFromPlatformUserId(DeviceUser))
		{
			continue;
		}

		TargetUser = DeviceUser;
		break;
	}

	// Preferred path: build the player for the user that actually owns the spare pad.
	// Fallback: let the engine take the next free local index, which is what keeps
	// keyboard-and-keyboard co-op working when no spare pad is connected at all.
	APlayerController* PC = TargetUser.IsValid()
		? UGameplayStatics::CreatePlayerFromPlatformUser(this, TargetUser, true)
		: UGameplayStatics::CreatePlayer(this, -1, true);

	if (PC)
	{
		CreatedLocalPlayer = PC->GetLocalPlayer();

		// No spare pad: the only gamepad is shared with the keyboard by player 1, so player 2 owns no
		// device and receives nothing. Hand the gamepad to player 2 (keyboard and mouse stay with
		// player 1) -- the engine's "skip assigning gamepad to player 1" remap, switched on for this run.
		if (!TargetUser.IsValid())
		{
			UGameMapsSettings* MapsSettings = GetMutableDefault<UGameMapsSettings>();
			if (!MapsSettings->bOffsetPlayerGamepadIds)
			{
				MapsSettings->bOffsetPlayerGamepadIds = true;
				bOffsetGamepadsForCouch = true;
				L2HTime::LogImportant(TEXT("TimeDirector: one gamepad -- it now drives player 2, keyboard and mouse drive player 1"));
			}
		}

		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeDirector: created %s for platform user %d (%s)"),
			*GetNameSafe(PC),
			TargetUser.GetInternalId(),
			TargetUser.IsValid() ? TEXT("spare device") : TEXT("next free index")));
	}
	else
	{
		L2HTime::LogImportant(TEXT("TimeDirector: could not create a second local player"));
	}

	LogLocalPlayerBindings();
}

void AL2H_TimeDirector::LogLocalPlayerBindings() const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInstance)
	{
		return;
	}

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();

	// One line per local player: this is the pad -> player binding, stated explicitly. If the
	// second pad is not driving the second character, the device columns here say why.
	const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
	for (int32 Index = 0; Index < LocalPlayers.Num(); ++Index)
	{
		const ULocalPlayer* LocalPlayer = LocalPlayers[Index];
		if (!LocalPlayer)
		{
			continue;
		}

		const FPlatformUserId User = LocalPlayer->GetPlatformUserId();
		const FInputDeviceId PrimaryDevice = DeviceMapper.GetPrimaryInputDeviceForUser(User);

		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeDirector: local player %d | controller id %d | platform user %d | primary device %d"),
			Index,
			LocalPlayer->GetControllerId(),
			User.GetInternalId(),
			PrimaryDevice.GetId()));
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		L2HTime::LogImportant(FString::Printf(
			TEXT("TimeDirector: controller %s | platform user %d | pawn %s"),
			*GetNameSafe(PC),
			PC->GetPlatformUserId().GetInternalId(),
			*GetNameSafe(PC->GetPawn())));
	}
}
