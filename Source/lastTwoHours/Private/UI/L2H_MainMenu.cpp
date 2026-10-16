// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/L2H_MainMenu.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Time/L2HTimeLog.h"
#include "UI/L2H_MenuPages.h"

#define LOCTEXT_NAMESPACE "L2HMainMenu"

AL2H_MainMenuGameMode::AL2H_MainMenuGameMode()
{
	PlayerControllerClass = AL2H_MainMenuController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void AL2H_MainMenuController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	Menu = CreateWidget<UL2H_ClockMenuWidget>(this, UL2H_ClockMenuWidget::StaticClass());
	if (!Menu)
	{
		return;
	}
	Menu->SetBackdropOpacity(1.0f);
	Menu->OnChosen.BindUObject(this, &AL2H_MainMenuController::OnChosen);
	Menu->OnBack.BindUObject(this, &AL2H_MainMenuController::OnBack);
	Menu->AddToPlayerScreen(10);
	Menu->ShowPage(MainPage());

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	Menu->TakeFocus();
}

bool AL2H_MainMenuController::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(L2HMenuPages::CheckpointSlotFor(StartLevel.ToString()), 0);
}

FL2H_ClockPage AL2H_MainMenuController::MainPage() const
{
	FL2H_ClockPage Page;
	Page.Id = TEXT("Main");
	Page.Title = LOCTEXT("Title", "LAST TWO HOURS");

	FL2H_ClockOption Continue;
	Continue.Id = TEXT("Continue");
	Continue.Label = LOCTEXT("Continue", "Continue");
	Continue.bEnabled = HasSave();
	Continue.Value = Continue.bEnabled ? FText::GetEmpty() : LOCTEXT("NoSave", "no save");
	Page.Options.Add(Continue);

	FL2H_ClockOption NewGame;
	NewGame.Id = TEXT("NewGame");
	NewGame.Label = LOCTEXT("NewGame", "New Game");
	Page.Options.Add(NewGame);

	FL2H_ClockOption Settings;
	Settings.Id = TEXT("Settings");
	Settings.Label = LOCTEXT("Settings", "Settings");
	Page.Options.Add(Settings);

	FL2H_ClockOption Quit;
	Quit.Id = TEXT("Quit");
	Quit.Label = LOCTEXT("Quit", "Quit");
	Quit.bDanger = true;
	Page.Options.Add(Quit);
	return Page;
}

void AL2H_MainMenuController::OnChosen(FName Id)
{
	if (!Menu)
	{
		return;
	}
	const FName PageId = Menu->GetPage().Id;

	if (L2HMenuPages::ApplySetting(Id))
	{
		Menu->RefreshPage(L2HMenuPages::SettingsPage());
		return;
	}
	if (Id == TEXT("Back") || Id == TEXT("No"))
	{
		OnBack();
		return;
	}

	if (PageId == TEXT("ConfirmNew") && Id == TEXT("Yes"))
	{
		StartGame(/*bFresh=*/true);
	}
	else if (PageId == TEXT("ConfirmQuit") && Id == TEXT("Yes"))
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
	else if (Id == TEXT("Continue"))
	{
		StartGame(/*bFresh=*/false);
	}
	else if (Id == TEXT("NewGame"))
	{
		// Starting over throws the saved run away, so it asks first when there is one.
		if (HasSave())
		{
			Menu->ShowPage(L2HMenuPages::ConfirmPage(TEXT("ConfirmNew"), LOCTEXT("ConfirmNew", "START OVER?"),
				LOCTEXT("StartOver", "Start over"), LOCTEXT("Keep", "Keep my run")));
		}
		else
		{
			StartGame(/*bFresh=*/true);
		}
	}
	else if (Id == TEXT("Settings"))
	{
		Menu->ShowPage(L2HMenuPages::SettingsPage());
	}
	else if (Id == TEXT("Quit"))
	{
		Menu->ShowPage(L2HMenuPages::ConfirmPage(TEXT("ConfirmQuit"), LOCTEXT("ConfirmQuit", "QUIT?"),
			LOCTEXT("QuitYes", "Quit"), LOCTEXT("QuitNo", "Stay")));
	}
}

void AL2H_MainMenuController::OnBack()
{
	if (Menu && Menu->GetPage().Id != TEXT("Main"))
	{
		Menu->ShowPage(MainPage(), /*bForward=*/false);
	}
}

void AL2H_MainMenuController::StartGame(bool bFresh)
{
	if (bFresh)
	{
		UGameplayStatics::DeleteGameInSlot(L2HMenuPages::CheckpointSlotFor(StartLevel.ToString()), 0);
	}
	L2HTime::LogImportant(FString::Printf(TEXT("Menu: %s %s"), bFresh ? TEXT("new game in") : TEXT("continue in"), *StartLevel.ToString()));

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	UGameplayStatics::OpenLevel(this, StartLevel, /*bAbsolute=*/true, StartOptions);
}

#undef LOCTEXT_NAMESPACE
