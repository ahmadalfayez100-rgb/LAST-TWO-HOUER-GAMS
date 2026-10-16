// Copyright Epic Games, Inc. All Rights Reserved.

// The bag on the clock face: the part of UL2H_InventoryComponent that draws it and acts on it. Every
// action goes out through the component's own Request* calls, so the server still decides.

#include "Item/L2H_InventoryComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Item/L2H_ItemDataAsset.h"
#include "Time/L2HTimeLog.h"
#include "UI/L2H_ClockMenuWidget.h"

#define LOCTEXT_NAMESPACE "L2HClockBag"

namespace
{
	const FName BagPageId(TEXT("Bag"));

	FL2H_ClockOption BagOption(const FString& Id, const FText& Label, const FText& Value = FText::GetEmpty(), bool bEnabled = true, bool bDanger = false)
	{
		FL2H_ClockOption Option;
		Option.Id = *Id;
		Option.Label = Label;
		Option.Value = Value;
		Option.bEnabled = bEnabled;
		Option.bDanger = bDanger;
		return Option;
	}

	/** "Prefix:A" or "Prefix:A:B" -- the numbers after the prefix. */
	bool ParseId(FName Id, const TCHAR* Prefix, int32& OutA, int32* OutB = nullptr)
	{
		TArray<FString> Parts;
		Id.ToString().ParseIntoArray(Parts, TEXT(":"));
		if (Parts.Num() < 2 || Parts[0] != Prefix || (OutB && Parts.Num() < 3))
		{
			return false;
		}
		OutA = FCString::Atoi(*Parts[1]);
		if (OutB)
		{
			*OutB = FCString::Atoi(*Parts[2]);
		}
		return true;
	}
}

void UL2H_InventoryComponent::OpenClockBag()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !Pawn->IsLocallyControlled() || ClockBag)
	{
		return;
	}
	ClockBag = CreateWidget<UL2H_ClockMenuWidget>(PC, UL2H_ClockMenuWidget::StaticClass());
	if (!ClockBag)
	{
		return;
	}
	ClockBag->SetTimeSource(Pawn);
	ClockBag->SetBackdropOpacity(0.35f);
	ClockBag->OnChosen.BindUObject(this, &UL2H_InventoryComponent::OnClockBagChosen);
	ClockBag->OnBack.BindUObject(this, &UL2H_InventoryComponent::OnClockBagBack);
	ClockBag->AddToPlayerScreen(40);

	ClockBagStack.Reset();
	ClockBagStack.Add(BagPageId);
	ClockBag->ShowPage(BuildBagPage(BagPageId));

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(ClockBag->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	ClockBag->TakeFocus();

	// The component does not tick; the open bag refreshes itself on the core ticker instead.
	ClockBagTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UL2H_InventoryComponent::TickClockBag), 0.25f);
	L2HTime::LogImportant(FString::Printf(TEXT("Inventory clock opened for %s"), *GetNameSafe(GetOwner())));
}

void UL2H_InventoryComponent::CloseClockBag()
{
	if (ClockBagTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ClockBagTicker);
		ClockBagTicker.Reset();
	}
	if (!ClockBag)
	{
		return;
	}
	ClockBag->RemoveFromParent();
	ClockBag = nullptr;
	ClockBagStack.Reset();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

bool UL2H_InventoryComponent::TickClockBag(float DeltaTime)
{
	if (!ClockBag)
	{
		return false;
	}
	if (ClockBag->GetTransition() < 1.0f)
	{
		return true;
	}
	// An item that has gone (used up, dropped, combined away) takes its face with it.
	int32 Cell = 0;
	const FName Current = ClockBag->GetPageId();
	const bool bItemFace = ParseId(Current, TEXT("Item"), Cell) || ParseId(Current, TEXT("Short"), Cell) || ParseId(Current, TEXT("Comb"), Cell);
	if (bItemFace && !(Slots.IsValidIndex(Cell) && Slots[Cell].IsSet()))
	{
		ClockBagStack.Reset();
		ClockBagStack.Add(BagPageId);
		ClockBag->ShowPage(BuildBagPage(BagPageId), false);
		return true;
	}
	ClockBag->RefreshPage(BuildBagPage(Current));
	return true;
}

