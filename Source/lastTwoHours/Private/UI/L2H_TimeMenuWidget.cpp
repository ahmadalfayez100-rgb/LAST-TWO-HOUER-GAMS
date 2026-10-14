// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_TimeMenuWidget.h"

#include "Character/L2H_BaseCharacter.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Save/L2H_SaveSubsystem.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2H_TimeDirector.h"
#include "Time/L2HTimeLog.h"
#include "World/L2H_CheckpointTerminal.h"

#define LOCTEXT_NAMESPACE "L2HTimeMenu"

void UL2H_TimeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BindWidget guarantees these exist -- a missing child is a Blueprint compile error, not a
	// runtime null -- so the guards here only protect against a designer renaming one later.
	if (TransferButton)
	{
		TransferButton->OnClicked.AddDynamic(this, &UL2H_TimeMenuWidget::HandleTransferClicked);
	}
	if (RequestButton)
	{
		RequestButton->OnClicked.AddDynamic(this, &UL2H_TimeMenuWidget::HandleRequestClicked);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UL2H_TimeMenuWidget::HandleSaveClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UL2H_TimeMenuWidget::HandleCloseClicked);
	}

	RefreshMenu();
}

void UL2H_TimeMenuWidget::SetTerminal(AL2H_CheckpointTerminal* InTerminal)
{
	Terminal = InTerminal;
	RefreshMenu();
}

AL2H_BaseCharacter* UL2H_TimeMenuWidget::GetOwningCharacter() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		return Cast<AL2H_BaseCharacter>(PC->GetPawn());
	}

	return nullptr;
}

AL2H_BaseCharacter* UL2H_TimeMenuWidget::ResolveActingCharacter()
{
	AL2H_BaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		LastResultText = LOCTEXT("MenuNoPawn", "No pawn to act for.");
		RefreshMenu();
		return nullptr;
	}

	if (!Character->IsLocallyControlled())
	{
		LastResultText = LOCTEXT("MenuNotLocal", "That pawn is not yours to command.");
		RefreshMenu();
		return nullptr;
	}

	return Character;
}

void UL2H_TimeMenuWidget::HandleTransferClicked()
{
	AL2H_BaseCharacter* Character = ResolveActingCharacter();
	if (!Character)
	{
		return;
	}

	// The same route the dedicated key takes: the tag names the ability and the component decides
	// whether it may start, so an ungranted ability is a logged no-op rather than a bad state.
	Character->ServerTryActivateAbilityByTag(Character->GetTransferAbilityTag());

	LastResultText = LOCTEXT("MenuTransferSent", "Transfer sent.");
	RefreshMenu();
}

void UL2H_TimeMenuWidget::HandleRequestClicked()
{
	AL2H_BaseCharacter* Character = ResolveActingCharacter();
	if (!Character)
	{
		return;
	}

	Character->ServerRequestTimeFromPartner();

	LastResultText = LOCTEXT("MenuRequestSent", "Request sent to your partner.");
	RefreshMenu();
}

void UL2H_TimeMenuWidget::HandleSaveClicked()
{
	AL2H_BaseCharacter* Character = ResolveActingCharacter();
	if (!Character)
	{
		return;
	}

	Character->ServerRequestOptionalSave();

	LastResultText = LOCTEXT("MenuSaveSent", "Save requested.");
	RefreshMenu();
}

void UL2H_TimeMenuWidget::HandleCloseClicked()
{
	// The pawn owns the input mode, so it has to be the one to hand input back to the game.
	if (AL2H_BaseCharacter* Character = GetOwningCharacter())
	{
		Character->CloseTerminalMenu();
	}
}

void UL2H_TimeMenuWidget::RefreshMenu()
{
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);

	const FText Title = (Terminal && !Terminal->DisplayName.IsEmpty())
		? Terminal->DisplayName
		: LOCTEXT("MenuTitleFallback", "Checkpoint");

	const float Gift = Config ? Config->TransferAmount : 0.0f;
	const float SaveCost = Config ? Config->SaveCostSeconds : 0.0f;

	if (MenuTitle)
	{
		MenuTitle->SetText(Title);
	}
	if (MenuStatus)
	{
		MenuStatus->SetText(LastResultText);
	}
	if (TransferLabel)
	{
		TransferLabel->SetText(FText::Format(
			LOCTEXT("MenuTransferLabel", "Transfer time ({0} s)"), FText::AsNumber(Gift)));
	}
	if (RequestLabel)
	{
		RequestLabel->SetText(FText::Format(
			LOCTEXT("MenuRequestLabel", "Request time ({0} s)"), FText::AsNumber(Gift)));
	}
	if (SaveLabel)
	{
		// Named for what it now does. It used to write a second, separate checkpoint; the run keeps a
		// stack now, so the paid button's job is to replace the times recorded where the players are
		// standing. That is a rewrite of their own safety net -- usually downwards -- which is exactly
		// why it costs and why it has to be asked for.
		SaveLabel->SetText(FText::Format(
			LOCTEXT("MenuSaveLabel", "Record my time here ({0} s)"), FText::AsNumber(SaveCost)));
	}

	// The mandatory save already ran when the players walked in, so a hub still offers the paid one
	// on top: it is a different promise ("as you are now") rather than a duplicate of it.
	OnMenuRefreshed(Title, LastResultText);
}

TArray<FL2H_CheckpointRecord> UL2H_TimeMenuWidget::GetTravelOptions() const
{
	TArray<FL2H_CheckpointRecord> Options;

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		return Options;
	}

	// The menu knows the place it was opened at; the operator cannot be trusted to remember it, so
	// the destination list is always computed from the terminal rather than passed in.
	const FName Standing = Terminal ? Terminal->GetCheckpointId() : NAME_None;
	return SaveSubsystem->GetTravelOptions(this, Standing);
}

void UL2H_TimeMenuWidget::RequestTravelTo(FName CheckpointId)
{
	AL2H_BaseCharacter* Character = ResolveActingCharacter();
	if (!Character)
	{
		return;
	}

	Character->ServerTravelToCheckpoint(CheckpointId);

	LastResultText = LOCTEXT("MenuTravelSent", "Rewinding to an earlier checkpoint.");

	// Closed here rather than left up for the server's answer: the request is a rewind, and a menu that
	// names the place the players are standing in has no business staying on screen while they leave
	// it. A refused rewind logs its own reason and simply does not move anybody.
	Character->CloseTerminalMenu();
}

float UL2H_TimeMenuWidget::GetDesignerTime() const
{
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	return Config ? Config->DesignerTime : 0.0f;
}

bool UL2H_TimeMenuWidget::CanAdoptDesignerTime() const
{
	const AL2H_BaseCharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return false;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (!Config || Config->DesignerTime <= 0.0f)
	{
		return false;
	}

	// The same test the server applies before it acts. Repeated here so a Blueprint can hide the offer
	// instead of showing a button that is guaranteed to be refused -- a dead button reads as a bug.
	return AL2H_TimeDirector::GetTimeFor(Character) < Config->DesignerTime;
}

void UL2H_TimeMenuWidget::RequestDesignerTime()
{
	AL2H_BaseCharacter* Character = ResolveActingCharacter();
	if (!Character)
	{
		return;
	}

	Character->ServerAdoptDesignerTime();

	LastResultText = LOCTEXT("MenuDesignerTimeSent", "Designer time requested.");
	RefreshMenu();
}

#undef LOCTEXT_NAMESPACE
