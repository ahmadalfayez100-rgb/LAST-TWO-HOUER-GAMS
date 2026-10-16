// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "L2H_StruggleComponent.generated.h"

class UInputAction;
class UL2H_StruggleWidget;

/**
 * Being grabbed, from the player's side. An enemy calls BeginGrab; from then until EndGrab the player mashes
 * the jump button (X / Space) and every press fills Struggle. At 1 the grabber lets go; the grabber decides
 * what happens when time runs out first.
 *
 * The meter is the owner's, shown on their screen; the partner sees a call to help instead -- a shot or a
 * blow on the grabber frees them. It binds its own key, so the character only has to own it.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Struggle"))
class LASTTWOHOURS_API UL2H_StruggleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_StruggleComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Struggle")
	bool IsGrabbed() const { return GrabbedBy != nullptr; }

	UFUNCTION(BlueprintPure, Category = "L2H|Struggle")
	AActor* GetGrabber() const { return GrabbedBy; }

	/** 0..1. At 1 the player is free. */
	UFUNCTION(BlueprintPure, Category = "L2H|Struggle")
	float GetStruggle() const { return Struggle; }

	/** Server: Grabber has hold of this player for up to Seconds. */
	void BeginGrab(AActor* Grabber, float Seconds);

	/** Server: let go, whatever the reason. */
	void EndGrab();

	/** True when Who is held by anything. */
	static bool IsGrabbed(const AActor* Who);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Struggle")
	TObjectPtr<UInputAction> MashAction;

	/** Struggle gained per press. 0.1 = ten quick presses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Struggle", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MashStep = 0.1f;

	/** Struggle lost per second: stopping to rest gives ground back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Struggle", meta = (ClampMin = "0.0"))
	float StruggleDecay = 0.25f;

	UPROPERTY(ReplicatedUsing = OnRep_Grab)
	TObjectPtr<AActor> GrabbedBy;

	UPROPERTY(Replicated)
	float Struggle = 0.0f;

	/** Server time the grab ends on its own (the bite), so every screen can show the seconds left. */
	UPROPERTY(Replicated)
	float GrabEndTime = 0.0f;

	UFUNCTION()
	void OnRep_Grab();

	UFUNCTION(Server, Reliable)
	void ServerMash();

private:
	void OnMash();
	void TryBindInput();
	void UpdateWidget();

	TWeakObjectPtr<UInputComponent> BoundInput;

	UPROPERTY(Transient)
	TObjectPtr<UL2H_StruggleWidget> Widget;
};
