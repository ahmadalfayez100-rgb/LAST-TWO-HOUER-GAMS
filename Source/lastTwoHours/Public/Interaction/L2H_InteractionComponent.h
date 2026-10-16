// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Interaction/L2H_InteractionTypes.h"

#include "L2H_InteractionComponent.generated.h"

/**
 * Fired whenever the focused object changes.
 *
 * A Blueprint child uses this to drive the outline/highlight, so the highlight is a presentation
 * decision the component does not have to know how to make. It fires on clients from OnRep and on the
 * server from the refresh, so a listen server's own HUD highlights like any client's.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FL2HOnFocusChanged, AActor*, NewFocus, AActor*, PreviousFocus);

/**
 * The player's half of the interaction system.
 *
 * Drop it on any pawn. Nothing here casts to a specific pawn class -- the owner is used only as
 * "whatever is looking" -- which is what lets the same component sit on a test pawn first and be moved
 * to the hero later, or onto a completely different character rig, without a code change.
 *
 * The authority decides what is focused and replicates the answer, because only the server may decide
 * what is in range; a client reads FocusedActor and shows the prompt it was handed. The press itself is
 * routed to the server for the same reason.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Interaction"))
class LASTTWOHOURS_API UL2H_InteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_InteractionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The object currently offered to the player, or null. Replicated. */
	UFUNCTION(BlueprintPure, Category = "L2H|Interaction")
	AActor* GetFocusedActor() const { return FocusedActor; }

	/** The prompt for the focused object. Replicated, so a client shows what the server decided. */
	UFUNCTION(BlueprintPure, Category = "L2H|Interaction")
	FL2H_InteractionPrompt GetPrompt() const { return Prompt; }

	/**
	 * The local player's press. Safe to call from any machine.
	 *
	 * On the host this runs the interaction directly; on a client it routes to the server. Both paths
	 * end in the same authority-only Interact(), so the host is not a special case with its own rules.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|Interaction")
	void RequestInteract();

	/**
	 * Server-only. Runs the focused object's interaction and reports whether it happened.
	 *
	 * Refusals are logged rather than silent: "the door did not open" and "the press never reached
	 * the door" are different bugs and the log has to say which one it was.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Interaction")
	bool Interact();

	/** Broadcast when the focus changes. Hook a Blueprint child's highlight up here. */
	UPROPERTY(BlueprintAssignable, Category = "L2H|Interaction")
	FL2HOnFocusChanged OnFocusChanged;

protected:
	UFUNCTION()
	void OnRep_FocusedActor();

	/** The client's press, arriving on the server. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "L2H|Interaction")
	void ServerInteract();

	/** Traces for a candidate and adopts the best one. Authority only. */
	void RefreshFocus();

	/** Rebuilds Prompt from whatever is focused right now. */
	void RefreshPrompt();

	/** An actor qualifies only by implementing the interface. */
	static bool ImplementsInteractable(const AActor* Candidate);

	/** Reach in centimetres from the owner's eyes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Interaction", meta = (ClampMin = "50.0"))
	float InteractRange = 250.0f;

	/**
	 * Radius of the focus sweep.
	 *
	 * A sphere rather than a bare line on purpose: a paper on a desk or a drawer handle is rarely
	 * exactly on the crosshair, and a line trace makes the player fight the camera to press anything.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Interaction", meta = (ClampMin = "0.0"))
	float ProbeRadius = 18.0f;

	/** With nothing under the view, the nearest interactable within this radius of the body (cm), front first. 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Interaction", meta = (ClampMin = "0.0"))
	float ProximityRadius = 160.0f;

	/**
	 * Channel the focus sweep runs on. Defaults to the L2H Interact channel defined in DefaultEngine.ini.
	 * Only things configured to block it are ever considered, so scenery cannot steal the focus.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel4;

	/** Seconds between focus refreshes. Throttled because focus cannot change faster than a player aims. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Interaction", meta = (ClampMin = "0.0"))
	float RefreshInterval = 0.1f;

	UPROPERTY(ReplicatedUsing = OnRep_FocusedActor, BlueprintReadOnly, Category = "L2H|Interaction")
	TObjectPtr<AActor> FocusedActor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "L2H|Interaction")
	FL2H_InteractionPrompt Prompt;

	/** Kept so a delegate fired on a client can say what was focused before this one. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviousFocusedActor;

private:
	float TimeSinceRefresh = 0.0f;
};
