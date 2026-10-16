// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/L2H_ClockMenuWidget.h"

#include "L2H_PauseMenuComponent.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * The in-game menu (Options / P): the same clock face as the title screen, with the player's time left
 * in the middle. Resume, settings, back to the last checkpoint (the host), main menu, quit.
 *
 * In co-op the world does not stop -- the partner is still playing -- so it pauses only a game with one
 * player in it. It binds its own key, so the character only has to own it.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Pause Menu"))
class LASTTWOHOURS_API UL2H_PauseMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_PauseMenuComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	bool IsOpen() const { return Menu != nullptr; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** The title screen's map, for "Main menu". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Menu")
	FName MainMenuLevel = TEXT("L2H_MainMenu");

private:
	void Open();
	void Close();
	void OnChosen(FName Id);
	void OnBack();
	FL2H_ClockPage PausePage() const;
	void TryBindInput();
	bool IsSoloGame() const;

	UPROPERTY(Transient)
	TObjectPtr<UL2H_ClockMenuWidget> Menu;

	TWeakObjectPtr<UInputComponent> BoundInput;
	bool bPausedWorld = false;
};
