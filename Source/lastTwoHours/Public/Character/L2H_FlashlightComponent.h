// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "L2H_FlashlightComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundBase;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A lamp worn on the shoulder (ResidentHorrorV2's flashlight mesh), so both hands stay free for the
 * weapons. It points where its wearer looks -- the camera for the one holding the pad, the replicated aim
 * for everyone else -- and the partner sees its beam.
 *
 * It belongs to the time theme: when its wearer is low on time (LowTimeSeconds) it starts to stutter, with
 * ResidentHorrorV2's flicker sounds, a warning that reaches the player even in the dark.
 *
 * It binds its own input (IA_L2H_Flashlight in IMC_L2H_Flashlight) once its pawn has an input component,
 * so the character only has to own it.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Flashlight"))
class LASTTWOHOURS_API UL2H_FlashlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_FlashlightComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "L2H|Flashlight")
	bool IsOn() const { return bOn; }

	/** Local: asks the server to switch it. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Flashlight")
	void Toggle();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	TObjectPtr<UInputAction> ToggleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	TObjectPtr<UStaticMesh> LampMesh;

	/** Where it is worn: this bone (or the capsule if the mesh lacks it), with this offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	FName AttachBone = TEXT("clavicle_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	FVector CapsuleOffset = FVector(18.0f, 20.0f, 52.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	float LampScale = 0.6f;

	/** Lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight", meta = (ClampMin = "0.0"))
	float Intensity = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight", meta = (ClampMin = "100.0"))
	float Range = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float OuterCone = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float InnerCone = 12.0f;

	/** Below this many seconds of time left, it stutters. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight", meta = (ClampMin = "0.0"))
	float LowTimeSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	TArray<TObjectPtr<USoundBase>> FlickerSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Flashlight")
	TObjectPtr<USoundBase> ClickSound;

	UPROPERTY(ReplicatedUsing = OnRep_On)
	bool bOn = false;

	UFUNCTION()
	void OnRep_On();

	UFUNCTION(Server, Reliable)
	void ServerSetOn(bool bNewOn);

private:
	void BuildLamp();
	void TryBindInput();
	void ApplyOn();
	void UpdateFlicker(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Spot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lamp;

	TWeakObjectPtr<UInputComponent> BoundInput;
	float FlickerUntil = 0.0f;
	float NextFlickerTime = 0.0f;
};
