// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_StruggleComponent.h"

#include "Character/L2H_BaseCharacter.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Net/UnrealNetwork.h"
#include "UI/L2H_StruggleWidget.h"

#define LOCTEXT_NAMESPACE "L2HStruggle"

UL2H_StruggleComponent::UL2H_StruggleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	// The jump button: the press every player already has under their thumb.
	MashAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Jump.IA_Jump"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UL2H_StruggleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UL2H_StruggleComponent, GrabbedBy);
	DOREPLIFETIME(UL2H_StruggleComponent, Struggle);
	DOREPLIFETIME(UL2H_StruggleComponent, GrabEndTime);
}

void UL2H_StruggleComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UL2H_StruggleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UL2H_StruggleComponent::IsGrabbed(const AActor* Who)
{
	const UL2H_StruggleComponent* Struggle = Who ? Who->FindComponentByClass<UL2H_StruggleComponent>() : nullptr;
	return Struggle && Struggle->IsGrabbed();
}

void UL2H_StruggleComponent::BeginGrab(AActor* Grabber, float Seconds)
{
	const UWorld* World = GetWorld();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !World)
	{
		return;
	}
	GrabbedBy = Grabber;
	Struggle = 0.0f;
	GrabEndTime = World->GetTimeSeconds() + Seconds;
	OnRep_Grab();
}

void UL2H_StruggleComponent::EndGrab()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	GrabbedBy = nullptr;
	Struggle = 0.0f;
	OnRep_Grab();
}

void UL2H_StruggleComponent::OnRep_Grab()
{
	UpdateWidget();
}

void UL2H_StruggleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryBindInput();

	if (GetOwner() && GetOwner()->HasAuthority() && GrabbedBy)
	{
		Struggle = FMath::Max(0.0f, Struggle - StruggleDecay * DeltaTime);
	}

	UpdateWidget();
}

void UL2H_StruggleComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !MashAction)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}
	BoundInput = Input;
	Input->BindAction(MashAction, ETriggerEvent::Started, this, &UL2H_StruggleComponent::OnMash);
}

void UL2H_StruggleComponent::OnMash()
{
	if (GrabbedBy)
	{
		ServerMash();
	}
}

void UL2H_StruggleComponent::ServerMash_Implementation()
{
	if (GrabbedBy)
	{
		Struggle = FMath::Min(1.0f, Struggle + MashStep);
	}
}

void UL2H_StruggleComponent::UpdateWidget()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Pawn->IsLocallyControlled() || !PC || !World)
	{
		return;
	}

	// Held: the meter. The partner held: a call to help. Neither: nothing on screen.
	FText Text;
	float Progress = -1.0f;
	if (GrabbedBy)
	{
		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(GrabEndTime - World->GetTimeSeconds()));
		Text = FText::Format(LOCTEXT("Held", "GRABBED -- mash X / Space to break free ({0})"), FText::AsNumber(Seconds));
		Progress = Struggle;
	}
	else
	{
		for (TActorIterator<AL2H_BaseCharacter> It(World); It; ++It)
		{
			if (*It != Pawn && IsGrabbed(*It))
			{
				Text = FText::Format(LOCTEXT("Partner", "{0} is grabbed -- shoot or hit it to free them!"), FText::FromString(It->GetName().Contains(TEXT("Samer")) ? TEXT("Samer") : TEXT("Yousef")));
				break;
			}
		}
	}

	if (Text.IsEmpty())
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
			Widget = nullptr;
		}
		return;
	}
	if (!Widget)
	{
		Widget = CreateWidget<UL2H_StruggleWidget>(PC, UL2H_StruggleWidget::StaticClass());
		if (Widget)
		{
			Widget->AddToPlayerScreen(50);
		}
	}
	if (Widget)
	{
		Widget->SetState(Text, Progress);
	}
}

#undef LOCTEXT_NAMESPACE
