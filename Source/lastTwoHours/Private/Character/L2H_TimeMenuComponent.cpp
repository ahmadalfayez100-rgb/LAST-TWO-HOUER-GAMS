// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_TimeMenuComponent.h"

#include "Character/L2H_BaseCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Item/L2H_InventoryComponent.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Save/L2H_SaveSubsystem.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"
#include "Weapon/L2H_WeaponUpgradeDataAsset.h"
#include "World/L2H_CheckpointTerminal.h"

#define LOCTEXT_NAMESPACE "L2HTimeMenu"

namespace
{
	FL2H_ClockOption MakeOption(FName Id, const FText& Label, const FText& Value = FText::GetEmpty(), bool bEnabled = true, bool bDanger = false)
	{
		FL2H_ClockOption Option;
		Option.Id = Id;
		Option.Label = Label;
		Option.Value = Value;
		Option.bEnabled = bEnabled;
		Option.bDanger = bDanger;
		return Option;
	}

	FText Seconds(float Value)
	{
		return FText::Format(LOCTEXT("Seconds", "{0} s"), FText::AsNumber(FMath::RoundToInt(Value)));
	}

	/** "Prefix:Rest" option ids carry an argument. */
	bool SplitId(FName Id, const TCHAR* Prefix, FString& OutRest)
	{
		const FString Text = Id.ToString();
		const FString Head = FString(Prefix) + TEXT(":");
		if (!Text.StartsWith(Head))
		{
			return false;
		}
		OutRest = Text.RightChop(Head.Len());
		return true;
	}

	const FName RootId(TEXT("Root"));
}

UL2H_TimeMenuComponent::UL2H_TimeMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UL2H_TimeMenuComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Close();
	Super::EndPlay(EndPlayReason);
}

AL2H_BaseCharacter* UL2H_TimeMenuComponent::GetCharacter() const
{
	return Cast<AL2H_BaseCharacter>(GetOwner());
}

bool UL2H_TimeMenuComponent::AtTerminal() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	return Character && Character->GetCurrentTerminal() && Character->GetCurrentTerminal()->OffersMenu();
}

bool UL2H_TimeMenuComponent::AtHub() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	return Character && Character->GetCurrentTerminal() && Character->GetCurrentTerminal()->SavesOnArrival();
}

void UL2H_TimeMenuComponent::Toggle()
{
	if (Menu)
	{
		Close();
	}
	else
	{
		Open();
	}
}

bool UL2H_TimeMenuComponent::Open()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC || !Character->IsLocallyControlled())
	{
		return false;
	}
	if (Menu)
	{
		return true;
	}
	Menu = CreateWidget<UL2H_ClockMenuWidget>(PC, UL2H_ClockMenuWidget::StaticClass());
	if (!Menu)
	{
		return false;
	}
	Menu->SetTimeSource(Character);
	Menu->SetBackdropOpacity(0.35f);
	Menu->OnChosen.BindUObject(this, &UL2H_TimeMenuComponent::OnChosen);
	Menu->OnBack.BindUObject(this, &UL2H_TimeMenuComponent::OnBack);
	Menu->AddToPlayerScreen(40);

	PageStack.Reset();
	PageStack.Add(RootId);
	Menu->ShowPage(RootPage());

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	Menu->TakeFocus();
	return true;
}

