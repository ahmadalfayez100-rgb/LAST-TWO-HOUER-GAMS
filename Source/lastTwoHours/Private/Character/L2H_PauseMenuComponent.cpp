// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_PauseMenuComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Save/L2H_SaveSubsystem.h"
#include "Time/L2HTimeLog.h"
#include "UI/L2H_ClockMenuWidget.h"
#include "UI/L2H_MenuPages.h"

#define LOCTEXT_NAMESPACE "L2HPauseMenu"

UL2H_PauseMenuComponent::UL2H_PauseMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PauseAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_Pause.IA_L2H_Pause"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_Pause.IMC_L2H_Pause"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UL2H_PauseMenuComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Close();
	Super::EndPlay(EndPlayReason);
}

void UL2H_PauseMenuComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TryBindInput();
}

void UL2H_PauseMenuComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !PauseAction)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}
	BoundInput = Input;
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &UL2H_PauseMenuComponent::Toggle);

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (MappingContext && !Subsystem->HasMappingContext(MappingContext))
		{
			Subsystem->AddMappingContext(MappingContext, 2);
		}
	}
}

bool UL2H_PauseMenuComponent::IsSoloGame() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return World && GameInstance && World->GetNetMode() == NM_Standalone && GameInstance->GetNumLocalPlayers() <= 1;
}

void UL2H_PauseMenuComponent::Toggle()
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

FL2H_ClockPage UL2H_PauseMenuComponent::PausePage() const
{
	FL2H_ClockPage Page;
	Page.Id = TEXT("Pause");
	Page.Title = IsSoloGame() ? LOCTEXT("Paused", "PAUSED") : LOCTEXT("Menu", "MENU");

	auto Add = [&Page](FName Id, const FText& Label, bool bEnabled = true, bool bDanger = false, const FText& Value = FText::GetEmpty())
	{
		FL2H_ClockOption Option;
		Option.Id = Id;
		Option.Label = Label;
		Option.bEnabled = bEnabled;
		Option.bDanger = bDanger;
		Option.Value = Value;
		Page.Options.Add(Option);
	};
	const bool bHost = GetOwner() && GetOwner()->HasAuthority();
	Add(TEXT("Resume"), LOCTEXT("Resume", "Resume"));
	Add(TEXT("Settings"), LOCTEXT("Settings", "Settings"));
	Add(TEXT("Rewind"), LOCTEXT("Rewind", "Last checkpoint"), bHost, false, bHost ? FText::GetEmpty() : LOCTEXT("HostOnly", "host only"));
	Add(TEXT("MainMenu"), LOCTEXT("MainMenu", "Main menu"), true, true);
	Add(TEXT("Quit"), LOCTEXT("Quit", "Quit game"), true, true);
	return Page;
}

void UL2H_PauseMenuComponent::Open()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || Menu)
	{
		return;
	}
	Menu = CreateWidget<UL2H_ClockMenuWidget>(PC, UL2H_ClockMenuWidget::StaticClass());
	if (!Menu)
	{
		return;
	}
	Menu->SetTimeSource(Pawn);
	Menu->SetBackdropOpacity(0.45f);
	Menu->OnChosen.BindUObject(this, &UL2H_PauseMenuComponent::OnChosen);
	Menu->OnBack.BindUObject(this, &UL2H_PauseMenuComponent::OnBack);
	Menu->AddToPlayerScreen(60);
	Menu->ShowPage(PausePage());

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	Menu->TakeFocus();

	if (IsSoloGame())
	{
		bPausedWorld = UGameplayStatics::SetGamePaused(this, true);
	}
}

void UL2H_PauseMenuComponent::Close()
{
	if (!Menu)
	{
		return;
	}
	Menu->RemoveFromParent();
	Menu = nullptr;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	if (bPausedWorld)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedWorld = false;
	}
}

void UL2H_PauseMenuComponent::OnBack()
{
	if (!Menu)
	{
		return;
	}
	if (Menu->GetPage().Id == TEXT("Pause"))
	{
		Close();
	}
	else
	{
		Menu->ShowPage(PausePage(), /*bForward=*/false);
	}
}

void UL2H_PauseMenuComponent::OnChosen(FName Id)
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
	if (Id == TEXT("Resume"))
	{
		Close();
	}
	else if (Id == TEXT("Settings"))
	{
		Menu->ShowPage(L2HMenuPages::SettingsPage());
	}
	else if (Id == TEXT("Rewind"))
	{
		Menu->ShowPage(L2HMenuPages::ConfirmPage(TEXT("ConfirmRewind"), LOCTEXT("ConfirmRewind", "BACK TO THE LAST CHECKPOINT?"),
			LOCTEXT("RewindYes", "Go back"), LOCTEXT("RewindNo", "Stay")));
	}
	else if (Id == TEXT("MainMenu"))
	{
		Menu->ShowPage(L2HMenuPages::ConfirmPage(TEXT("ConfirmMenu"), LOCTEXT("ConfirmMenu", "LEAVE FOR THE MAIN MENU?"),
			LOCTEXT("MenuYes", "Leave"), LOCTEXT("MenuNo", "Stay")));
	}
	else if (Id == TEXT("Quit"))
	{
		Menu->ShowPage(L2HMenuPages::ConfirmPage(TEXT("ConfirmQuit"), LOCTEXT("ConfirmQuit", "QUIT THE GAME?"),
			LOCTEXT("QuitYes", "Quit"), LOCTEXT("QuitNo", "Stay")));
	}
	else if (Id == TEXT("Yes"))
	{
		if (PageId == TEXT("ConfirmRewind"))
		{
			Close();
			UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
			if (UL2H_SaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr)
			{
				const bool bRestored = Save->RestoreCheckpoint(this);
				L2HTime::LogImportant(FString::Printf(TEXT("Menu: back to the last checkpoint (%s)"), bRestored ? TEXT("restored") : TEXT("none saved")));
			}
		}
		else if (PageId == TEXT("ConfirmMenu"))
		{
			Close();
			UGameplayStatics::OpenLevel(this, MainMenuLevel);
		}
		else if (PageId == TEXT("ConfirmQuit"))
		{
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
