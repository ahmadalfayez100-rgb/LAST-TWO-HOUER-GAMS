// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_AbilityModeComponent.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
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
#include "Ability/L2H_RecallAnchor.h"
#include "Character/L2H_ShrinkComponent.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"

#define LOCTEXT_NAMESPACE "L2HAbilityModes"

UL2H_AbilityModeComponent::UL2H_AbilityModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SwitchAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_AbilitySwitch.IA_L2H_AbilitySwitch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	SwitchContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_AbilitySwitch.IMC_L2H_AbilitySwitch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	AltAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_AbilityAlt.IA_L2H_AbilityAlt"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	AltContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_AbilityAlt.IMC_L2H_AbilityAlt"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	ShrinkMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/_Project/animation/L2H/Magic/Montages/AM_L2H_S_ShrinkCast.AM_L2H_S_ShrinkCast"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	MarkerThrowMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/_Project/animation/L2H/Magic/Montages/AM_L2H_S_OrbLaunch.AM_L2H_S_OrbLaunch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	SlowOrbThrowMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/_Project/animation/L2H/Magic/Montages/AM_L2H_Y_OrbThrow.AM_L2H_Y_OrbThrow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UL2H_AbilityModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UL2H_AbilityModeComponent, ModeIndex);
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
		// Samer's own abilities, each one only when he was given it (the set lives in his ability data).
		if (HasAbilityTag(TEXT("Ability.SlowBall")))
		{
			Modes.Add(EL2H_AbilityMode::SlowBall);
		}
		if (HasAbilityTag(TEXT("Ability.HasteBall")))
		{
			Modes.Add(EL2H_AbilityMode::HasteBall);
		}
		if (HasAbilityTag(TEXT("Ability.Shrink")))
		{
			Modes.Add(EL2H_AbilityMode::Shrink);
		}
		if (HasAbilityTag(TEXT("Ability.RecallAnchor")))
		{
			Modes.Add(EL2H_AbilityMode::Return);
		}
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
	case EL2H_AbilityMode::Shrink: return LOCTEXT("Shrink", "Shrink");
	case EL2H_AbilityMode::Return: return LOCTEXT("Return", "Return mark");
	case EL2H_AbilityMode::SlowBall: return LOCTEXT("SlowBall", "Slow ball (field)");
	case EL2H_AbilityMode::HasteBall: return LOCTEXT("HasteBall", "Haste ball (field)");
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
	// Growing back on its own once the time is up and there is room (the shrink ability toggles back).
	const UL2H_ShrinkComponent* Shrink = GetOwner() ? GetOwner()->FindComponentByClass<UL2H_ShrinkComponent>() : nullptr;
	if (GetOwner() && GetOwner()->HasAuthority() && GrowAt > 0.0f && World->GetTimeSeconds() >= GrowAt)
	{
		if (!Shrink || !Shrink->IsShrunk())
		{
			GrowAt = 0.0f;
		}
		else if (Shrink->HasRoomToGrow() && ActivateByTag(TEXT("Ability.Shrink")) && !Shrink->IsShrunk())
		{
			GrowAt = 0.0f;
		}
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
	if (!Input)
	{
		return;
	}

	// Yousef's bend is held: its own button's release (whichever row starts it) ends it. Tried every
	// tick until found, because on a client the granted abilities arrive after the input exists.
	if (BendReleaseInput.Get() != Input)
	{
		const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(Pawn);
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		for (const FGameplayAbilitySpec& Spec : ASC ? ASC->GetActivatableAbilities() : TArray<FGameplayAbilitySpec>())
		{
			if (!Spec.Ability || !Spec.Ability->GetClass()->IsChildOf(UL2H_Ability_TimeBend::StaticClass()))
			{
				continue;
			}
			for (const FL2H_AbilityInputBinding& Binding : Character->AbilityInputBindings)
			{
				if (Binding.Action && Binding.AbilityTag.IsValid() && Spec.Ability->GetAssetTags().HasTag(Binding.AbilityTag))
				{
					Input->BindAction(Binding.Action, ETriggerEvent::Completed, this, &UL2H_AbilityModeComponent::ReleaseBend);
					BendReleaseInput = Input;
				}
			}
		}
	}

	if (BoundInput.Get() == Input)
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
	// The orbs pay their own way (their ability charges taking one out), and their hold pose is the orb's.
	case EL2H_AbilityMode::SlowBall: ActivateByTag(TEXT("Ability.SlowBall")); break;
	case EL2H_AbilityMode::HasteBall: ActivateByTag(TEXT("Ability.HasteBall")); break;
	default: break;
	}
}

