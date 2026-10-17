// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "L2H_AbilityModeComponent.generated.h"

class AL2H_ModeOrb;
class UAnimMontage;
class UInputAction;
class UInputMappingContext;
class UL2H_StruggleWidget;

/** What L1 does right now. Default is the character's own super ability (Samer's ball, Yousef's arena). */
UENUM(BlueprintType)
enum class EL2H_AbilityMode : uint8
{
	Default UMETA(DisplayName = "Default"),
	/** Samer: shrink himself for a while -- small gaps, harder to see, no shooting (Ability.Shrink). */
	Shrink UMETA(DisplayName = "Shrink"),
	/** Samer: lay a mark; the next press takes him back to it (Ability.RecallAnchor). */
	Return UMETA(DisplayName = "Return"),
	/** Yousef: throw an orb that sticks to what it hits and slows it. */
	SlowOrb UMETA(DisplayName = "Slow Orb"),
	/** Samer: an orb that plants a slow field where it lands (Ability.SlowBall). */
	SlowBall UMETA(DisplayName = "Slow Ball"),
	/** Samer: an orb that plants a haste field where it lands (Ability.HasteBall). */
	HasteBall UMETA(DisplayName = "Haste Ball"),
};

/**
 * The character's set of abilities on L1, switched with R1. Samer: his travel ball, the slow and haste
 * balls, shrink, return. Yousef: his arena, the slow orb. The first mode is the character's existing super
 * ability, untouched; the others take over L1 with a higher-priority input context while selected. Samer's
 * modes are his own GAS abilities, activated here by tag; Yousef's orb is run here directly. Which set a
 * character has is read from the abilities it was given.
 *
 * Every mode costs time through the director, like the super abilities do. The balls charge their own.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Ability Modes"))
class LASTTWOHOURS_API UL2H_AbilityModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_AbilityModeComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Ability")
	EL2H_AbilityMode GetMode() const { return Modes.IsValidIndex(ModeIndex) ? Modes[ModeIndex] : EL2H_AbilityMode::Default; }

	/** Local: the next mode (R1). */
	UFUNCTION(BlueprintCallable, Category = "L2H|Ability")
	void CycleMode();

	/** Local: use the selected mode (L1, when it is not the default one). */
	UFUNCTION(BlueprintCallable, Category = "L2H|Ability")
	void UseMode();

	/** The orb thrown by this player hit or landed (called by the orb, server). */
	void OnOrbLanded(AL2H_ModeOrb* Orb);

	/** The name of a mode, for the toast and the HUD. */
	FText GetModeName(EL2H_AbilityMode Mode) const;

protected:
	virtual void BeginPlay() override;

	// Input -----------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Input")
	TObjectPtr<UInputAction> SwitchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Input")
	TObjectPtr<UInputMappingContext> SwitchContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Input")
	TObjectPtr<UInputAction> AltAction;

	/** L1 / Q for the non-default modes; added above the normal mappings only while one is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Input")
	TObjectPtr<UInputMappingContext> AltContext;

	// Shrink ----------------------------------------------------------------------------------------

	/** Seconds before growing back on its own (a press grows back sooner). The size is on the shrink component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Shrink", meta = (ClampMin = "1.0"))
	float ShrinkSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Shrink", meta = (ClampMin = "0.0"))
	float ShrinkCost = 3.0f;

	// Return ----------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Return", meta = (ClampMin = "0.0"))
	float ReturnThrowCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Return", meta = (ClampMin = "0.0"))
	float ReturnJumpCost = 3.0f;

	// Slow orb --------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|SlowOrb", meta = (ClampMin = "0.0"))
	float SlowOrbCost = 4.0f;

	/** How much of normal speed the struck thing keeps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|SlowOrb", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SlowOrbFactor = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|SlowOrb", meta = (ClampMin = "0.5"))
	float SlowOrbSeconds = 6.0f;

	/** Throw speed of either orb, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability", meta = (ClampMin = "100.0"))
	float OrbSpeed = 1800.0f;

	// Poses (upper body; he keeps walking) -----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Anim")
	TObjectPtr<UAnimMontage> ShrinkMontage;

	/** Samer laying the return marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Anim")
	TObjectPtr<UAnimMontage> MarkerThrowMontage;

	/** Yousef throwing the slow orb. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability|Anim")
	TObjectPtr<UAnimMontage> SlowOrbThrowMontage;

	/** Seconds between two uses of a mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Ability", meta = (ClampMin = "0.0"))
	float ModeCooldown = 0.6f;

	// State -----------------------------------------------------------------------------------------

	/** This character's modes, in R1 order. Filled on every machine from what it was given. */
	TArray<EL2H_AbilityMode> Modes;

	UPROPERTY(ReplicatedUsing = OnRep_ModeIndex)
	int32 ModeIndex = 0;

	UFUNCTION()
	void OnRep_ModeIndex();

	UFUNCTION(Server, Reliable)
	void ServerSetMode(int32 NewIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseMode();

	/** The time-bend button came up: a held bend ends. */
	UFUNCTION(Server, Reliable)
	void ServerReleaseBend();

private:
	void ReleaseBend();
	void PlayPose(UAnimMontage* Montage);
	void BuildModes();
	void TryBindInput();
	void ApplyAltContext();
	void ShowToast(const FText& Text);

	bool PayFor(float Seconds);
	/** Enough time for this, with a second left over (the press never ends the player). */
	bool CanAfford(float Seconds) const;
	bool HasAbilityTag(FName Tag) const;
	bool ActivateByTag(FName Tag);
	void UseShrink();
	void UseReturn();
	void UseSlowOrb();
	AL2H_ModeOrb* ThrowOrb(bool bSlow);

	TWeakObjectPtr<UInputComponent> BoundInput;
	TWeakObjectPtr<UInputComponent> BendReleaseInput;
	float NextUseTime = 0.0f;
	float GrowAt = 0.0f;
	float ToastUntil = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UL2H_StruggleWidget> Toast;
};
