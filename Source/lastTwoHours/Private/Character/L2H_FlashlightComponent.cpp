// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_FlashlightComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Time/L2H_TimeDirector.h"

UL2H_FlashlightComponent::UL2H_FlashlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	ToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_Flashlight.IA_L2H_Flashlight"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_Flashlight.IMC_L2H_Flashlight"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	LampMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ResidentHorrorV2/Demo/Meshes/SM_Flashlight.SM_Flashlight"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/ResidentHorrorV2/Audio/SoundWaves/WAV_LightFlicker0%d.WAV_LightFlicker0%d"), Index, Index);
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			FlickerSounds.Add(Sound);
		}
	}
	ClickSound = FlickerSounds.Num() > 0 ? FlickerSounds[0] : nullptr;
}

void UL2H_FlashlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UL2H_FlashlightComponent, bOn);
}

void UL2H_FlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildLamp();
	ApplyOn();
}

void UL2H_FlashlightComponent::BuildLamp()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Spot)
	{
		return;
	}

	USkeletalMeshComponent* Body = Character->GetMesh();
	const bool bOnBone = Body && Body->DoesSocketExist(AttachBone);
	USceneComponent* Parent = bOnBone ? static_cast<USceneComponent*>(Body) : static_cast<USceneComponent*>(Character->GetCapsuleComponent());

	Lamp = NewObject<UStaticMeshComponent>(Character, TEXT("L2H_FlashlightLamp"));
	Lamp->SetStaticMesh(LampMesh);
	Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lamp->SetCastShadow(false);
	Lamp->SetRelativeScale3D(FVector(LampScale));
	Lamp->SetUsingAbsoluteRotation(true);
	Lamp->SetupAttachment(Parent, bOnBone ? AttachBone : NAME_None);
	if (!bOnBone)
	{
		Lamp->SetRelativeLocation(CapsuleOffset);
	}
	Lamp->RegisterComponent();

	// The beam is aimed in world space every frame, so it is not bound to the bone's swing.
	Spot = NewObject<USpotLightComponent>(Character, TEXT("L2H_FlashlightBeam"));
	Spot->SetupAttachment(Lamp);
	Spot->SetUsingAbsoluteRotation(true);
	Spot->SetIntensityUnits(ELightUnits::Lumens);
	Spot->SetIntensity(Intensity);
	Spot->SetAttenuationRadius(Range);
	Spot->SetOuterConeAngle(OuterCone);
	Spot->SetInnerConeAngle(FMath::Min(InnerCone, OuterCone));
	Spot->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));
	Spot->SetCastShadows(true);
	Spot->SetVisibility(false);
	Spot->RegisterComponent();
}

void UL2H_FlashlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryBindInput();

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Spot || !Pawn || !bOn)
	{
		return;
	}

	// Where its wearer looks: the camera locally, the replicated aim on the other machines. It starts a
	// little ahead of the shoulder so the head does not throw the whole beam into shadow.
	const FRotator Aim = Pawn->GetBaseAimRotation();
	Spot->SetWorldRotation(Aim);
	// The mesh is modelled along +Y; turned so its lens points down the beam.
	Lamp->SetWorldRotation(Aim.Quaternion() * FRotator(0.0f, -90.0f, 0.0f).Quaternion());
	Spot->SetWorldLocation(Lamp->GetComponentLocation() + Aim.Vector() * 25.0f);

	UpdateFlicker(DeltaTime);
}

void UL2H_FlashlightComponent::UpdateFlicker(float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float TimeLeft = AL2H_TimeDirector::GetTimeFor(GetOwner());
	const bool bLow = LowTimeSeconds > 0.0f && TimeLeft > 0.0f && TimeLeft < LowTimeSeconds;

	if (bLow && Now >= NextFlickerTime)
	{
		// The less time is left, the more often it gives out.
		const float Urgency = 1.0f - TimeLeft / LowTimeSeconds;
		FlickerUntil = Now + FMath::FRandRange(0.08f, 0.35f);
		NextFlickerTime = Now + FMath::Lerp(4.0f, 0.8f, Urgency) * FMath::FRandRange(0.6f, 1.4f);
		if (FlickerSounds.Num() > 0)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FlickerSounds[FMath::RandRange(0, FlickerSounds.Num() - 1)], Spot->GetComponentLocation(), 0.5f);
		}
	}

	float Scale = 1.0f;
	if (Now < FlickerUntil)
	{
		Scale = FMath::FRand() < 0.5f ? 0.05f : FMath::FRandRange(0.3f, 0.8f);
	}
	Spot->SetIntensity(Intensity * Scale);
}

void UL2H_FlashlightComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !ToggleAction)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}
	BoundInput = Input;
	Input->BindAction(ToggleAction, ETriggerEvent::Started, this, &UL2H_FlashlightComponent::Toggle);

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (MappingContext && !Subsystem->HasMappingContext(MappingContext))
		{
			Subsystem->AddMappingContext(MappingContext, 1);
		}
	}
}

void UL2H_FlashlightComponent::Toggle()
{
	if (ClickSound && Spot)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ClickSound, Spot->GetComponentLocation(), 0.35f);
	}
	ServerSetOn(!bOn);
}

void UL2H_FlashlightComponent::ServerSetOn_Implementation(bool bNewOn)
{
	bOn = bNewOn;
	ApplyOn();
}

void UL2H_FlashlightComponent::OnRep_On()
{
	ApplyOn();
}

void UL2H_FlashlightComponent::ApplyOn()
{
	if (Spot)
	{
		Spot->SetVisibility(bOn);
		Spot->SetIntensity(Intensity);
	}
}