FL2H_ClockPage UL2H_InventoryComponent::BuildBagPage(FName PageId) const
{
	FL2H_ClockPage Page;
	Page.Id = PageId;
	int32 Cell = 0;

	if (ParseId(PageId, TEXT("Item"), Cell) && Slots.IsValidIndex(Cell) && Slots[Cell].IsSet())
	{
		const FL2H_ItemInstance& Item = Slots[Cell];
		const FL2H_ItemBaseData& Data = Item.Data->Data;
		Page.Title = Data.DisplayName;
		if (Data.bUsable || Data.UseEffect != EL2H_ItemUseEffect::None)
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Use:%d"), Cell), LOCTEXT("Use", "Use"),
				Item.Quantity > 1 ? FText::Format(LOCTEXT("Count", "x{0}"), FText::AsNumber(Item.Quantity)) : FText::GetEmpty()));
		}
		if (Data.bEquipable)
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Open:Short:%d"), Cell), LOCTEXT("Shortcut", "Shortcut")));
		}
		if (Data.bCombinable)
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Open:Comb:%d"), Cell), LOCTEXT("Combine", "Combine")));
		}
		if (Data.bDiscardable)
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Drop:%d"), Cell), LOCTEXT("Drop", "Drop"), FText::GetEmpty(), true, true));
		}
		Page.Options.Add(BagOption(TEXT("Back"), LOCTEXT("Back", "Back")));
		return Page;
	}

	if (ParseId(PageId, TEXT("Short"), Cell) && Slots.IsValidIndex(Cell))
	{
		Page.Title = LOCTEXT("ShortcutTitle", "SHORTCUT");
		for (int32 Index = 0; Index < ShortcutCount; ++Index)
		{
			const FL2H_ItemInstance Current = Shortcuts.IsValidIndex(Index) ? Shortcuts[Index] : FL2H_ItemInstance();
			Page.Options.Add(BagOption(FString::Printf(TEXT("Assign:%d:%d"), Cell, Index),
				FText::Format(LOCTEXT("Slot", "Slot {0}"), FText::AsNumber(Index + 1)),
				Current.IsSet() ? Current.Data->Data.DisplayName : LOCTEXT("Empty", "empty")));
		}
		Page.Options.Add(BagOption(TEXT("Back"), LOCTEXT("Back", "Back")));
		return Page;
	}

	if (ParseId(PageId, TEXT("Comb"), Cell) && Slots.IsValidIndex(Cell) && Slots[Cell].IsSet())
	{
		Page.Title = LOCTEXT("CombineTitle", "COMBINE WITH");
		for (int32 Other = 0; Other < Slots.Num() && Page.Options.Num() < 10; ++Other)
		{
			if (Other != Cell && Slots[Other].IsSet() && !Slots[Other].bSecondaryHalf && FindRecipe(Slots[Cell].Data, Slots[Other].Data))
			{
				Page.Options.Add(BagOption(FString::Printf(TEXT("With:%d:%d"), Cell, Other), Slots[Other].Data->Data.DisplayName));
			}
		}
		if (Page.Options.Num() == 0)
		{
			Page.Options.Add(BagOption(TEXT("None"), LOCTEXT("NothingCombines", "Nothing fits"), FText::GetEmpty(), false));
		}
		Page.Options.Add(BagOption(TEXT("Back"), LOCTEXT("Back", "Back")));
		return Page;
	}

	// The bag: every cell at its hour, full or empty.
	int32 Used = 0;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FL2H_ItemInstance& Item = Slots[Index];
		if (Item.IsSet() && !Item.bSecondaryHalf)
		{
			++Used;
			Page.Options.Add(BagOption(FString::Printf(TEXT("Open:Item:%d"), Index), Item.Data->Data.DisplayName,
				Item.Quantity > 1 ? FText::Format(LOCTEXT("Count", "x{0}"), FText::AsNumber(Item.Quantity)) : FText::GetEmpty()));
		}
		else if (Item.bSecondaryHalf)
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Half:%d"), Index), LOCTEXT("SecondHalf", "..."), FText::GetEmpty(), false));
		}
		else
		{
			Page.Options.Add(BagOption(FString::Printf(TEXT("Empty:%d"), Index), LOCTEXT("EmptyCell", "empty"), FText::GetEmpty(), false));
		}
	}
	Page.Id = BagPageId;
	Page.Title = FText::Format(LOCTEXT("BagTitle", "BAG  {0}/{1}"), FText::AsNumber(Used), FText::AsNumber(Slots.Num()));
	return Page;
}

