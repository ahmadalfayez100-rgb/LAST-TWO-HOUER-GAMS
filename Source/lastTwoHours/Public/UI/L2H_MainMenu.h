// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "UI/L2H_ClockMenuWidget.h"

#include "L2H_MainMenu.generated.h"

/**
 * The title screen's player: no pawn, only the clock menu. New game (starting over wipes the level's
 * checkpoint, after asking), continue (opens the level, which restores its checkpoint), settings, quit.
 */
UCLASS(meta = (DisplayName = "L2H Main Menu Controller"))
class LASTTWOHOURS_API AL2H_MainMenuController : public APlayerController
{
	GENERATED_BODY()

public:
	/** The level a game starts in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Menu")
	FName StartLevel = TEXT("DefaultLevel");

	/** Travel options: listen, so a partner can join the host's game. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Menu")
	FString StartOptions = TEXT("listen");

protected:
	virtual void BeginPlay() override;

private:
	FL2H_ClockPage MainPage() const;
	bool HasSave() const;
	void OnChosen(FName Id);
	void OnBack();
	void StartGame(bool bFresh);

	UPROPERTY(Transient)
	TObjectPtr<UL2H_ClockMenuWidget> Menu;
};

/** The title screen's mode: the menu controller, no pawn. Set it as the menu map's GameMode override. */
UCLASS(meta = (DisplayName = "L2H Main Menu Mode"))
class LASTTWOHOURS_API AL2H_MainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AL2H_MainMenuGameMode();
};