void UL2H_TimeMenuComponent::Close()
{
	if (!Menu)
	{
		return;
	}
	Menu->RemoveFromParent();
	Menu = nullptr;
	PageStack.Reset();
	const AL2H_BaseCharacter* Character = GetCharacter();
	if (APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void UL2H_TimeMenuComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Values move while the menu is open (the bank after a purchase, a request arriving), so the face on
	// screen is rebuilt in place twice a second. A terminal left behind closes its faces.
	const UWorld* World = GetWorld();
	if (!Menu || !World || World->GetRealTimeSeconds() < NextRefresh)
	{
		return;
	}
	NextRefresh = World->GetRealTimeSeconds() + 0.5f;
	const FName Current = Menu->GetPageId();
	if (Current != RootId && Current != TEXT("Time") && Current != TEXT("Tools") && !AtTerminal())
	{
		PageStack.Reset();
		PageStack.Add(RootId);
		Menu->ShowPage(RootPage(), false);
		return;
	}
	if (Menu->GetTransition() >= 1.0f)
	{
		Menu->RefreshPage(BuildPage(Current));
	}
}

FL2H_ClockPage UL2H_TimeMenuComponent::BuildPage(FName PageId) const
{
	if (PageId == TEXT("Time")) return TimePage();
	if (PageId == TEXT("Shop")) return ShopPage();
	if (PageId == TEXT("Upgrades")) return UpgradesPage();
	if (PageId.ToString().StartsWith(TEXT("Upg:"))) return WeaponUpgradesPage(PageId);
	if (PageId == TEXT("Tools")) return ToolsPage();
	if (PageId == TEXT("Bank")) return BankPage();
	if (PageId == TEXT("Checkpoint")) return CheckpointPage();
	if (PageId == TEXT("Rewind")) return RewindPage();
	return RootPage();
}

FL2H_ClockPage UL2H_TimeMenuComponent::RootPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	FL2H_ClockPage Page;
	Page.Id = RootId;

	if (!AtTerminal())
	{
		// Away from a checkpoint the root is the time face itself.
		Page = TimePage();
		Page.Id = RootId;
		Page.Options.Add(MakeOption(TEXT("Open_Tools"), LOCTEXT("Tools", "Tools")));
		Page.Options.Add(MakeOption(TEXT("Close"), LOCTEXT("Close", "Close")));
		return Page;
	}

	const AL2H_CheckpointTerminal* Terminal = Character->GetCurrentTerminal();
	Page.Title = Terminal && !Terminal->DisplayName.IsEmpty() ? Terminal->DisplayName : LOCTEXT("Checkpoint", "CHECKPOINT");
	const bool bHub = AtHub();
	const FText Bank = FText::Format(LOCTEXT("BankValue", "bank {0}"), FText::AsNumber(FMath::RoundToInt(Character->GetChrons())));

	Page.Options.Add(MakeOption(TEXT("Open_Time"), LOCTEXT("TimeFace", "Time"),
		Character->PendingRequest.bActive ? LOCTEXT("RequestWaiting", "a request waits") : FText::GetEmpty()));
	if (bHub)
	{
		Page.Options.Add(MakeOption(TEXT("Open_Shop"), LOCTEXT("Shop", "Shop"), Bank));
		Page.Options.Add(MakeOption(TEXT("Open_Upgrades"), LOCTEXT("Upgrades", "Upgrades"), Bank));
	}
	Page.Options.Add(MakeOption(TEXT("Open_Tools"), LOCTEXT("Tools", "Tools")));
	if (Character->bHubBankOpen)
	{
		Page.Options.Add(MakeOption(TEXT("Open_Bank"), LOCTEXT("BankFace", "Bank"),
			FText::Format(LOCTEXT("Carrying", "carry {0}/{1}"), FText::AsNumber(FMath::RoundToInt(Character->GetHubCarried())),
				FText::AsNumber(FMath::RoundToInt(Character->GetHubCarryAllowance())))));
	}
	Page.Options.Add(MakeOption(TEXT("Open_Checkpoint"), LOCTEXT("CheckpointFace", "Checkpoint"), LOCTEXT("SaveRewind", "save / rewind")));
	Page.Options.Add(MakeOption(TEXT("Close"), LOCTEXT("Close", "Close")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::TimePage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	const AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(GetOwner());
	const float Gift = Config ? Config->TransferAmount : 0.0f;
	const float Cost = (Director && Character) ? Director->GetTransferCost(Character) : Gift;

	FL2H_ClockPage Page;
	Page.Id = TEXT("Time");
	Page.Title = LOCTEXT("TimeTitle", "TIME");
	if (!Character)
	{
		return Page;
	}

	// A waiting request comes first: it is the likely reason the menu was opened.
	if (Character->PendingRequest.bActive)
	{
		const FText Asked = FText::Format(LOCTEXT("Asked", "asks {0}"), Seconds(Character->PendingRequest.Amount));
		Page.Options.Add(MakeOption(TEXT("Accept"), LOCTEXT("Accept", "Accept"), Asked));
		Page.Options.Add(MakeOption(TEXT("Decline"), LOCTEXT("Decline", "Decline"), FText::GetEmpty(), true, true));
	}
	Page.Options.Add(MakeOption(TEXT("Give"), FText::Format(LOCTEXT("Give", "Give {0}"), Seconds(Gift)),
		FText::Format(LOCTEXT("Costs", "costs {0}"), Seconds(Cost))));
	Page.Options.Add(MakeOption(TEXT("Ask"), FText::Format(LOCTEXT("Ask", "Ask for {0}"), Seconds(Gift))));

	// At a hub, a partner who is down elsewhere can be brought here.
	if (AtHub() && Config && GetWorld())
	{
		for (TActorIterator<AL2H_BaseCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Character && (It->IsDead() || It->IsDowned()))
			{
				Page.Options.Add(MakeOption(TEXT("ReviveHere"), LOCTEXT("ReviveHere", "Bring partner"),
					FText::Format(LOCTEXT("Price", "{0} from bank"), FText::AsNumber(FMath::RoundToInt(Config->HubRevivePrice)))));
				break;
			}
		}
	}
	if (PageStack.Num() > 1 || AtTerminal())
	{
		Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	}
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::ShopPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	FL2H_ClockPage Page;
	Page.Id = TEXT("Shop");
	Page.Title = FText::Format(LOCTEXT("ShopTitle", "SHOP  bank {0}"), FText::AsNumber(FMath::RoundToInt(Character ? Character->GetChrons() : 0.0f)));
	if (Config && Character)
	{
		for (int32 Index = 0; Index < Config->ShopItems.Num(); ++Index)
		{
			const UL2H_ItemDataAsset* Item = Config->ShopItems[Index];
			if (!Item || Cast<UL2H_WeaponUpgradeDataAsset>(Item))
			{
				continue;
			}
			const int32 Price = UL2H_WeaponUpgradeDataAsset::GetShopPrice(Character, Item);
			Page.Options.Add(MakeOption(*FString::Printf(TEXT("Buy:%d"), Index), Item->Data.DisplayName,
				Seconds(Price), Character->GetChrons() >= Price));
		}
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::UpgradesPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	const UL2H_WeaponComponent* Weapons = Character ? Character->FindComponentByClass<UL2H_WeaponComponent>() : nullptr;
	FL2H_ClockPage Page;
	Page.Id = TEXT("Upgrades");
	Page.Title = FText::Format(LOCTEXT("UpgradesTitle", "UPGRADES  bank {0}"), FText::AsNumber(FMath::RoundToInt(Character ? Character->GetChrons() : 0.0f)));

	// One choice per weapon the player carries that has upgrades on sale.
	TArray<const UL2H_WeaponDataAsset*> Seen;
	if (Config && Weapons)
	{
		for (const TObjectPtr<UL2H_ItemDataAsset>& Item : Config->ShopItems)
		{
			const UL2H_WeaponUpgradeDataAsset* Upgrade = Cast<UL2H_WeaponUpgradeDataAsset>(Item);
			if (Upgrade && Upgrade->Weapon && Weapons->HasWeapon(Upgrade->Weapon) && !Seen.Contains(Upgrade->Weapon))
			{
				Seen.Add(Upgrade->Weapon);
				Page.Options.Add(MakeOption(*FString::Printf(TEXT("Open_Upg:%s"), *Upgrade->Weapon->GetName()), Upgrade->Weapon->Data.DisplayName));
			}
		}
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::WeaponUpgradesPage(FName PageId) const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	FString WeaponName;
	SplitId(PageId, TEXT("Upg"), WeaponName);

	static const FText StatNames[] = {
		LOCTEXT("Firepower", "Firepower"), LOCTEXT("Capacity", "Capacity"), LOCTEXT("FireRate", "Fire rate"), LOCTEXT("RoundPrice", "Cheaper rounds") };

	FL2H_ClockPage Page;
	Page.Id = PageId;
	if (Config && Character)
	{
		for (int32 Index = 0; Index < Config->ShopItems.Num(); ++Index)
		{
			const UL2H_WeaponUpgradeDataAsset* Upgrade = Cast<UL2H_WeaponUpgradeDataAsset>(Config->ShopItems[Index]);
			if (!Upgrade || !Upgrade->Weapon || Upgrade->Weapon->GetName() != WeaponName)
			{
				continue;
			}
			Page.Title = FText::Format(LOCTEXT("WeaponTitle", "{0}  bank {1}"), Upgrade->Weapon->Data.DisplayName,
				FText::AsNumber(FMath::RoundToInt(Character->GetChrons())));
			const int32 StatIndex = FMath::Clamp(static_cast<int32>(Upgrade->Stat), 0, UE_ARRAY_COUNT(StatNames) - 1);
			const int32 Level = Upgrade->GetLevelFor(Character);
			const int32 Price = Upgrade->GetPriceFor(Character);
			const FText Value = Price < 0
				? FText::Format(LOCTEXT("MaxLevel", "{0}/{0} max"), FText::AsNumber(Upgrade->MaxLevel))
				: FText::Format(LOCTEXT("LevelPrice", "{0}/{1}  {2}"), FText::AsNumber(Level), FText::AsNumber(Upgrade->MaxLevel), Seconds(Price));
			Page.Options.Add(MakeOption(*FString::Printf(TEXT("Buy:%d"), Index), StatNames[StatIndex], Value,
				Price >= 0 && Character->GetChrons() >= Price));
		}
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::ToolsPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_InventoryComponent* Bag = Character ? Character->FindComponentByClass<UL2H_InventoryComponent>() : nullptr;
	FL2H_ClockPage Page;
	Page.Id = TEXT("Tools");
	Page.Title = LOCTEXT("ToolsTitle", "TOOLS");
	if (Bag)
	{
		const TArray<FL2H_ItemInstance> Slots = Bag->GetSlots();
		for (int32 Cell = 0; Cell < Slots.Num() && Page.Options.Num() < 9; ++Cell)
		{
			if (Slots[Cell].IsSet() && Slots[Cell].Data->Data.UseEffect != EL2H_ItemUseEffect::None)
			{
				Page.Options.Add(MakeOption(*FString::Printf(TEXT("Use:%d"), Cell), Slots[Cell].Data->Data.DisplayName,
					FText::Format(LOCTEXT("Count", "x{0}"), FText::AsNumber(Slots[Cell].Quantity))));
			}
		}
	}
	if (Page.Options.Num() == 0)
	{
		Page.Options.Add(MakeOption(TEXT("None"), LOCTEXT("NoTools", "No tools"), LOCTEXT("BuyAtHub", "buy at a hub"), false));
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::BankPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	const float Step = Config ? Config->CarryStep : 15.0f;
	FL2H_ClockPage Page;
	Page.Id = TEXT("Bank");
	Page.Title = FText::Format(LOCTEXT("BankTitle", "BANK {0}"), FText::AsNumber(FMath::RoundToInt(Character ? Character->GetChrons() : 0.0f)));
	if (Character)
	{
		const FText Carrying = FText::Format(LOCTEXT("CarryingNow", "carrying {0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(Character->GetHubCarried())), FText::AsNumber(FMath::RoundToInt(Character->GetHubCarryAllowance())));
		Page.Options.Add(MakeOption(TEXT("CarryMore"), FText::Format(LOCTEXT("CarryMore", "Carry +{0}"), Seconds(Step)), Carrying));
		Page.Options.Add(MakeOption(TEXT("CarryLess"), FText::Format(LOCTEXT("CarryLess", "Bank {0} back"), Seconds(Step)), Carrying));
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::CheckpointPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
	FL2H_ClockPage Page;
	Page.Id = TEXT("Checkpoint");
	Page.Title = LOCTEXT("CheckpointTitle", "CHECKPOINT");
	Page.Options.Add(MakeOption(TEXT("SaveHere"), LOCTEXT("SaveHere", "Record my time"), Seconds(Config ? Config->SaveCostSeconds : 0.0f)));
	Page.Options.Add(MakeOption(TEXT("Open_Rewind"), LOCTEXT("Rewind", "Rewind"), LOCTEXT("EarlierCheckpoint", "earlier checkpoint"), RewindPage().Options.Num() > 1));
	if (Config && Character && Config->DesignerTime > 0.0f)
	{
		Page.Options.Add(MakeOption(TEXT("Designer"), LOCTEXT("Designer", "Designer time"), Seconds(Config->DesignerTime),
			AL2H_TimeDirector::GetTimeFor(Character) < Config->DesignerTime));
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

FL2H_ClockPage UL2H_TimeMenuComponent::RewindPage() const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	FL2H_ClockPage Page;
	Page.Id = TEXT("Rewind");
	Page.Title = LOCTEXT("RewindTitle", "REWIND TO");
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr)
	{
		const FName Standing = (Character && Character->GetCurrentTerminal()) ? Character->GetCurrentTerminal()->GetCheckpointId() : NAME_None;
		for (const FL2H_CheckpointRecord& Record : Save->GetTravelOptions(GetOwner(), Standing))
		{
			if (Page.Options.Num() >= 9)
			{
				break;
			}
			Page.Options.Add(MakeOption(*FString::Printf(TEXT("Rewind:%s"), *Record.Id.ToString()),
				Record.DisplayName.IsEmpty() ? FText::FromName(Record.Id) : Record.DisplayName, FText::GetEmpty(), true, true));
		}
	}
	Page.Options.Add(MakeOption(TEXT("Back"), LOCTEXT("Back", "Back")));
	return Page;
}

void UL2H_TimeMenuComponent::GoTo(FName PageId)
{
	if (!Menu)
	{
		return;
	}
	PageStack.Add(PageId);
	Menu->ShowPage(BuildPage(PageId), /*bForward=*/true);
}

void UL2H_TimeMenuComponent::OnBack()
{
	if (!Menu)
	{
		return;
	}
	if (PageStack.Num() <= 1)
	{
		Close();
		return;
	}
	PageStack.Pop();
	Menu->ShowPage(BuildPage(PageStack.Last()), /*bForward=*/false);
}

void UL2H_TimeMenuComponent::OnChosen(FName Id)
{
	AL2H_BaseCharacter* Character = GetCharacter();
	if (!Menu || !Character)
	{
		return;
	}
	FString Arg;

	if (Id == TEXT("Close"))
	{
		Close();
		return;
	}
	if (Id == TEXT("Back"))
	{
		OnBack();
		return;
	}
	if (SplitId(Id, TEXT("Open_Upg"), Arg))
	{
		GoTo(*FString::Printf(TEXT("Upg:%s"), *Arg));
		return;
	}
	if (Id.ToString().StartsWith(TEXT("Open_")))
	{
		GoTo(*Id.ToString().RightChop(5));
		return;
	}

	FText Status;
	if (Id == TEXT("Give"))
	{
		Character->ServerTryActivateAbilityByTag(Character->GetTransferAbilityTag());
		Status = LOCTEXT("GiveSent", "Time sent to your partner.");
	}
	else if (Id == TEXT("Ask"))
	{
		Character->ServerRequestTimeFromPartner();
		Status = LOCTEXT("AskSent", "Asked your partner for time.");
	}
	else if (Id == TEXT("Accept") || Id == TEXT("Decline"))
	{
		Character->ServerRespondToTimeRequest(Id == TEXT("Accept"));
		Status = Id == TEXT("Accept") ? LOCTEXT("Accepted", "Request accepted.") : LOCTEXT("Declined", "Request declined.");
	}
	else if (Id == TEXT("ReviveHere"))
	{
		Character->ServerBuyPartnerRevive();
		Status = LOCTEXT("ReviveSent", "Bringing your partner here.");
	}
	else if (Id == TEXT("CarryMore") || Id == TEXT("CarryLess"))
	{
		Character->ServerAdjustCarry(Id == TEXT("CarryMore") ? +1 : -1);
	}
	else if (Id == TEXT("SaveHere"))
	{
		Character->ServerRequestOptionalSave();
		Status = LOCTEXT("SaveSent", "Recording your time here.");
	}
	else if (Id == TEXT("Designer"))
	{
		Character->ServerAdoptDesignerTime();
		Status = LOCTEXT("DesignerSent", "Designer time requested.");
	}
	else if (SplitId(Id, TEXT("Rewind"), Arg))
	{
		Character->ServerTravelToCheckpoint(*Arg);
		Close();
		return;
	}
	else if (SplitId(Id, TEXT("Use"), Arg))
	{
		if (UL2H_InventoryComponent* Bag = Character->FindComponentByClass<UL2H_InventoryComponent>())
		{
			Bag->RequestUse(FCString::Atoi(*Arg));
		}
	}
	else if (SplitId(Id, TEXT("Buy"), Arg))
	{
		const int32 Index = FCString::Atoi(*Arg);
		const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(GetOwner());
		const UL2H_ItemDataAsset* Item = (Config && Config->ShopItems.IsValidIndex(Index)) ? Config->ShopItems[Index].Get() : nullptr;
		const int32 Price = UL2H_WeaponUpgradeDataAsset::GetShopPrice(Character, Item);
		if (!Item)
		{
			return;
		}
		if (Price < 0)
		{
			Status = LOCTEXT("UpgradeMax", "Already at the top level.");
		}
		else if (Character->GetChrons() < Price)
		{
			Status = FText::Format(LOCTEXT("BankShort", "Not enough in the bank ({0} of {1})."),
				FText::AsNumber(FMath::RoundToInt(Character->GetChrons())), FText::AsNumber(Price));
		}
		else
		{
			Character->ServerBuyShopItem(Index);
			Status = FText::Format(LOCTEXT("Bought", "Bought {0}."), Item->Data.DisplayName);
		}
	}

	if (!Status.IsEmpty())
	{
		Menu->SetStatus(Status);
	}
	// The values on the face change as the server answers; the tick picks that up, this shows the rest now.
	NextRefresh = 0.0f;
}

#undef LOCTEXT_NAMESPACE
