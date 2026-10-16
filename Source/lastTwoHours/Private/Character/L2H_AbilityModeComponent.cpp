// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_AbilityModeComponent.h"

#include "AbilitySystemComponent.h"
#include "Ability/L2H_Ability_ChronoBall.h"
#include "Ability/L2H_Ability_TimeBend.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "UI/L2H_StruggleWidget.h"
#include "World/L2H_ModeOrb.h"

#define LOCTEXT_NAMESPACE "L2HAbilityModes"

UL2H_AbilityModeComponent::UL2H_AbilityModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SwitchAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_AbilitySwitch.IA_L2H_AbilitySwitch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	SwitchContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_AbilitySwitch.IMC_L2H_AbilitySwitch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	AltAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_AbilityAlt.IA_L2H_AbilityAlt"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	AltContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_AbilityAlt.IMC_L2H_AbilityAlt"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UL2H_AbilityModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UL2H_AbilityModeComponent, ModeIndex);
	DOREPLIFETIME(UL2H_AbilityModeComponent, bShrunk);
	DOREPLIFETIME(UL2H_AbilityModeComponent, ReturnMarker);
}

void UL2H_AbilityModeComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UL2H_AbilityModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Toast)
	{
		Toast->RemoveFromParent();
		Toast = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UL2H_AbilityModeComponent::BuildModes()
{
	// The set follows the super ability the character was given: the ball is Samer's, the bend Yousef's.
	Modes.Reset();
	Modes.Add(EL2H_AbilityMode::Default);
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	bool bBall = false;
	bool bBend = false;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(UL2H_Ability_ChronoBall::StaticClass()))
		{
			bBall = true;
		}
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(UL2H_Ability_TimeBend::StaticClass()))
		{
			bBend = true;
		}
	}
	if (bBall)
	{
		Modes.Add(EL2H_AbilityMode::Shrink);
		Modes.Add(EL2H_AbilityMode::Return);
	}
	else if (bBend)
	{
		Modes.Add(EL2H_AbilityMode::SlowOrb);
	}
}

FText UL2H_AbilityModeComponent::GetModeName(EL2H_AbilityMode Mode) const
{
	switch (Mode)
	{
	case EL2H_AbilityMode::Shrink: return LOCTEXT("Shrink", "Shrink ball");
	case EL2H_AbilityMode::Return: return LOCTEXT("Return", "Return ball");
	case EL2H_AbilityMode::SlowOrb: return LOCTEXT("SlowOrb", "Slow orb (thrown)");
	default: break;
	}
	return Modes.Contains(EL2H_AbilityMode::SlowOrb) ? LOCTEXT("Arena", "Time bend (around you)") : LOCTEXT("Chrono", "Chrono ball (travel)");
}

void UL2H_AbilityModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TryBindInput();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Growing back on its own once the time is up and there is room.
	if (GetOwner() && GetOwner()->HasAuthority() && bShrunk && World->GetTimeSeconds() >= GrowAt && HasRoomToGrow())
	{
		SetShrunk(false);
	}
	if (Toast && World->GetRealTimeSeconds() >= ToastUntil)
	{
		Toast->RemoveFromParent();
		Toast = nullptr;
	}
}

void UL2H_AbilityModeComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !SwitchAction)
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}
	BoundInput = Input;
	Input->BindAction(SwitchAction, ETriggerEvent::Started, this, &UL2H_AbilityModeComponent::CycleMode);
	if (AltAction)
	{
		Input->BindAction(AltAction, ETriggerEvent::Started, this, &UL2H_AbilityModeComponent::UseMode);
	}
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (SwitchContext && !Subsystem->HasMappingContext(SwitchContext))
		{
			Subsystem->AddMappingContext(SwitchContext, 3);
		}
	}
	ApplyAltContext();
}

