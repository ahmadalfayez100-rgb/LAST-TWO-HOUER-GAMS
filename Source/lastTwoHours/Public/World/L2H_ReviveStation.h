// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/L2H_InteractableInterface.h"

#include "L2H_ReviveStation.generated.h"

class AL2H_BaseCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class UL2H_StationMarker;

/**
 * A revive station: carry a partner's body into it (or lay it down inside) and they stand up with
 * ReviveSeconds on the clock and half their Health.
 *
 * One use per stage by default: a station that revived for free every time was a way to mint time by
 * dying next to it. Its charges come back when the players finish the stage at a hub, and a rewind
 * puts them back as they were at that checkpoint (the charges used are part of the world state).
 */
UCLASS(BlueprintType, meta = (DisplayName = "L2H Revive Station"))
class LASTTWOHOURS_API AL2H_ReviveStation : public AActor, public IL2H_InteractableInterface
{
	GENERATED_BODY()

public:
	AL2H_ReviveStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void L2H_GetWorldStateId_Implementation(FGuid& OutId) override;
	virtual void L2H_CaptureWorldState_Implementation(TMap<FName, bool>& OutFlags) override;
	virtual void L2H_ApplyWorldState_Implementation(const TMap<FName, bool>& InFlags) override;

	/** Seconds the revived player gets on top of whatever their clock held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "1.0"))
	float ReviveSeconds = 60.0f;

	/** Revives per stage. Refilled when the stage ends at a hub. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Revive", meta = (ClampMin = "1", ClampMax = "8"))
	int32 Charges = 1;

	UFUNCTION(BlueprintPure, Category = "L2H|Revive")
	int32 GetChargesLeft() const { return Charges - ChargesUsed; }

	/** Server only. All charges back; the director calls it when a stage ends. */
	void Refill();

	/** Fired on every machine when a charge is used or refilled, for the glow and the counter. */
	UFUNCTION(BlueprintImplementableEvent, Category = "L2H|Revive")
	void OnChargesChanged(int32 ChargesLeft);

protected:
	virtual void BeginPlay() override;

	/** Looks for a body carried into, or lying in, the station. Server only, on a short timer. */
	void Scan();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Revive")
	TObjectPtr<UBoxComponent> Range;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Revive")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Red pad, pole and label, so the station can be found across the level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Revive")
	TObjectPtr<UL2H_StationMarker> Marker;

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(ReplicatedUsing = OnRep_ChargesUsed)
	int32 ChargesUsed = 0;

	UFUNCTION()
	void OnRep_ChargesUsed();

	FTimerHandle ScanTimer;
};
