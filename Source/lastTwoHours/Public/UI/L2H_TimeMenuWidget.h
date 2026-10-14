// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Save/L2H_SaveGame.h"

#include "L2H_TimeMenuWidget.generated.h"

class AL2H_BaseCharacter;
class AL2H_CheckpointTerminal;
class UButton;
class UTextBlock;

/**
 * The checkpoint terminal's menu: transfer time, request time, and a paid optional save.
 *
 * The widget owns no state and no authority. Transfer and request both leave through the pawn's
 * server RPCs, so the server decides; the optional save leaves through the save subsystem, which
 * charges before it writes. Every outcome -- including every refusal -- comes back as a line of
 * text the Blueprint side shows, because a silent no-op reads as a broken button.
 */
UCLASS(Abstract, meta = (DisplayName = "L2H Time Menu Widget"))
class LASTTWOHOURS_API UL2H_TimeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Points the menu at the terminal that opened it. Safe to call again. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void SetTerminal(AL2H_CheckpointTerminal* InTerminal);

	/** Re-reads the pawn, the costs and the labels and repaints. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void RefreshMenu();

	/** The result of the last action, shown on the status line. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu")
	FText LastResultText;

	/** Fired after every repaint, for Blueprint-side polish. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Menu")
	void OnMenuRefreshed(const FText& Title, const FText& Status);

	// ---------------------------------------------------------------------------------------
	// Rewind and designer time.
	//
	// The Blueprint side owns the layout -- a list whose length is not known until the run has been
	// played cannot be laid out in advance -- so these hand it the list and take the choice back. They
	// are functions rather than buttons on purpose: the number of destinations is a property of the
	// run, not of the menu asset.
	// ---------------------------------------------------------------------------------------

	/** The checkpoints this menu may rewind to: the ones older than where the players are standing. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	TArray<FL2H_CheckpointRecord> GetTravelOptions() const;

	/**
	 * Rewinds the run to CheckpointId and closes the menu.
	 *
	 * The server decides: it is the only side that holds the stack and the only one that may move the
	 * players. Everything after the destination is given up, which is what makes this a rewind.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void RequestTravelTo(FName CheckpointId);

	/** The level's designer time, or 0 when the level has none set. */
	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	float GetDesignerTime() const;

	/**
	 * True when the designer-time offer would actually improve this player's position.
	 *
	 * The same test the server applies before it acts, so a Blueprint can hide the offer rather than
	 * show a button that will be refused.
	 */
	UFUNCTION(BlueprintPure, Category = "L2H|Menu")
	bool CanAdoptDesignerTime() const;

	/** Sends the designer-time offer for the pawn this menu acts for. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Menu")
	void RequestDesignerTime();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleTransferClicked();

	UFUNCTION()
	void HandleRequestClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleCloseClicked();

	/** The pawn this menu acts for, resolved from the owning player. */
	AL2H_BaseCharacter* GetOwningCharacter() const;

	/** Shared body of the three handlers: resolves the pawn, complains once when there is none. */
	AL2H_BaseCharacter* ResolveActingCharacter();

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UTextBlock> MenuTitle;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UTextBlock> MenuStatus;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UButton> TransferButton;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UTextBlock> TransferLabel;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UButton> RequestButton;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UTextBlock> RequestLabel;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UTextBlock> SaveLabel;

	/**
	 * A close button as well as the key that opened the menu. Slate claims Tab for widget navigation,
	 * so a menu you can only leave by pressing the key that opened it is a menu you can get stuck in.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Menu", meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<AL2H_CheckpointTerminal> Terminal;
};
