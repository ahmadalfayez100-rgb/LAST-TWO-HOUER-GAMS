// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/L2H_ClockMenuWidget.h"

#include "L2H_TimeMenuComponent.generated.h"

class AL2H_BaseCharacter;

/**
 * The time menu on the clock face -- the one menu for everything time: giving and asking for time,
 * answering the partner, the tools in the bag, and at a checkpoint the shop, the weapon upgrades, the
 * bank, saving and rewinding. D-pad Left opens it anywhere; Triangle / the menu key at a terminal opens
 * it on that terminal's face.
 *
 * Long lists are split into faces (the shop, each weapon's upgrades, the rewind list) so the dial never
 * holds more than it can show. Every choice goes out through the pawn's own server calls, so the server
 * still decides; what came of it is shown on the line under the clock.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Time Menu"))
class LASTTWOHOURS_API UL2H_TimeMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_TimeMenuComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Opens the menu (on the terminal's face when standing at one), or closes it. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void Toggle();

	/** Opens it. False when this is not the local player's pawn. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void Close();

	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	bool IsOpen() const { return Menu != nullptr; }

	/** The widget, for tests. */
	UL2H_ClockMenuWidget* GetMenuWidget() const { return Menu; }

private:
	AL2H_BaseCharacter* GetCharacter() const;
	bool AtTerminal() const;
	bool AtHub() const;

	/** Builds the face for PageId with the values as they stand now. */
	FL2H_ClockPage BuildPage(FName PageId) const;
	FL2H_ClockPage RootPage() const;
	FL2H_ClockPage TimePage() const;
	FL2H_ClockPage ShopPage() const;
	FL2H_ClockPage UpgradesPage() const;
	FL2H_ClockPage WeaponUpgradesPage(FName PageId) const;
	FL2H_ClockPage ToolsPage() const;
	FL2H_ClockPage BankPage() const;
	FL2H_ClockPage CheckpointPage() const;
	FL2H_ClockPage RewindPage() const;

	void GoTo(FName PageId);
	void OnChosen(FName Id);
	void OnBack();

	UPROPERTY(Transient)
	TObjectPtr<UL2H_ClockMenuWidget> Menu;

	/** The faces opened on the way here; Back returns along it. */
	TArray<FName> PageStack;
	float NextRefresh = 0.0f;
};
