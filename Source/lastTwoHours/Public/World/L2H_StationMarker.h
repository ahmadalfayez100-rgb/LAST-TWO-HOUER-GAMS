// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "L2H_StationMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A coloured marker for a time-system station: a flat pad on the floor, a tall pole you can see across
 * the level, and a floating label saying what the station does. Hubs are green, save spots blue, revive
 * stations red, so a player can find them without knowing where they were placed.
 *
 * The pieces are built when the component registers (editor and game alike), so the owning actor only
 * sets Color and Label.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent))
class LASTTWOHOURS_API UL2H_StationMarker : public USceneComponent
{
	GENERATED_BODY()

public:
	UL2H_StationMarker();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Marker")
	FLinearColor Color = FLinearColor(0.1f, 0.9f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Marker")
	FText Label;

	/** Pad radius, cm. Matches the station's trigger so the pad shows where to stand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Marker", meta = (ClampMin = "10.0"))
	float PadRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Marker", meta = (ClampMin = "50.0"))
	float PoleHeight = 600.0f;

	/** Applies Color and Label to the built pieces (call after changing either at runtime). */
	UFUNCTION(BlueprintCallable, Category = "L2H|Marker")
	void Refresh();

protected:
	virtual void OnRegister() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Pad;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> Text;

	UStaticMeshComponent* MakePiece(const TCHAR* Name);
};