void UL2H_InventoryComponent::OnClockBagBack()
{
	if (!ClockBag)
	{
		return;
	}
	if (ClockBagStack.Num() <= 1)
	{
		CloseClockBag();
		return;
	}
	ClockBagStack.Pop();
	ClockBag->ShowPage(BuildBagPage(ClockBagStack.Last()), /*bForward=*/false);
}

void UL2H_InventoryComponent::OnClockBagChosen(FName Id)
{
	if (!ClockBag)
	{
		return;
	}
	const FString Text = Id.ToString();
	if (Id == TEXT("Back"))
	{
		OnClockBagBack();
		return;
	}
	if (Text.StartsWith(TEXT("Open:")))
	{
		const FName PageId = *Text.RightChop(5);
		ClockBagStack.Add(PageId);
		ClockBag->ShowPage(BuildBagPage(PageId), /*bForward=*/true);
		int32 Cell = 0;
		if (ParseId(PageId, TEXT("Item"), Cell) && Slots.IsValidIndex(Cell) && Slots[Cell].IsSet())
		{
			// What the thing is, under the clock while its face is up.
			ClockBag->SetStatus(Slots[Cell].Data->Data.Description, 6.0f);
		}
		return;
	}

	int32 A = 0;
	int32 B = 0;
	const FText Name = [this, &Text]()
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(":"));
		const int32 Cell = Parts.Num() > 1 ? FCString::Atoi(*Parts[1]) : -1;
		return Slots.IsValidIndex(Cell) && Slots[Cell].IsSet() ? Slots[Cell].Data->Data.DisplayName : FText::GetEmpty();
	}();

	if (ParseId(Id, TEXT("Use"), A))
	{
		RequestUse(A);
		ClockBag->SetStatus(FText::Format(LOCTEXT("Used", "Used {0}."), Name));
	}
	else if (ParseId(Id, TEXT("Drop"), A))
	{
		RequestDrop(A);
		ClockBag->SetStatus(FText::Format(LOCTEXT("Dropped", "Dropped {0}."), Name));
	}
	else if (ParseId(Id, TEXT("Assign"), A, &B))
	{
		RequestAssignShortcut(A, B);
		ClockBag->SetStatus(FText::Format(LOCTEXT("Assigned", "{0} on shortcut {1}."), Name, FText::AsNumber(B + 1)));
		OnClockBagBack();
	}
	else if (ParseId(Id, TEXT("With"), A, &B))
	{
		RequestCombine(A, B);
		ClockBag->SetStatus(FText::Format(LOCTEXT("Combined", "Combined {0}."), Name));
		// Both may be gone now: back to the bag.
		ClockBagStack.Reset();
		ClockBagStack.Add(BagPageId);
		ClockBag->ShowPage(BuildBagPage(BagPageId), false);
	}
}

#undef LOCTEXT_NAMESPACE