void UL2H_AbilityModeComponent::ReleaseBend()
{
	ServerReleaseBend();
}

void UL2H_AbilityModeComponent::ServerReleaseBend_Implementation()
{
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || !Spec.Ability->GetClass()->IsChildOf(UL2H_Ability_TimeBend::StaticClass()))
		{
			continue;
		}
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UL2H_Ability_TimeBend* Bend = Cast<UL2H_Ability_TimeBend>(Instance))
			{
				Bend->ReleaseHold();
			}
		}
	}
}

void UL2H_AbilityModeComponent::PlayPose(UAnimMontage* Montage)
{
	if (AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner()))
	{
		Character->MulticastPlayCharacterMontage(Montage, 1.0f, /*bLoop=*/false);
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

// Samer's abilities ---------------------------------------------------------------------------------

bool UL2H_AbilityModeComponent::CanAfford(float Seconds) const
{
	return Seconds <= 0.0f || !AL2H_TimeDirector::Get(GetOwner()) || AL2H_TimeDirector::GetTimeFor(GetOwner()) > Seconds + 1.0f;
}

bool UL2H_AbilityModeComponent::HasAbilityTag(FName Tag) const
{
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(Tag, /*ErrorIfNotFound=*/false);
	if (!ASC || !GameplayTag.IsValid())
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(GameplayTag))
		{
			return true;
		}
	}
	return false;
}

bool UL2H_AbilityModeComponent::ActivateByTag(FName Tag)
{
	const AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(GetOwner());
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(Tag, /*ErrorIfNotFound=*/false);
	return ASC && GameplayTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(GameplayTag));
}

void UL2H_AbilityModeComponent::UseShrink()
{
	// The shrink itself is the ability's (the shrink component); this adds the price and the timer.
	// Growing back is free; shrinking costs ShrinkCost and lasts ShrinkSeconds unless pressed again.
	const UL2H_ShrinkComponent* Shrink = GetOwner()->FindComponentByClass<UL2H_ShrinkComponent>();
	if (!Shrink)
	{
		return;
	}
	const bool bWasShrunk = Shrink->IsShrunk();
	if (!bWasShrunk && !CanAfford(ShrinkCost))
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Shrink: %s cannot afford %.0fs"), *GetNameSafe(GetOwner()), ShrinkCost));
		return;
	}
	ActivateByTag(TEXT("Ability.Shrink"));
	if (!bWasShrunk && Shrink->IsShrunk())
	{
		PayFor(ShrinkCost);
		PlayPose(ShrinkMontage);
		GrowAt = GetWorld()->GetTimeSeconds() + ShrinkSeconds;
	}
	else if (bWasShrunk && !Shrink->IsShrunk())
	{
		GrowAt = 0.0f;
	}
}

void UL2H_AbilityModeComponent::UseReturn()
{
	// Laying the mark and going back to it are the recall ability's (the anchor); this adds the price.
	const AActor* Owner = GetOwner();
	auto FindMark = [this, Owner]() -> AL2H_RecallAnchor*
	{
		for (TActorIterator<AL2H_RecallAnchor> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->GetAnchorOwner() == Owner)
			{
				return *It;
			}
		}
		return nullptr;
	};
	const bool bHadMark = FindMark() != nullptr;
	const float Price = bHadMark ? ReturnJumpCost : ReturnThrowCost;
	if (!CanAfford(Price))
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Return: %s cannot afford %.0fs"), *GetNameSafe(Owner), Price));
		return;
	}
	const FVector Before = Owner->GetActorLocation();
	ActivateByTag(TEXT("Ability.RecallAnchor"));
	const bool bHasMark = FindMark() != nullptr;
	if (!bHadMark && bHasMark)
	{
		PayFor(Price);
		PlayPose(MarkerThrowMontage);
	}
	else if (bHadMark && !FVector::PointsAreNear(Before, Owner->GetActorLocation(), 1.0f))
	{
		// A refused return also spends the mark, but only a return that moved him is paid for.
		PayFor(Price);
	}
}

// Slow orb -----------------------------------------------------------------------------------------

void UL2H_AbilityModeComponent::UseSlowOrb()
{
	if (PayFor(SlowOrbCost))
	{
		PlayPose(SlowOrbThrowMontage);
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
		Orb->Launch(this, bSlow, Velocity, SlowOrbFactor, SlowOrbSeconds, bSlow ? 4.0f : 90.0f);
	}
	return Orb;
}

void UL2H_AbilityModeComponent::OnOrbLanded(AL2H_ModeOrb* Orb)
{
	// The marker's own collision is off once it lands; nothing else to do here yet.
}

#undef LOCTEXT_NAMESPACE