void UL2H_AbilityModeComponent::ApplyAltContext()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem || !AltContext)
	{
		return;
	}
	// L1 is the default super ability unless another mode is selected; then this context sits on top and
	// takes the key (its action consumes it, so the default ability does not also fire).
	const bool bAlt = GetMode() != EL2H_AbilityMode::Default;
	if (bAlt && !Subsystem->HasMappingContext(AltContext))
	{
		Subsystem->AddMappingContext(AltContext, 90);
	}
	else if (!bAlt && Subsystem->HasMappingContext(AltContext))
	{
		Subsystem->RemoveMappingContext(AltContext);
	}
}

void UL2H_AbilityModeComponent::CycleMode()
{
	BuildModes();
	if (Modes.Num() <= 1)
	{
		return;
	}
	const int32 Next = (ModeIndex + 1) % Modes.Num();
	ModeIndex = Next;
	OnRep_ModeIndex();
	ServerSetMode(Next);
}

void UL2H_AbilityModeComponent::ServerSetMode_Implementation(int32 NewIndex)
{
	BuildModes();
	ModeIndex = Modes.IsValidIndex(NewIndex) ? NewIndex : 0;
	OnRep_ModeIndex();
}

void UL2H_AbilityModeComponent::OnRep_ModeIndex()
{
	if (Modes.Num() == 0)
	{
		BuildModes();
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		ApplyAltContext();
		ShowToast(FText::Format(LOCTEXT("ModeToast", "L1: {0}"), GetModeName(GetMode())));
	}
}

void UL2H_AbilityModeComponent::ShowToast(const FText& Text)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !GetWorld())
	{
		return;
	}
	if (!Toast)
	{
		Toast = CreateWidget<UL2H_StruggleWidget>(PC, UL2H_StruggleWidget::StaticClass());
		if (Toast)
		{
			Toast->AddToPlayerScreen(30);
		}
	}
	if (Toast)
	{
		Toast->SetState(Text, -1.0f);
		ToastUntil = GetWorld()->GetRealTimeSeconds() + 1.6f;
	}
}

void UL2H_AbilityModeComponent::UseMode()
{
	ServerUseMode();
}

void UL2H_AbilityModeComponent::ServerUseMode_Implementation()
{
	const UWorld* World = GetWorld();
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	if (!World || !Character || Character->IsDowned() || Character->IsDead() || World->GetTimeSeconds() < NextUseTime)
	{
		return;
	}
	if (Modes.Num() == 0)
	{
		BuildModes();
	}
	NextUseTime = World->GetTimeSeconds() + ModeCooldown;
	switch (GetMode())
	{
	case EL2H_AbilityMode::Shrink: UseShrink(); break;
	case EL2H_AbilityMode::Return: UseReturn(); break;
	case EL2H_AbilityMode::SlowOrb: UseSlowOrb(); break;
	default: break;
	}
}

bool UL2H_AbilityModeComponent::PayFor(float Seconds)
{
	AActor* Owner = GetOwner();
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(Owner);
	if (!Director || Seconds <= 0.0f)
	{
		return true;
	}
	// Never the press that ends the player: a second must be left on the clock.
	if (AL2H_TimeDirector::GetTimeFor(Owner) <= Seconds + 1.0f)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Ability mode refused: %s cannot afford %.0fs"), *GetNameSafe(Owner), Seconds));
		return false;
	}
	Director->ApplyTimeCost(Owner, Seconds);
	return true;
}

// Shrink -------------------------------------------------------------------------------------------

bool UL2H_AbilityModeComponent::HasRoomToGrow() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float FullHalf = Capsule->GetUnscaledCapsuleHalfHeight();
	const float FullRadius = Capsule->GetUnscaledCapsuleRadius();
	const float SmallHalf = Capsule->GetScaledCapsuleHalfHeight();
	const FVector GrownCentre = Character->GetActorLocation() + FVector(0.0f, 0.0f, FullHalf - SmallHalf + 2.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HGrowRoom), false, Character);
	return !World->OverlapBlockingTestByChannel(GrownCentre, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(FullRadius * 0.95f, FullHalf * 0.98f), Params);
}

void UL2H_AbilityModeComponent::UseShrink()
{
	if (bShrunk)
	{
		// A press grows back early -- when there is room to.
		if (HasRoomToGrow())
		{
			SetShrunk(false);
		}
		else
		{
			L2HTime::LogImportant(TEXT("Shrink: no room to grow back here"));
		}
		return;
	}
	if (!PayFor(ShrinkCost))
	{
		return;
	}
	SetShrunk(true);
	GrowAt = GetWorld()->GetTimeSeconds() + ShrinkSeconds;
}

void UL2H_AbilityModeComponent::SetShrunk(bool bNewShrunk)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || bShrunk == bNewShrunk)
	{
		return;
	}
	// Growing: lift by the difference first, so the bigger capsule does not start inside the floor.
	if (!bNewShrunk)
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		Character->SetActorLocation(Character->GetActorLocation() + FVector(0.0f, 0.0f,
			Capsule->GetUnscaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleHalfHeight() + 2.0f));
	}
	bShrunk = bNewShrunk;
	OnRep_Shrunk();
	L2HTime::LogImportant(FString::Printf(TEXT("Shrink: %s %s"), *GetNameSafe(Character), bShrunk ? TEXT("shrinks") : TEXT("grows back")));
}

void UL2H_AbilityModeComponent::OnRep_Shrunk()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorScale3D(FVector(bShrunk ? ShrinkScale : 1.0f));
	}
}

// Return -------------------------------------------------------------------------------------------

void UL2H_AbilityModeComponent::UseReturn()
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	AL2H_ModeOrb* Marker = ReturnMarker.Get();
	if (Marker && !Marker->HasLanded())
	{
		return;
	}
	if (!Marker)
	{
		if (PayFor(ReturnThrowCost))
		{
			ReturnMarker = ThrowOrb(false);
		}
		return;
	}
	// Back to the marker, standing on it.
	if (!PayFor(ReturnJumpCost))
	{
		return;
	}
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Destination = Marker->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
	const bool bMoved = Character->TeleportTo(Destination, Character->GetActorRotation());
	L2HTime::LogImportant(FString::Printf(TEXT("Return: %s %s to the marker"), *GetNameSafe(Character), bMoved ? TEXT("returned") : TEXT("could not return")));
	if (bMoved)
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Marker->Destroy();
		ReturnMarker = nullptr;
	}
}

// Slow orb -----------------------------------------------------------------------------------------

void UL2H_AbilityModeComponent::UseSlowOrb()
{
	if (PayFor(SlowOrbCost))
	{
		ThrowOrb(true);
	}
}

AL2H_ModeOrb* UL2H_AbilityModeComponent::ThrowOrb(bool bSlow)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}
	const FRotator Aim = Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Pawn->GetActorRotation();
	const FVector Start = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f) + Aim.Vector() * 60.0f;
	FActorSpawnParameters Params;
	Params.Owner = Pawn;
	Params.Instigator = Pawn;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AL2H_ModeOrb* Orb = World->SpawnActor<AL2H_ModeOrb>(AL2H_ModeOrb::StaticClass(), Start, Aim, Params);
	if (Orb)
	{
		// A little loft so a throw at the floor ahead lands there instead of skimming off.
		const FVector Velocity = (Aim.Vector() + FVector(0.0f, 0.0f, 0.12f)).GetSafeNormal() * OrbSpeed;
		Orb->Launch(this, bSlow, Velocity, SlowOrbFactor, SlowOrbSeconds, bSlow ? 4.0f : ReturnMarkerSeconds);
	}
	return Orb;
}

void UL2H_AbilityModeComponent::OnOrbLanded(AL2H_ModeOrb* Orb)
{
	// The marker's own collision is off once it lands; nothing else to do here yet.
}

#undef LOCTEXT_NAMESPACE
