// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_BaseCharacter.h"

#include "Ability/L2H_AbilitySet.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"
#include "Character/L2H_CoverComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Grab/L2H_GrabbableInterface.h"
#include "Item/L2H_InventoryComponent.h"
#include "Item/L2H_ItemDataAsset.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "Save/L2H_SaveSubsystem.h"
#include "UI/L2H_TimeMenuWidget.h"
#include "UI/L2H_TimeRequestPromptWidget.h"
#include "World/L2H_CheckpointTerminal.h"
#include "World/L2H_PingMarker.h"
#include "Enemy/L2H_EnemyRobot.h"
#include "Time/L2H_AbilitySystemComponent.h"
#include "Time/L2H_TimeAttributeSet.h"
#include "Grab/L2H_MassProfileComponent.h"
#include "Time/L2H_LevelTimeConfig.h"
#include "Time/L2HTimeLog.h"
#include "Time/L2H_TimeDirector.h"
#include "UI/L2H_TimeClockWidget.h"
#include "UI/L2H_TimeQuickMenuWidget.h"
#include "Weapon/L2H_WeaponActor.h"
#include "Weapon/L2H_WeaponComponent.h"
#include "Weapon/L2H_WeaponDataAsset.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

AL2H_BaseCharacter::AL2H_BaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// The whole time layer is server-authoritative, so the pawn replicates and the ASC
	// component added on the Blueprint child inherits this flag.
	bReplicates = true;
	// bReplicateMovement is a private bitfield in 5.8; the accessor is the only way in.
	SetReplicateMovement(true);

	// Camera. The arm is the only thing that rotates with pitch, so the mesh keeps its own facing.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Character movement. Walk/crouch-walk 250, run 500, sprint 700 (500 * SprintMultiplier).
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 500.0f;
		Move->MaxWalkSpeedCrouched = 250.0f;
		Move->JumpZVelocity = 450.0f;
		Move->AirControl = 0.35f;
		Move->MaxAcceleration = 2048.0f;
		Move->BrakingDecelerationWalking = 2048.0f;
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	}

	// The movement decides facing, not the controller.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// The aiming laser is refreshed from Tick. Enabled here rather than on the class so a Blueprint child
	// that wants no per-frame work can still turn it back off.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Markers: the laser dot, the throw arc, the hook rope and the caught point. Plain engine shapes in a
	// glowing unlit material, placed in world space every frame, never colliding and never casting shadows.
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		auto Material = [](const TCHAR* Name) -> UMaterialInterface*
		{
			const FString Path = FString::Printf(TEXT("/Game/_Project/Weapons/FX/%s.%s"), Name, Name);
			return LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		};
		auto Setup = [this](UStaticMeshComponent* Comp, UStaticMesh* ShapeMesh, UMaterialInterface* Mat)
		{
			Comp->SetupAttachment(GetRootComponent());
			Comp->SetUsingAbsoluteLocation(true);
			Comp->SetUsingAbsoluteRotation(true);
			Comp->SetUsingAbsoluteScale(true);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetGenerateOverlapEvents(false);
			Comp->SetCastShadow(false);
			Comp->SetHiddenInGame(false);
			Comp->SetVisibility(false);
			Comp->SetStaticMesh(ShapeMesh);
			if (Mat)
			{
				Comp->SetMaterial(0, Mat);
			}
		};

		AimDot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("L2H_AimDot"));
		Setup(AimDot, Sphere, Material(TEXT("MI_L2H_LaserDot")));
		AimDot->SetOnlyOwnerSee(true);

		ThrowArc = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("L2H_ThrowArc"));
		Setup(ThrowArc, Sphere, Material(TEXT("MI_L2H_ThrowArc")));
		ThrowArc->SetOnlyOwnerSee(true);
		ThrowArc->SetWorldTransform(FTransform::Identity);

		HookRope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("L2H_HookRope"));
		Setup(HookRope, Cylinder, Material(TEXT("MI_L2H_Rope")));

		HookMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("L2H_HookMarker"));
		Setup(HookMarker, Sphere, Material(TEXT("MI_L2H_HookAnchor")));

		// The carry beams live on the mesh, not in world space: instances are written in the mesh's own
		// space, so they travel with the body in the same frame as the hands that emit them.
		CarryBeams = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("L2H_CarryBeams"));
		Setup(CarryBeams, Cylinder, Material(TEXT("MI_L2H_LaserDot")));
		CarryBeams->SetupAttachment(GetMesh());
		CarryBeams->SetUsingAbsoluteLocation(false);
		CarryBeams->SetUsingAbsoluteRotation(false);
		CarryBeams->SetUsingAbsoluteScale(false);
	}

	// The code-built time menu works with no Widget Blueprint at all.
	TimeQuickMenuClass = UL2H_TimeQuickMenuWidget::StaticClass();
	TimeQuickMenuContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/L2HTime/Input/IMC_L2H_TimeMenu.IMC_L2H_TimeMenu"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	// Carry clips from the deliveryman pack, retargeted to UEFN. Defaults only; the Blueprint may override.
	{
		auto Clip = [](const TCHAR* Name) -> UAnimSequenceBase*
		{
			const FString Path = FString::Printf(TEXT("/Game/_Project/animation/L2H/Carry/Clips/AnimSeq_Carrying_%s_UEFN.AnimSeq_Carrying_%s_UEFN"), Name, Name);
			return LoadObject<UAnimSequenceBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		};
		CarryPickupLight = Clip(TEXT("BothArms_RightArm_BoxWithHandle_Floor_PickUp"));
		CarryPutDownLight = Clip(TEXT("BothArms_RightArm_BoxWithHandle_Floor_PutDown"));
		CarryPickupHeavy = Clip(TEXT("BothArms_LargeBox_Floor_PickUp"));
		CarryPutDownHeavy = Clip(TEXT("BothArms_LargeBox_Floor_PutDown"));
		CarryPersonStart = Clip(TEXT("HumanTransport_Worker_Start"));
		CarryPersonEnd = Clip(TEXT("HumanTransport_Worker_End"));
		RiderIdle = Clip(TEXT("HumanTransport_Customer_Idle"));
		RiderMove = Clip(TEXT("HumanTransport_Customer_Walks"));
	}

	GetUpBackMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/_Project/animation/GASPALS/GetUp/M_Neutral_GetUp_Back_Montage_Default.M_Neutral_GetUp_Back_Montage_Default"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	GetUpFrontMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/_Project/animation/GASPALS/GetUp/M_Neutral_GetUp_Front_Montage_Default.M_Neutral_GetUp_Front_Montage_Default"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// GAS. Created here rather than on the Blueprint child: a UAttributeSet is a UObject and
	// cannot be a Blueprint component, and the ASC only registers attribute sets that are
	// subobjects of the owner actor. Replication mode is set by the component itself.
	AbilitySystemComponent = CreateDefaultSubobject<UL2H_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	TimeAttributeSet = CreateDefaultSubobject<UL2H_TimeAttributeSet>(TEXT("TimeAttributeSet"));
	CoverComponent = CreateDefaultSubobject<UL2H_CoverComponent>(TEXT("L2H_Cover"));
	CoverAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Crouch.IA_Crouch"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	// Mapped only by IMC_L2H_Cover, which the cover component adds while in cover over the Blueprint's input.
	CoverJumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/L2HTime/Input/IA_L2H_CoverCross.IA_L2H_CoverCross"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	// Mannequin default. The Blueprint child may override this; it is here so the class is
	// usable without one.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
		MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
}

void AL2H_BaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Captured before any ragdoll can move the mesh, so ending the ragdoll can put it back
	// exactly where the Blueprint authored it -- whatever mesh that Blueprint uses.
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshRelativeLocation = MeshComp->GetRelativeLocation();
		MeshRelativeRotation = MeshComp->GetRelativeRotation();
	}

	// The aim dot wears the laser's own material, so it is the same colour as the beam that points at it:
	// a white laser leaves a white dot on whatever it touches.
	if (AimDot)
	{
		TArray<UStaticMeshComponent*> Meshes;
		GetComponents<UStaticMeshComponent>(Meshes);
		for (const UStaticMeshComponent* Comp : Meshes)
		{
			if (Comp && Comp->GetName() == TEXT("AimLaser") && Comp->GetMaterial(0))
			{
				// Not the carry beams: they are instanced, and the laser's material is not flagged for
				// instanced meshes, so it rendered as the dark default (measured in a capture).
				AimDot->SetMaterial(0, Comp->GetMaterial(0));
				break;
			}
		}
	}

	// The ASC initialises its own actor info from its BeginPlay, but the order of component and actor
	// BeginPlay is not something to rely on. Re-asserting it here is idempotent and removes the
	// ordering question entirely, so the grant below always lands on an initialised component.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// The base kit every player has. GrantTo is a no-op off the authority, so this is safe to reach
	// from a client's copy of the pawn as well.
	GrantDefaultAbilities();
}

void AL2H_BaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyTimeClock();
	Super::EndPlay(EndPlayReason);
}

void AL2H_BaseCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Runs on possession and unpossession, on the server directly and on the owning client via
	// OnRep_Controller. Rebuilding here is what gives the split-screen second player its own clock.
	DestroyTimeClock();
	CreateTimeClock();
}

void AL2H_BaseCharacter::CreateTimeClock()
{
	if (TimeClockWidget || !TimeClockWidgetClass || !IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	TimeClockWidget = CreateWidget<UL2H_TimeClockWidget>(PC, TimeClockWidgetClass);
	if (!TimeClockWidget)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("%s: could not create the time clock"), *GetNameSafe(this)));
		return;
	}

	TimeClockWidget->SetTimeSource(this);

	// Player screen, not viewport: in split screen each clock stays inside its own player's view.
	TimeClockWidget->AddToPlayerScreen(0);
}

void AL2H_BaseCharacter::DestroyTimeClock()
{
	if (TimeClockWidget)
	{
		TimeClockWidget->RemoveFromParent();
		TimeClockWidget = nullptr;
	}
}

void AL2H_BaseCharacter::RefreshTimeClock()
{
	if (TimeClockWidget)
	{
		TimeClockWidget->RefreshClock();
	}
}

// -------------------------------------------------------------------------------------------
// Test commands
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::L2HTimeDamage(int32 PlayerIndex, float Amount)
{
	RunTimeTestCommand(PlayerIndex, Amount, /*bRestore=*/false);
}

void AL2H_BaseCharacter::L2HTimeRestore(int32 PlayerIndex, float Amount)
{
	RunTimeTestCommand(PlayerIndex, Amount, /*bRestore=*/true);
}

void AL2H_BaseCharacter::L2HTimeReset()
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HTimeReset: test commands run on the host only"));
		return;
	}

	for (TActorIterator<AL2H_TimeDirector> It(GetWorld()); It; ++It)
	{
		It->ResetRun();
		L2HTime::LogImportant(TEXT("L2HTimeReset: run reset"));
		return;
	}

	L2HTime::LogImportant(TEXT("L2HTimeReset: no L2H_TimeDirector in this level"));
}

void AL2H_BaseCharacter::L2HTimeSave(int32 PlayerIndex)
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HTimeSave: test commands run on the host only"));
		return;
	}

	ACharacter* Target = UGameplayStatics::GetPlayerCharacter(this, PlayerIndex);
	if (!Target)
	{
		L2HTime::LogImportant(TEXT("L2HTimeSave: no player at that index"));
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		L2HTime::LogImportant(TEXT("L2HTimeSave: no L2H_SaveSubsystem"));
		return;
	}

	// SaveForTime charges the payer before it writes, so a refusal costs nothing and logs its own reason.
	const bool bSaved = SaveSubsystem->SaveForTime(Target);

	L2HTime::LogImportant(FString::Printf(TEXT("L2HTime test: save %s for %s -> now %.1fs"),
		bSaved ? TEXT("written") : TEXT("refused"), *GetNameSafe(Target),
		AL2H_TimeDirector::GetTimeFor(Target)));
}

void AL2H_BaseCharacter::ServerTravelToCheckpoint_Implementation(FName CheckpointId)
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		L2HTime::LogImportant(TEXT("Travel: refused -- no L2H_SaveSubsystem on this game instance"));
		return;
	}

	if (!SaveSubsystem->TravelToCheckpoint(this, CheckpointId))
	{
		return;
	}

	// The menu was opened at the checkpoint the players just left, so it is describing a place they
	// are no longer standing in. This closes it on a listen server, where the menu really is on this
	// object; on a client the widget is closed by the caller, because a widget is not replicated and
	// the server's copy of a remote pawn has none.
	CloseTerminalMenu();
}

void AL2H_BaseCharacter::ServerAdoptDesignerTime_Implementation()
{
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (!Config || Config->DesignerTime <= 0.0f)
	{
		L2HTime::LogImportant(TEXT("DesignerTime: refused -- this level has no designer time set"));
		return;
	}

	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		L2HTime::LogImportant(TEXT("DesignerTime: refused -- no L2H_TimeDirector in this level"));
		return;
	}

	// The offer replaces a player's time, it does not top it up, and its whole point is to say what
	// the level costs to finish. Applying it to somebody already above that number would be the
	// opposite of the promise, and would quietly take their seconds away, so it is refused instead.
	const float Current = AL2H_TimeDirector::GetTimeFor(this);
	if (Current >= Config->DesignerTime)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("DesignerTime: refused -- %.1fs is not below the designer time of %.1fs"),
			Current, Config->DesignerTime));
		return;
	}

	Director->ApplyTimeTo(this, Config->DesignerTime);

	L2HTime::LogImportant(FString::Printf(TEXT("DesignerTime: %s adopted %.1fs"),
		*GetName(), Config->DesignerTime));
}

void AL2H_BaseCharacter::L2HTimeCheckpoints()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		L2HTime::LogImportant(TEXT("L2HTimeCheckpoints: no L2H_SaveSubsystem"));
		return;
	}

	const TArray<FL2H_CheckpointRecord> Run = SaveSubsystem->GetCheckpoints(this);
	if (Run.Num() == 0)
	{
		L2HTime::LogImportant(TEXT("L2HTimeCheckpoints: the run has no checkpoints yet"));
		return;
	}

	const FName Standing = CurrentTerminal ? CurrentTerminal->GetCheckpointId() : NAME_None;
	const TArray<FL2H_CheckpointRecord> Options = SaveSubsystem->GetTravelOptions(this, Standing);

	L2HTime::LogImportant(FString::Printf(TEXT("L2HTimeCheckpoints: %d in the run, standing in %s"),
		Run.Num(), *Standing.ToString()));

	for (int32 Index = 0; Index < Run.Num(); ++Index)
	{
		const FL2H_CheckpointRecord& Record = Run[Index];
		const bool bRewindable = Options.ContainsByPredicate(
			[&Record](const FL2H_CheckpointRecord& Option) { return Option.Id == Record.Id; });

		L2HTime::LogImportant(FString::Printf(TEXT("  [%d] %s \"%s\" -- %d player(s)%s"),
			Index, *Record.Id.ToString(), *Record.DisplayName.ToString(), Record.Players.Num(),
			bRewindable ? TEXT("  [rewindable]") : TEXT("")));
	}
}

void AL2H_BaseCharacter::L2HTimeTravel(int32 Index)
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HTimeTravel: test commands run on the host only"));
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		L2HTime::LogImportant(TEXT("L2HTimeTravel: no L2H_SaveSubsystem"));
		return;
	}

	const TArray<FL2H_CheckpointRecord> Run = SaveSubsystem->GetCheckpoints(this);
	if (!Run.IsValidIndex(Index))
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("L2HTimeTravel: index %d is outside the run's %d checkpoint(s)"), Index, Run.Num()));
		return;
	}

	SaveSubsystem->TravelToCheckpoint(this, Run[Index].Id);
}

void AL2H_BaseCharacter::L2HTimeDesigner(int32 PlayerIndex)
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HTimeDesigner: test commands run on the host only"));
		return;
	}

	AL2H_BaseCharacter* Target =
		Cast<AL2H_BaseCharacter>(UGameplayStatics::GetPlayerCharacter(this, PlayerIndex));
	if (!Target)
	{
		L2HTime::LogImportant(TEXT("L2HTimeDesigner: no player at that index"));
		return;
	}

	// Called rather than sent: on the host the Server RPC is the local body, so this exercises
	// exactly the path a client's button would take.
	Target->ServerAdoptDesignerTime();
}

void AL2H_BaseCharacter::RunTimeTestCommand(int32 PlayerIndex, float Amount, bool bRestore)
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HTime test commands run on the host only"));
		return;
	}

	AL2H_TimeDirector* Director = nullptr;
	for (TActorIterator<AL2H_TimeDirector> It(GetWorld()); It; ++It)
	{
		Director = *It;
		break;
	}

	ACharacter* Target = UGameplayStatics::GetPlayerCharacter(this, PlayerIndex);
	if (!Director || !Target)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("L2HTime test: missing %s"),
			Director ? TEXT("player at that index") : TEXT("L2H_TimeDirector")));
		return;
	}

	if (bRestore)
	{
		Director->ApplyTimeRestore(Target, Amount);
	}
	else
	{
		Director->ApplyTimeDamage(Target, Amount);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("L2HTime test: %s %.1fs on %s -> now %.1fs"),
		bRestore ? TEXT("restored") : TEXT("damaged"), FMath::Abs(Amount), *GetNameSafe(Target),
		AL2H_TimeDirector::GetTimeFor(Target)));
}

void AL2H_BaseCharacter::L2HHealthDamage(int32 PlayerIndex, float Amount)
{
	RunHealthTestCommand(PlayerIndex, Amount, /*bRestore=*/false);
}

void AL2H_BaseCharacter::L2HHealthRestore(int32 PlayerIndex, float Amount)
{
	RunHealthTestCommand(PlayerIndex, Amount, /*bRestore=*/true);
}

void AL2H_BaseCharacter::RunHealthTestCommand(int32 PlayerIndex, float Amount, bool bRestore)
{
	if (!HasAuthority())
	{
		L2HTime::LogImportant(TEXT("L2HHealth test commands run on the host only"));
		return;
	}

	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	ACharacter* Target = UGameplayStatics::GetPlayerCharacter(this, PlayerIndex);
	if (!Director || !Target)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("L2HHealth test: missing %s"),
			Director ? TEXT("player at that index") : TEXT("L2H_TimeDirector")));
		return;
	}

	// Named the damage path on purpose. ApplyHealthDamage is the health layer's own entry -- the one
	// the director routes a hazard into -- so a test that used it exercises the same code a bullet
	// does, rather than a shortcut that could hide a break in the routing.
	if (bRestore)
	{
		Director->ApplyHealthRestore(Target, Amount);
	}
	else
	{
		Director->ApplyHealthDamage(Target, Amount);
	}

	// Both numbers are logged because both are supposed to move: the health bar from the hit, and the
	// clock from the translation. A test that only reported health could not see the translation fail.
	L2HTime::LogImportant(FString::Printf(TEXT("L2HHealth test: %s %.1f on %s -> %.1f hp, %.1fs"),
		bRestore ? TEXT("restored") : TEXT("damaged"), FMath::Abs(Amount), *GetNameSafe(Target),
		AL2H_TimeDirector::GetHealthFor(Target), AL2H_TimeDirector::GetTimeFor(Target)));
}

void AL2H_BaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_BaseCharacter, bDowned);
	DOREPLIFETIME(AL2H_BaseCharacter, bBeingCarried);

	// Death travels with the state flags: the HUD on the other screen has to be able to tell "downed,
	// waiting to be picked up" apart from "dead, waiting for the run to rewind".
	DOREPLIFETIME(AL2H_BaseCharacter, bDead);
	DOREPLIFETIME(AL2H_BaseCharacter, FreeReviveDeadline);
	DOREPLIFETIME(AL2H_BaseCharacter, Chrons);
	DOREPLIFETIME(AL2H_BaseCharacter, bHubBankOpen);
	DOREPLIFETIME(AL2H_BaseCharacter, HubCarryAllowance);
	DOREPLIFETIME(AL2H_BaseCharacter, HubCarried);
	DOREPLIFETIME(AL2H_BaseCharacter, LastHubId);
	DOREPLIFETIME(AL2H_BaseCharacter, ShieldHits);
	DOREPLIFETIME(AL2H_BaseCharacter, LastTransferGiveTime);
	DOREPLIFETIME(AL2H_BaseCharacter, PendingReviveSeconds);

	// The hook: every machine draws the rope to the anchor, so both points travel with the flag.
	DOREPLIFETIME(AL2H_BaseCharacter, HookAnchor);
	DOREPLIFETIME(AL2H_BaseCharacter, HookLandPoint);
	DOREPLIFETIME(AL2H_BaseCharacter, bHookToLedge);
	DOREPLIFETIME(AL2H_BaseCharacter, HookFaceYaw);
	DOREPLIFETIME(AL2H_BaseCharacter, bHookSwinging);

	// Socket before actor on purpose: both arrive in the same bunch and are applied in this order, so
	// OnRep_CarriedActor always has the socket in hand by the time it fires for the actor.
	DOREPLIFETIME(AL2H_BaseCharacter, ActiveCarrySocket);
	DOREPLIFETIME(AL2H_BaseCharacter, CarriedActor);

	// Counted so both screens, and the pawn's own cap, agree on whether another shard will fit.
	DOREPLIFETIME(AL2H_BaseCharacter, CarriedShardCount);

	// The live time request. It replicates here rather than on the director, because the director does
	// not replicate and the partner's own screen is what has to show the question.
	DOREPLIFETIME(AL2H_BaseCharacter, PendingRequest);
}

UAbilitySystemComponent* AL2H_BaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AL2H_BaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// The mapping context is added here rather than in BeginPlay. SetupPlayerInputComponent only
	// runs for a locally controlled pawn, and by then both the controller and its LocalPlayer are
	// valid -- which is what makes the second split-screen player work, since that player is
	// created after the world is already running.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
			else
			{
				L2HTime::LogImportant(TEXT("L2H_BaseCharacter: DefaultMappingContext is unset -- no input will fire"));
			}
		}
	}

	// The request strip is per local player and lives as long as the pawn: it is collapsed almost
	// always, so there is no reason to pay for creating one on every single request.
	if (APlayerController* LocalPC = Cast<APlayerController>(GetController()))
	{
		if (RequestPromptClass)
		{
			RequestPrompt = CreateWidget<UL2H_TimeRequestPromptWidget>(LocalPC, RequestPromptClass);
			if (RequestPrompt)
			{
				RequestPrompt->AddToPlayerScreen(0);

				// Collapsed from the first frame, so an empty strip never sits at the screen edge and
				// never competes with the game for input.
				RequestPrompt->SetRequest(false, FText::GetEmpty(), 0.0f, 0.0f);
			}
		}
		else
		{
			L2HTime::LogImportant(
				TEXT("L2H_BaseCharacter: RequestPromptClass is unset -- time requests stay invisible"));
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("L2H_BaseCharacter: input component is %s, expected UEnhancedInputComponent"),
			*GetNameSafe(PlayerInputComponent)));
		return;
	}

	// The menu and the two request answers are one-shot presses, so Started is the right edge.
	if (MenuAction)
	{
		EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_Menu);
	}
	if (RequestAcceptAction)
	{
		EIC->BindAction(RequestAcceptAction, ETriggerEvent::Started, this,
			&AL2H_BaseCharacter::Input_RequestAccept);
	}
	if (RequestDeclineAction)
	{
		EIC->BindAction(RequestDeclineAction, ETriggerEvent::Started, this,
			&AL2H_BaseCharacter::Input_RequestDecline);
	}
	if (TimeQuickMenuAction)
	{
		EIC->BindAction(TimeQuickMenuAction, ETriggerEvent::Started, this,
			&AL2H_BaseCharacter::Input_TimeQuickMenu);
	}

	// Triggered = held (movement and look), Started/Completed = the press and the release.
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AL2H_BaseCharacter::Input_Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AL2H_BaseCharacter::Input_Look);
	}
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_JumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AL2H_BaseCharacter::Input_JumpCompleted);
	}
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_SprintStarted);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &AL2H_BaseCharacter::Input_SprintCompleted);
	}
	if (InteractAction)
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_Interact);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &AL2H_BaseCharacter::Input_InteractReleased);
	}
	if (CoverAction)
	{
		EIC->BindAction(CoverAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_Cover);
	}
	if (CoverJumpAction)
	{
		EIC->BindAction(CoverJumpAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_CoverJump);
	}
	if (HookAction)
	{
		EIC->BindAction(HookAction, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_Hook);
	}

	// Character abilities, bound from data. The tag rides along as a binding payload, so one handler
	// serves every row: adding a super ability is a row on the character Blueprint plus a granted
	// ability, not a new binding here. A row missing either half is skipped rather than bound to
	// nothing, because a half-filled row is a designer mid-edit, not an error.
	for (const FL2H_AbilityInputBinding& Binding : AbilityInputBindings)
	{
		if (Binding.Action && Binding.AbilityTag.IsValid())
		{
			EIC->BindAction(Binding.Action, ETriggerEvent::Started, this, &AL2H_BaseCharacter::Input_AbilityByTag, Binding.AbilityTag);

			// Held: only the fire row uses it, and only for an automatic weapon (see Input_AbilityHeld).
			EIC->BindAction(Binding.Action, ETriggerEvent::Triggered, this, &AL2H_BaseCharacter::Input_AbilityHeld, Binding.AbilityTag);
		}
	}
}

// -------------------------------------------------------------------------------------------
// Input handlers
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::Input_Move(const FInputActionValue& Value)
{
	// Axis2D on the asset. If this silently does nothing, the IA's value type is not Axis2D.
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero() || IsDowned() || IsBeingCarried())
	{
		return;
	}

	const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
}

void AL2H_BaseCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AL2H_BaseCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	Jump();
}

void AL2H_BaseCharacter::Input_JumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

void AL2H_BaseCharacter::Input_SprintStarted(const FInputActionValue& Value)
{
	SetSprinting(true);
}

void AL2H_BaseCharacter::Input_SprintCompleted(const FInputActionValue& Value)
{
	SetSprinting(false);
}

void AL2H_BaseCharacter::Input_Interact(const FInputActionValue& Value)
{
	// Started rather than Triggered: one press is one grab or one drop, and a held button must not
	// pick the object straight back up on the next frame.
	// On the floor the button only swaps the view between the body and the partner.
	if (IsDowned() || IsDead())
	{
		if (IsLocallyControlled() && DownViewSince >= 0.0f)
		{
			bDownViewOwnBody = !bDownViewOwnBody;
		}
		return;
	}

	// A checkpoint is a place rather than an object, so the trace the interact ability runs could never
	// find it. It is also the one verb here that is pure UI, resolved on the machine showing the menu,
	// so it is answered before any ability is asked. One key opens and closes.
	if (CurrentTerminal && CurrentTerminal->OffersMenu())
	{
		if (TerminalMenu)
		{
			CloseTerminalMenu();
		}
		else
		{
			TryOpenTerminalMenu();
		}
		return;
	}

	// A partner inside their free revive window: the press becomes a hold. Held to the end it revives;
	// let go early and it is the ordinary press (picking the body up), sent on release.
	if (AL2H_BaseCharacter* ReviveTarget = FindFreeReviveTarget())
	{
		ReviveHoldTarget = ReviveTarget;
		ReviveHoldStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		return;
	}

	ServerTryActivateAbilityByClass(InteractAbilityClass);
}

void AL2H_BaseCharacter::Input_InteractReleased(const FInputActionValue& Value)
{
	if (ReviveHoldStart < 0.0f)
	{
		return;
	}

	// Released before the hold completed (UpdateReviveHold clears it when it does): a plain press.
	ReviveHoldStart = -1.0f;
	ReviveHoldTarget = nullptr;
	ServerTryActivateAbilityByClass(InteractAbilityClass);
}

AL2H_BaseCharacter* AL2H_BaseCharacter::FindFreeReviveTarget() const
{
	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	const UWorld* World = GetWorld();
	if (!LevelConfig || !World || bDead || bDowned || bBeingCarried || IsCarrying())
	{
		return nullptr;
	}

	// The director does not replicate, so the candidates are found the way a client can: every pawn of
	// this class in the world. A co-op run has two, so this is never a long walk.
	for (TActorIterator<AL2H_BaseCharacter> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AL2H_BaseCharacter* Other = *It;
		if (Other && Other != this && CanHoldReviveOn(Other) && !Other->IsBeingCarried()
			&& FVector::Dist(Other->GetActorLocation(), GetActorLocation()) <= LevelConfig->FreeReviveRange)
		{
			return Other;
		}
	}
	return nullptr;
}

bool AL2H_BaseCharacter::CanHoldReviveOn(const AL2H_BaseCharacter* Other) const
{
	// Free inside the window; after it, or for a partner down to the clock, only with a tool to spend.
	return Other && (Other->IsFreeReviveOpen() || AL2H_TimeDirector::FindReviveToolCell(this, Other) != INDEX_NONE);
}

void AL2H_BaseCharacter::UpdateReviveHold()
{
	if (ReviveHoldStart < 0.0f)
	{
		return;
	}

	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	AL2H_BaseCharacter* Target = ReviveHoldTarget.Get();

	// The window closed, the partner was picked up, or this player walked off or went down: the hold
	// is dropped without doing anything, and the button has to be pressed again.
	if (!LevelConfig || !Target || !CanHoldReviveOn(Target) || bDead || bDowned
		|| FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > LevelConfig->FreeReviveRange + 50.0f)
	{
		ReviveHoldStart = -1.0f;
		ReviveHoldTarget = nullptr;
		return;
	}

	if (GetReviveHoldProgress() >= 1.0f)
	{
		ReviveHoldStart = -1.0f;
		ReviveHoldTarget = nullptr;
		ServerFreeRevive(Target);
	}
}

float AL2H_BaseCharacter::GetReviveHoldProgress() const
{
	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	if (ReviveHoldStart < 0.0f || !LevelConfig || !GetWorld())
	{
		return -1.0f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - ReviveHoldStart) / LevelConfig->FreeReviveHoldTime, 0.0f, 1.0f);
}

void AL2H_BaseCharacter::ServerFreeRevive_Implementation(AL2H_BaseCharacter* Target)
{
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->TryFreeRevive(this, Target);
	}
}

float AL2H_BaseCharacter::GetFreeReviveSecondsLeft() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!bDead || FreeReviveDeadline <= 0.0f || !GameState)
	{
		return 0.0f;
	}
	return FMath::Max(FreeReviveDeadline - static_cast<float>(GameState->GetServerWorldTimeSeconds()), 0.0f);
}

bool AL2H_BaseCharacter::IsFreeReviveOpen() const
{
	return GetFreeReviveSecondsLeft() > 0.0f;
}

void AL2H_BaseCharacter::AddChrons(float Delta)
{
	if (HasAuthority())
	{
		Chrons = FMath::Max(Chrons + Delta, 0.0f);
	}
}

void AL2H_BaseCharacter::SetChrons(float Value)
{
	if (HasAuthority())
	{
		Chrons = FMath::Max(Value, 0.0f);
	}
}

void AL2H_BaseCharacter::OpenHubBank(float Allowance)
{
	if (HasAuthority())
	{
		bHubBankOpen = true;
		HubCarryAllowance = FMath::Max(Allowance, 0.0f);
		HubCarried = 0.0f;
	}
}

void AL2H_BaseCharacter::CloseHubBank()
{
	if (HasAuthority())
	{
		bHubBankOpen = false;
		HubCarryAllowance = 0.0f;
		HubCarried = 0.0f;
	}
}

void AL2H_BaseCharacter::ServerAdjustCarry_Implementation(int32 Direction)
{
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->AdjustHubCarry(this, FMath::Clamp(Direction, -1, 1));
	}
}

void AL2H_BaseCharacter::SyncDownDrainPause()
{
	// Back on their feet by any route: seconds still waiting for a remote revive are moot.
	if (!bDead && !bDowned)
	{
		PendingReviveSeconds = 0.0f;
	}

	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->SetDrainPaused(this, EL2H_DrainPause::Down, bDead || bDowned);
	}
}

void AL2H_BaseCharacter::Input_Hook(const FInputActionValue& Value)
{
	if (IsDowned())
	{
		return;
	}

	// A press while already travelling lets go: the flight carries on as a plain fall.
	if (bHookSwinging)
	{
		ReleaseHookSwing();
		return;
	}

	ServerTryActivateAbilityByClass(HookAbilityClass);
}

void AL2H_BaseCharacter::Input_AbilityByTag(const FInputActionValue& Value, FGameplayTag AbilityTag)
{
	// A super ability spends time, and time is server state, so it takes the same route as the base
	// kit. Only the locally controlled pawn answers input; a simulated proxy has none to answer.
	if (!IsLocallyControlled())
	{
		return;
	}

	// D-pad Down is the weapon switch; while the time menu is open it moves the selection instead.
	static const FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Switch"), /*ErrorIfNotFound=*/false);
	if (TimeQuickMenu && SwitchTag.IsValid() && AbilityTag == SwitchTag)
	{
		MoveTimeQuickMenu(+1);
		return;
	}

	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Fire"), /*ErrorIfNotFound=*/false);
	if (FireTag.IsValid() && AbilityTag == FireTag && GetWorld())
	{
		LastAutoFireRequest = GetWorld()->GetTimeSeconds();
	}

	ServerTryActivateAbilityByTag(AbilityTag);
}

void AL2H_BaseCharacter::Input_AbilityHeld(const FInputActionValue& Value, FGameplayTag AbilityTag)
{
	// Automatic fire. The press itself already fired through Input_AbilityByTag; while the button stays
	// down an automatic weapon asks again once per 1/FireRate. The server's fire ability enforces the same
	// rate, so a late or doubled request cannot fire faster than the weapon allows. The rifle was authored
	// automatic and fired one round per press, because nothing read the flag.
	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Fire"), /*ErrorIfNotFound=*/false);
	if (!IsLocallyControlled() || !FireTag.IsValid() || AbilityTag != FireTag || !GetWorld())
	{
		return;
	}

	const UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>();
	const UL2H_WeaponDataAsset* Data = Weapons ? Weapons->GetActiveWeaponData() : nullptr;
	if (!Data || !Data->Data.bAutomatic || Data->Data.FireRate <= 0.0f || IsCarrying())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAutoFireRequest >= 1.0f / Data->Data.FireRate)
	{
		LastAutoFireRequest = Now;
		ServerTryActivateAbilityByTag(AbilityTag);
	}
}

void AL2H_BaseCharacter::Landed(const FHitResult& Hit)
{
	// The velocity still holds the impact speed here; the movement component zeroes Z after this call.
	const float Impact = -GetVelocity().Z;
	Super::Landed(Hit);

	if (!HasAuthority() || bDead || bDowned)
	{
		return;
	}

	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!LevelConfig || !Director || Impact <= LevelConfig->FallDamageMinSpeed
		|| LevelConfig->FallDamageLethalSpeed <= LevelConfig->FallDamageMinSpeed)
	{
		return;
	}

	// Through the same funnel as a bullet: Health first, and the Time that Health was worth. Dodge
	// i-frames do not apply to the ground, but the funnel checks them anyway, which is harmless.
	const float Fraction = (Impact - LevelConfig->FallDamageMinSpeed) / (LevelConfig->FallDamageLethalSpeed - LevelConfig->FallDamageMinSpeed);
	const float Damage = FMath::Clamp(Fraction, 0.0f, 1.0f) * FMath::Max(LevelConfig->MaxHealth, 1.0f);
	L2HTime::LogImportant(FString::Printf(TEXT("%s landed at %.0f cm/s -> %.0f fall damage"), *GetNameSafe(this), Impact, Damage));
	Director->ApplyTimeDamage(this, Damage);
}

bool AL2H_BaseCharacter::CanJumpInternal_Implementation() const
{
	// A downed pawn is ragdolled and a carried pawn is attached to its carrier; neither may jump. In cover
	// the key crosses to the next cover instead.
	if (IsDowned() || IsBeingCarried() || (CoverComponent && CoverComponent->IsInCover()))
	{
		return false;
	}

	return Super::CanJumpInternal_Implementation();
}

// -------------------------------------------------------------------------------------------
// Movement speed: cache -> change -> restore
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::RefreshWalkSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	// Captured lazily, so whatever the Blueprint authored is what gets restored.
	if (!bWalkSpeedCached)
	{
		CachedWalkSpeed = Move->MaxWalkSpeed;
		bWalkSpeedCached = true;
	}

	Move->MaxWalkSpeed = CachedWalkSpeed * SprintScale * CarryScale * AimScale;
}

void AL2H_BaseCharacter::SetSprinting(bool bNewSprinting)
{
	SprintScale = bNewSprinting ? SprintMultiplier : 1.0f;
	RefreshWalkSpeed();
}

void AL2H_BaseCharacter::SetCarrySpeedScale(float NewScale)
{
	CarryScale = FMath::Max(NewScale, 0.0f);
	RefreshWalkSpeed();
}

void AL2H_BaseCharacter::SetAimSpeedScale(float NewScale)
{
	AimScale = FMath::Max(NewScale, 0.0f);
	RefreshWalkSpeed();
}

void AL2H_BaseCharacter::SetAimFieldOfView(float NewFieldOfView)
{
	if (!FollowCamera)
	{
		return;
	}

	// Captured lazily, so what gets restored is whatever the Blueprint authored -- and only once, or a
	// second call would store an already-narrowed FOV as the value to go back to.
	if (!bFieldOfViewCached)
	{
		CachedFieldOfView = FollowCamera->FieldOfView;
		bFieldOfViewCached = true;
	}

	FollowCamera->SetFieldOfView(NewFieldOfView > 0.0f ? NewFieldOfView : CachedFieldOfView);
}

void AL2H_BaseCharacter::GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	// The camera is what the crosshair sits on. A player controller knows it on every machine that
	// matters: locally from the camera manager, on the server from the client's ServerUpdateCamera.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	GetActorEyesViewPoint(OutLocation, OutRotation);
}

float AL2H_BaseCharacter::GetAimTwistCorrection() const
{
	// The shot leaves the muzzle for the point under the camera's centre, but the aiming pose points the
	// gun straight ahead of the body, and the camera sits over a shoulder: up close the barrel and the shot
	// visibly disagree. The torso is turned (the same spine twist the carry uses) until the barrel's yaw
	// matches the line from the muzzle to the aim point. Closed loop: the barrel is measured as it is
	// drawn, so the correction already applied is part of what is measured.
	float YawError = 0.0f;
	float PitchError = 0.0f;
	const UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>();
	if (!Weapons || !Weapons->IsAiming())
	{
		return 0.0f;
	}
	if (!MeasureBarrelError(YawError, PitchError))
	{
		return CarryTwist;
	}
	return FMath::Clamp(CarryTwist + YawError, -AimMaxTwist, AimMaxTwist);
}

void AL2H_BaseCharacter::Input_Cover(const FInputActionValue& Value)
{
	if (CoverComponent && IsLocallyControlled())
	{
		CoverComponent->NotifyCoverKeyPressed();
	}
}

void AL2H_BaseCharacter::Input_CoverJump(const FInputActionValue& Value)
{
	if (CoverComponent && CoverComponent->IsInCover() && IsLocallyControlled())
	{
		CoverComponent->NotifyJumpKeyPressed();
	}
}

void AL2H_BaseCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (CoverComponent && CoverComponent->IsInCover())
	{
		// The Blueprint passes the axis' sign in ScaleValue (stick left = right vector x -1); folded in
		// first, or a direction the cover remaps would have its sign flipped again afterwards.
		WorldDirection *= ScaleValue;
		ScaleValue = 1.0f;
		WorldDirection = CoverComponent->ConstrainMovementInput(WorldDirection, ScaleValue);
		if (WorldDirection.IsNearlyZero())
		{
			return;
		}
	}
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

FRotator AL2H_BaseCharacter::GetBaseAimRotation() const
{
	FRotator Aim = Super::GetBaseAimRotation();
	Aim.Pitch += AimPitchCorrection;
	return Aim;
}

bool AL2H_BaseCharacter::MeasureBarrelError(float& OutYawError, float& OutPitchError) const
{
	OutYawError = 0.0f;
	OutPitchError = 0.0f;

	const UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>();
	const AL2H_WeaponActor* Weapon = Weapons ? Weapons->GetActiveWeapon() : nullptr;
	const UL2H_WeaponDataAsset* Data = Weapons ? Weapons->GetActiveWeaponData() : nullptr;
	const USceneComponent* View = Weapon ? Weapon->GetActiveMeshView() : nullptr;
	if (!Weapons || !Weapons->IsAiming() || !View || !Data || Data->Data.FireSocketName.IsNone()
		|| !View->DoesSocketExist(Data->Data.FireSocketName))
	{
		return false;
	}

	// The barrel's own axis: the Barrel socket sits on it. The grip hangs below it, so Grip -> muzzle tilts
	// with the gun and reads the yaw wrong once the gun points up or down (measured ~10 deg at -30 pitch).
	const FName Rear = View->DoesSocketExist(TEXT("Barrel")) ? FName(TEXT("Barrel")) : FName(TEXT("Grip"));
	if (!View->DoesSocketExist(Rear))
	{
		return false;
	}

	const FVector Muzzle = View->GetSocketLocation(Data->Data.FireSocketName);
	const FVector Barrel = Muzzle - View->GetSocketLocation(Rear);

	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	FVector AimPoint = ViewLocation + ViewRotation.Vector() * 5000.0f;
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("L2H_AimTwist")), false, this);
		Params.AddIgnoredActor(Weapon);
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, AimPoint, ECC_Visibility, Params))
		{
			AimPoint = Hit.ImpactPoint;
		}
	}

	// Only far enough out to mean anything: a point inside the arm's reach would spin the torso around.
	const FVector ToAim = AimPoint - Muzzle;
	if (ToAim.Size2D() < 80.0f || Barrel.Size2D() < 1.0f)
	{
		return false;
	}

	const FRotator AimDir = ToAim.Rotation();
	const FRotator BarrelDir = Barrel.Rotation();
	OutYawError = FRotator::NormalizeAxis(AimDir.Yaw - BarrelDir.Yaw);
	OutPitchError = FRotator::NormalizeAxis(AimDir.Pitch - BarrelDir.Pitch);
	return true;
}

void AL2H_BaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAimBeam();
	UpdateAimState();
	UpdateHookSwing(DeltaSeconds);
	UpdateCarryAnimation(DeltaSeconds);
	UpdateThrowPreview();
	UpdateClimbDownMove(DeltaSeconds);
	UpdateReviveHold();
	UpdateDownView();
	RecordRewindSample();

	// Jump held long enough while hanging: the free jump fires without waiting for the release.
	if (bHangJumpPending && GetWorld() && GetWorld()->GetTimeSeconds() - HangJumpPressTime >= HangHoldTime)
	{
		bHangJumpPending = false;
		HangDirectionalJump();
	}
}

int32 AL2H_BaseCharacter::GetCarryPose() const
{
	if (!CarriedActor)
	{
		return 0;
	}

	if (Cast<AL2H_BaseCharacter>(CarriedActor))
	{
		return 3;
	}

	// Heavy is the carry rules' own line, so the pose agrees with whether a second player was needed.
	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(CarriedActor);
	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	const float SingleCarryMass = LevelConfig ? LevelConfig->SingleCarryMass : 60.0f;
	const bool bHeavy = Profile && (Profile->bRequiresTwoPlayers || Profile->ResolveMass() > SingleCarryMass);

	// Size picks the hold as much as weight does: a light but bulky crate cannot be carried by a handle.
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent());
	const bool bBulky = Root && Root->Bounds.BoxExtent.GetMax() > 35.0f;
	return (bHeavy || bBulky) ? 2 : 1;
}

void AL2H_BaseCharacter::MulticastCarryAnim_Implementation(UAnimSequenceBase* Anim, bool bPickup, FVector_NetQuantize RestLocation)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const UWorld* World = GetWorld();
	if (!Anim || !AnimInstance || !World)
	{
		return;
	}

	// An authored montage plays as itself; a bare clip goes through the DefaultSlot the AnimBP already has.
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Anim))
	{
		AnimInstance->Montage_Play(Montage);
	}
	else
	{
		AnimInstance->PlaySlotAnimationAsDynamicMontage(Anim, TEXT("DefaultSlot"), 0.15f, 0.2f);
	}

	const float Duration = FMath::Max(Anim->GetPlayLength(), 0.3f);

	// The object waits on the floor until the hands reach it in the clip.
	if (bPickup)
	{
		CarryActionStartTime = World->GetTimeSeconds();
		CarryGrabDelay = Duration * CarryGrabFraction;
		CarryRestLocation = RestLocation;
	}

	// The feet stay planted while the clip bends down and comes back up: walking through a pick-up slid the
	// body across the floor with the hands still reaching for where the box had been. Locked on the two
	// machines that move this body; a simulated proxy follows replication.
	if ((HasAuthority() || IsLocallyControlled()) && !bDowned && !bDead && !bBeingCarried)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		GetWorldTimerManager().SetTimer(CarryActionLockTimer, this, &AL2H_BaseCharacter::EndCarryActionLock,
			FMath::Max(Duration * 0.85f, 0.3f), false);
	}
}

void AL2H_BaseCharacter::EndCarryActionLock()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move && !bDowned && !bDead && !bBeingCarried && Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

void AL2H_BaseCharacter::UpdateCarryAnimation(float DeltaSeconds)
{
	const int32 Pose = GetCarryPose();

	// Time since the hands closed on the object. Negative while the pick-up clip is still reaching for it.
	const UWorld* World = GetWorld();
	const float SinceGrab = (CarryActionStartTime < 0.0f || !World)
		? 1000.0f
		: World->GetTimeSeconds() - CarryActionStartTime - CarryGrabDelay;

	// The hold pose comes in once the object is actually in the hands, not while the clip is bending down.
	CarryLayerWeight = FMath::FInterpConstantTo(CarryLayerWeight, (Pose != 0 && SinceGrab >= 0.0f) ? 1.0f : 0.0f, DeltaSeconds, 4.0f);

	// The upper body turns toward the camera while holding an object; the legs keep following the
	// movement. Clamped, so looking behind does not wring the spine, and eased, so a flick of the stick
	// does not snap the torso. A partner on the back is not turned: they ride the whole body.
	const bool bHoldingObject = (Pose == 1 || Pose == 2) && SinceGrab >= 0.0f;
	// The body's facing is the root bone's, not the actor's: GASP's Offset Root Bone lets the actor snap
	// round while the legs catch up through a turn-in-place, and the torso and the load belong to the
	// legs that are actually on screen.
	float BodyYaw = GetActorRotation().Yaw;
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		BodyYaw = MeshComp->GetSocketRotation(TEXT("root")).Yaw - MeshRelativeRotation.Yaw;
	}

	// The torso keeps its facing in the world while the hips turn under it, so the load does not swing out
	// and back every time the legs step round.
	if (bCarryBodyYawValid && bHoldingObject)
	{
		CarryTwist -= FRotator::NormalizeAxis(BodyYaw - LastCarryBodyYaw);
	}
	LastCarryBodyYaw = BodyYaw;
	bCarryBodyYawValid = true;

	float WantedTwist = 0.0f;
	if (bHoldingObject && Controller)
	{
		const float Delta = FRotator::NormalizeAxis(GetControlRotation().Yaw - BodyYaw);

		// Standing still, once the torso is at its limit the whole body turns to the camera the way it
		// does when aiming -- GASP's strafe rotation, so the feet step round with a turn-in-place instead
		// of staying planted while the camera circles. It lets go as soon as the body has caught up.
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const bool bStanding = Move && Move->IsMovingOnGround() && GetVelocity().Size2D() < 30.0f;
		if (bStanding && FMath::Abs(Delta) > CarryMaxTwist)
		{
			bCarryTurning = true;
		}
		else if (!bStanding || FMath::Abs(Delta) < 10.0f)
		{
			// Released on the visible body, not the actor: the actor snaps to the camera on the first frame
			// of strafe rotation, and letting go then (measured) ended the turn before the legs moved.
			bCarryTurning = false;
		}

		WantedTwist = FMath::Clamp(Delta, -CarryMaxTwist, CarryMaxTwist);
	}
	else
	{
		bCarryTurning = false;
		WantedTwist = GetAimTwistCorrection();
	}
	CarryTwist = FMath::Clamp(FMath::FInterpTo(CarryTwist, WantedTwist, DeltaSeconds, 8.0f), -CarryMaxTwist, CarryMaxTwist);

	// Hunched over while hurt: the injured pose comes in at low health and goes as it regenerates. Health
	// replicates, so every machine eases the same weight.
	{
		float WantedInjured = 0.0f;
		const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
		const UL2H_TimeAttributeSet* TimeSet = ASC ? ASC->GetSet<UL2H_TimeAttributeSet>() : nullptr;
		if (TimeSet && TimeSet->GetMaxHealth() > 0.0f && !bDead && !bDowned && !bBeingCarried
			&& TimeSet->GetHealth() <= TimeSet->GetMaxHealth() * InjuredHealthFraction)
		{
			WantedInjured = InjuredMaxAlpha;
		}
		InjuredAlpha = FMath::FInterpConstantTo(InjuredAlpha, WantedInjured, DeltaSeconds, 1.5f);
	}

	// The vertical half of the same loop: the aim sweep is driven by the base aim pitch, so nudging that
	// pitch moves the drawn barrel up or down until it lies on the shot line. Off when not aiming.
	{
		float YawError = 0.0f;
		float PitchError = 0.0f;
		const float WantedPitch = (!bHoldingObject && MeasureBarrelError(YawError, PitchError))
			? FMath::Clamp(AimPitchCorrection + PitchError, -AimMaxPitchCorrection, AimMaxPitchCorrection)
			: 0.0f;
		AimPitchCorrection = FMath::FInterpTo(AimPitchCorrection, WantedPitch, DeltaSeconds, 6.0f);
	}

	// Both holds use the hand IK: the load's own faces are the targets, so the grip lands on the object.
	const float WantedIK = ((Pose == 1 || Pose == 2) && SinceGrab >= 0.25f) ? 1.0f : 0.0f;
	CarryIKAlpha = Pose == 0 ? 0.0f : FMath::FInterpConstantTo(CarryIKAlpha, WantedIK, DeltaSeconds, 4.0f);

	if (Pose == 0)
	{
		bCarryFacingValid = false;
	}

	// A held object answers no traces on any machine. Left queryable, the parkour search found it right
	// in front of the chest and took every jump press as a vault over it, so jumping with a load did
	// nothing. Restored on release: by EndCarry on the server, by OnRep_CarriedActor on clients.
	if (Pose == 1 || Pose == 2)
	{
		UPrimitiveComponent* HeldRoot = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent());
		if (HeldRoot && HeldRoot->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			HeldRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Before the grab the object stays where it rested.
	if ((Pose == 1 || Pose == 2) && SinceGrab < 0.0f)
	{
		CarriedActor->SetActorLocation(CarryRestLocation);
		bCarryFacingValid = false;
		return;
	}

	// The rider swaps between still and moving with the carrier's pace.
	if (bBeingCarried && RiderIdle && GetMesh())
	{
		const AActor* Carrier = GetAttachParentActor();
		const bool bMoving = Carrier && Carrier->GetVelocity().Size2D() > 60.0f;
		UAnimSequenceBase* Wanted = (bMoving && RiderMove) ? RiderMove.Get() : RiderIdle.Get();
		if (Wanted != CurrentRiderAnim)
		{
			GetMesh()->PlayAnimation(Wanted, /*bLooping=*/true);
			CurrentRiderAnim = Wanted;
		}
	}

	if ((Pose == 1 || Pose == 2) && GetMesh())
	{
		const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent());
		const FVector Extent = Root ? Root->Bounds.BoxExtent : FVector(20.0f);

		// The object's own box, not the world-aligned one: turned with the torso, a world box grows by
		// up to 40 percent and pushed the load half a metre further out.
		const FVector LocalExtent = Root
			? Root->CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, Root->GetComponentScale())).BoxExtent
			: FVector(20.0f);
		const FQuat ObjectRotation = CarriedActor->GetActorQuat();
		const auto ExtentAlong = [&LocalExtent, &ObjectRotation](const FVector& Axis)
		{
			return FMath::Abs(FVector::DotProduct(ObjectRotation.GetAxisX(), Axis)) * LocalExtent.X
				+ FMath::Abs(FVector::DotProduct(ObjectRotation.GetAxisY(), Axis)) * LocalExtent.Y
				+ FMath::Abs(FVector::DotProduct(ObjectRotation.GetAxisZ(), Axis)) * LocalExtent.Z;
		};

		// The facing the load is held toward: the hips plus the upper-body turn.
		const float FacingYaw = BodyYaw + CarryTwist;
		const FVector Forward = FRotator(0.0f, FacingYaw, 0.0f).Vector();
		const FVector Right = FRotator(0.0f, FacingYaw + 90.0f, 0.0f).Vector();

		// Set, never added. The object is attached to the mesh, so its world rotation already follows the
		// parent; adding this frame's change on top of that applied the same turn twice and the residual
		// accumulated, so a load carried for a while slowly screwed itself round. An absolute yaw is also what
		// the hand targets need: they are measured along the object's own axes, and a drifting rotation moved
		// those axes every frame, which is the hands twitching and never settling on the load.
		CarriedActor->SetActorRotation(FRotator(0.0f, FacingYaw, 0.0f));
		LastCarryFacingYaw = FacingYaw;
		bCarryFacingValid = true;

		// Placed from the capsule, not from a bone: a bone is last frame's pose, and a load placed from it
		// trailed the body by a frame and jittered while walking. Chest height is a fixed offset above the
		// capsule centre (spine_05 sits about 40 cm up). A handled box rides lower than a bulky one.
		const FVector Chest = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		FVector Centre = Pose == 1
			? Chest - FVector(0.0f, 0.0f, Extent.Z * 0.6f + 18.0f)
			: Chest - FVector(0.0f, 0.0f, 25.0f);

		// Never inside the carrier: its near face goes in front of the capsule, along the torso's facing.
		const float Radius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
		const float Depth = ExtentAlong(Forward);
		const float MinAhead = Radius + Depth + 5.0f;
		const FVector Flat = FVector(Centre.X - GetActorLocation().X, Centre.Y - GetActorLocation().Y, 0.0f);
		const float Ahead = FVector::DotProduct(Flat, Forward);
		if (Pose == 2)
		{
			// Held square in front of the torso, so the turn swings it round the body.
			Centre += Forward * (MinAhead - Ahead) - Right * FVector::DotProduct(Flat, Right);
		}
		else if (Ahead < MinAhead)
		{
			Centre += Forward * (MinAhead - Ahead);
		}

		// And never through the floor the carrier stands on.
		const float FeetZ = GetActorLocation().Z - (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f);
		Centre.Z = FMath::Max(Centre.Z, FeetZ + Extent.Z + 2.0f);

		// Eased off the floor over the first quarter second after the grab, so the hands take it rather
		// than it teleporting into them.
		const float LiftAlpha = FMath::Clamp(SinceGrab / 0.25f, 0.0f, 1.0f);
		const FVector Placed = FMath::Lerp(CarryRestLocation, Centre, FMath::InterpEaseInOut(0.0f, 1.0f, LiftAlpha, 2.0f));
		CarriedActor->SetActorLocation(Placed);

		// The hands go to the object's real size. Something narrower than the shoulders is held by its
		// sides, at its centre; anything wider is held against its near face, as far apart as the arms
		// comfortably go (measured: a 1 m crate's sides are out of reach, the near face is where the clip
		// already holds it).
		const float HalfWidth = ExtentAlong(Right);
		const float MaxHandSpread = 32.0f;
		FVector GripL;
		FVector GripR;
		FVector HandL;
		FVector HandR;
		const bool bSideGrip = HalfWidth + 2.0f <= MaxHandSpread;
		if (bSideGrip)
		{
			// Narrow: the grip points are on its sides, and the hands hover just outside them.
			const FVector GripCentre = Placed - Forward * FMath::Min(Depth * 0.3f, 8.0f);
			GripL = GripCentre - Right * HalfWidth;
			GripR = GripCentre + Right * HalfWidth;
			HandL = GripL - Right * CarryHandGap - Forward * (CarryHandGap * 0.5f);
			HandR = GripR + Right * CarryHandGap - Forward * (CarryHandGap * 0.5f);
		}
		else
		{
			// Wide: the grip points are on the near face, and the hands hover in front of it.
			const FVector GripCentre = Placed - Forward * Depth;
			GripL = GripCentre - Right * MaxHandSpread;
			GripR = GripCentre + Right * MaxHandSpread;
			HandL = GripL - Forward * CarryHandGap;
			HandR = GripR - Forward * CarryHandGap;
		}

		// Into the mesh's own space, against this frame's body: the IK and the beams then ride the mesh
		// through the rest of the frame, whatever the movement does to the capsule after this tick.
		const FTransform MeshXf = GetMesh()->GetComponentTransform();
		CarryHandTargetL = MeshXf.InverseTransformPosition(HandL);
		CarryHandTargetR = MeshXf.InverseTransformPosition(HandR);
		CarryGripL = MeshXf.InverseTransformPosition(GripL);
		CarryGripR = MeshXf.InverseTransformPosition(GripR);

		// Open hands: fingers up (tipped a little toward the load), palm flat toward its face, the way
		// someone holds a box between two palms. UEFN mannequin hand axes, read from the reference pose:
		// the right hand's fingers run along -X and its palm faces +Y; the left hand is the mirror
		// (+X, -Y). A rotation is built from those two axes, then taken into the mesh's space.
		const FVector FingersUp = (FVector::UpVector + Forward * 0.25f).GetSafeNormal();
		const FVector PalmL = bSideGrip ? Right : Forward;
		const FVector PalmR = bSideGrip ? -Right : Forward;
		const FQuat MeshQuatInv = MeshXf.GetRotation().Inverse();
		CarryFingerDir = MeshQuatInv.RotateVector(FingersUp);
		CarryHandRotR = (MeshQuatInv * FRotationMatrix::MakeFromXY(-FingersUp, PalmR).ToQuat()).Rotator();
		CarryHandRotL = (MeshQuatInv * FRotationMatrix::MakeFromXY(FingersUp, -PalmL).ToQuat()).Rotator();

		// Elbows bend out to the sides and down, below the line from shoulder to hand. Taking the bend
		// from the animated elbow (as before) let the arm fold whichever way the clip happened to point,
		// and with the hand pinned the elbow swung free -- the "broken" arm.
		const FVector Shoulder = GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
		const auto Elbow = [&](const FVector& Hand, float Side)
		{
			const FVector ShoulderSide = Shoulder + Right * (18.0f * Side);
			return (ShoulderSide + Hand) * 0.5f + Right * (30.0f * Side) - FVector(0.0f, 0.0f, 30.0f) - Forward * 10.0f;
		};
		CarryElbowL = MeshXf.InverseTransformPosition(Elbow(HandL, -1.0f));
		CarryElbowR = MeshXf.InverseTransformPosition(Elbow(HandR, 1.0f));
	}

	UpdateCarryBeams((Pose == 1 || Pose == 2) && SinceGrab >= 0.25f);
}

void AL2H_BaseCharacter::UpdateCarryBeams(bool bShow)
{
	if (!CarryBeams)
	{
		return;
	}

	if (!bShow)
	{
		if (CarryBeams->IsVisible())
		{
			CarryBeams->ClearInstances();
			CarryBeams->SetVisibility(false);
		}
		return;
	}

	// Each beam is a chain of short glowing segments from the hand to its grip point, bent by a travelling
	// sine wave and flickering in thickness: an unsteady energy line rather than a rigid rod. Everything
	// is in the mesh's space (the component is attached to it), so the beams never trail the body.
	const UWorld* World = GetWorld();
	const float Time = World ? World->GetTimeSeconds() : 0.0f;
	constexpr int32 Segments = 8;

	TArray<FTransform> Pieces;
	Pieces.Reserve(Segments * 2);

	const auto AddBeam = [&Pieces, Time](const FVector& From, const FVector& To, float Phase)
	{
		const FVector Span = To - From;
		const float Length = Span.Size();
		if (Length < 1.0f)
		{
			return;
		}
		const FVector Dir = Span / Length;
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Side, Dir);

		// Pinned at both ends, loosest in the middle.
		const auto PointAt = [&](float Alpha)
		{
			const float Envelope = FMath::Sin(Alpha * PI);
			const float Wave = FMath::Sin(Alpha * 9.0f - Time * 22.0f + Phase);
			const float Wave2 = FMath::Cos(Alpha * 7.0f - Time * 17.0f + Phase * 1.7f);
			return From + Span * Alpha + (Side * Wave + Up * Wave2) * (2.2f * Envelope);
		};

		const float Flicker = 0.75f + 0.25f * FMath::Sin(Time * 40.0f + Phase * 3.0f);
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const FVector A = PointAt(static_cast<float>(Index) / Segments);
			const FVector B = PointAt(static_cast<float>(Index + 1) / Segments);
			const FVector Piece = B - A;
			const float PieceLength = Piece.Size();
			if (PieceLength < 0.01f)
			{
				continue;
			}
			// The engine cylinder is 100 units tall along Z and 100 wide.
			Pieces.Add(FTransform(FRotationMatrix::MakeFromZ(Piece / PieceLength).ToQuat(), (A + B) * 0.5f,
				FVector(0.022f * Flicker, 0.022f * Flicker, PieceLength / 100.0f)));
		}
	};

	// From where the hands actually are (blended by the IK weight), to the grip points on the load.
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const FTransform MeshXf = MeshComp ? MeshComp->GetComponentTransform() : FTransform::Identity;
	const FVector AnimHandL = MeshComp ? MeshXf.InverseTransformPosition(MeshComp->GetSocketLocation(TEXT("hand_l"))) : CarryHandTargetL;
	const FVector AnimHandR = MeshComp ? MeshXf.InverseTransformPosition(MeshComp->GetSocketLocation(TEXT("hand_r"))) : CarryHandTargetR;
	// The hand bone is the wrist; the palm's centre is about 8 cm along the fingers from it.
	const FVector ToPalm = CarryFingerDir * (8.0f * CarryIKAlpha);
	AddBeam(FMath::Lerp(AnimHandL, CarryHandTargetL, CarryIKAlpha) + ToPalm, CarryGripL + ToPalm, 0.0f);
	AddBeam(FMath::Lerp(AnimHandR, CarryHandTargetR, CarryIKAlpha) + ToPalm, CarryGripR + ToPalm, 2.1f);

	CarryBeams->ClearInstances();
	CarryBeams->AddInstances(Pieces, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);
	CarryBeams->SetVisibility(true);
}

void AL2H_BaseCharacter::DropCarryForAction(bool bActionRunning)
{
	if (!bActionRunning)
	{
		return;
	}

	// A movement action that started during a hook flight owns the body from now on.
	if (bHookSwinging && HasAuthority())
	{
		EndHookSwing(/*bArrived=*/false);
	}

	if (!CarriedActor)
	{
		return;
	}

	if (HasAuthority())
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Carry: %s let go of %s to move"), *GetNameSafe(this), *GetNameSafe(CarriedActor)));
		EndCarry(/*bPlayPutDown=*/false);
	}
	else if (IsLocallyControlled() && !bCarryDropRequested)
	{
		bCarryDropRequested = true;
		ServerDropCarryForAction();
	}
}

void AL2H_BaseCharacter::ServerDropCarryForAction_Implementation()
{
	DropCarryForAction(true);
}

bool AL2H_BaseCharacter::GetThrowLaunch(FVector& OutStart, FVector& OutVelocity) const
{
	if (!CarriedActor || Cast<AL2H_BaseCharacter>(CarriedActor))
	{
		return false;
	}

	const UPrimitiveComponent* CarriedRoot = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent());
	if (!CarriedRoot)
	{
		return false;
	}

	// The band is the solo carry band itself, so nothing can be thrown that could not have been lifted.
	const UL2H_MassProfileComponent* Profile = UL2H_MassProfileComponent::FindOn(CarriedActor);
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float HeaviestCarryMass = Config ? FMath::Max(Config->SingleCarryMass, 1.0f) : 60.0f;
	const float MassAlpha = FMath::Clamp((Profile ? Profile->ResolveMass() : 0.0f) / HeaviestCarryMass, 0.0f, 1.0f);
	const float Speed = FMath::Lerp(ThrowLightSpeed, ThrowHeavySpeed, MassAlpha);

	// Aimed from the camera, like a shot.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetAimViewPoint(ViewLocation, ViewRotation);

	FVector Direction = ViewRotation.Vector();
	Direction.Z += ThrowUpwardBias;
	Direction = Direction.GetSafeNormal();

	// Released in front of the chest, clear of the capsule by the object's own size: released where it
	// hangs, it starts inside the thrower and bounces off sideways.
	const FVector Facing = Direction.GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector() : Direction.GetSafeNormal2D();
	const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
	OutStart = GetActorLocation() + Facing * (CapsuleRadius + CarriedRoot->Bounds.SphereRadius + 10.0f) + FVector(0.0f, 0.0f, 30.0f);
	OutVelocity = Direction * Speed;
	return true;
}

void AL2H_BaseCharacter::UpdateThrowPreview()
{
	if (!ThrowArc)
	{
		return;
	}

	FVector Start = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	// Only for the player holding it, and only once it is actually in the hands.
	const bool bWanted = IsLocallyControlled() && GetCarryPose() != 0 && CarryLayerWeight > 0.5f && GetThrowLaunch(Start, Velocity);
	if (!bWanted)
	{
		if (ThrowArc->IsVisible())
		{
			ThrowArc->ClearInstances();
			ThrowArc->SetVisibility(false);
		}
		return;
	}

	FPredictProjectilePathParams Params;
	Params.StartLocation = Start;
	Params.LaunchVelocity = Velocity;
	Params.bTraceWithCollision = true;
	Params.bTraceWithChannel = true;
	Params.TraceChannel = ECC_Visibility;
	// Sized like the object, so the arc ends where its underside meets the floor rather than its centre.
	const UPrimitiveComponent* HeldRoot = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent());
	Params.ProjectileRadius = HeldRoot ? FMath::Clamp(HeldRoot->Bounds.BoxExtent.Z, 5.0f, 40.0f) : 5.0f;
	Params.MaxSimTime = 3.0f;
	Params.SimFrequency = 15.0f;
	Params.ActorsToIgnore.Add(this);
	Params.ActorsToIgnore.Add(CarriedActor);
	Params.DrawDebugType = EDrawDebugTrace::None;

	FPredictProjectilePathResult Result;
	const bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

	// A dotted line, one small sphere per step shrinking toward the end, and a flat disc where it lands.
	TArray<FTransform> Dots;
	const int32 Count = Result.PathData.Num();
	for (int32 Index = 1; Index < Count; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / FMath::Max(Count - 1, 1);
		Dots.Add(FTransform(FQuat::Identity, Result.PathData[Index].Location, FVector(FMath::Lerp(0.07f, 0.045f, Alpha))));
	}
	if (bHit)
	{
		const FQuat Flat = FRotationMatrix::MakeFromZ(Result.HitResult.ImpactNormal).ToQuat();
		Dots.Add(FTransform(Flat, Result.HitResult.ImpactPoint + Result.HitResult.ImpactNormal * 2.0f, FVector(0.6f, 0.6f, 0.02f)));
	}

	ThrowArc->ClearInstances();
	ThrowArc->AddInstances(Dots, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
	ThrowArc->SetVisibility(true);
}

void AL2H_BaseCharacter::UpdateAimState()
{
	// The aim input is a hold, so the press raises the sights and the release lowers them. The aim ability
	// only ever raises them; the lowering is the Blueprint's, derived from the same held input. Undoing the
	// speed and FOV halves here, from the flag itself, means they come off whichever path lowered it -- the
	// button coming up, a carried object, a climb, a roll. Without this they stayed on, so letting go left
	// the player walking at the aim speed with the camera still narrowed.
	//
	// Applied on the owning machine as well as the authority: MaxWalkSpeed replicates with COND_SkipOwner,
	// so the owner has to compose its own value or its movement prediction drifts off the server's.
	const UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>();
	const bool bAiming = Weapons && Weapons->IsAiming();

	if (bAiming)
	{
		// Raised. Whatever applied it -- the aim ability on the machine that pressed the button, or the
		// flag arriving over the wire -- the halves are on from here on.
		bAimHalvesApplied = true;
		return;
	}

	if (!bAimHalvesApplied)
	{
		return;
	}

	bAimHalvesApplied = false;
	SetAimSpeedScale(1.0f);
	if (IsLocallyControlled())
	{
		SetAimFieldOfView(0.0f);
	}
}

void AL2H_BaseCharacter::UpdateAimBeam()
{
	// The beam is a view aid, so it exists only where the view does. Nothing here is replicated: each
	// machine that needs to see a beam runs this for its own pawn. Every other copy still gets one
	// "off" -- the Blueprint's beam component starts visible, and a remote pawn would otherwise carry
	// a grey stick out of its shoulder for the whole session.
	if (!IsLocallyControlled())
	{
		if (bAimBeamActive)
		{
			bAimBeamActive = false;
			OnAimBeamUpdate(FVector::ZeroVector, FVector::ZeroVector, /*bActive=*/false);
		}
		return;
	}

	UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>();

	// The sights have to be up. A carried object takes the press instead of the barrel, and its own aim
	// line is drawn by the throw, not by the gun -- so carrying deliberately shows no weapon laser.
	const bool bBeamWanted = Weapons && Weapons->IsAiming() && !IsCarrying();

	if (!bBeamWanted)
	{
		if (AimDot && AimDot->IsVisible())
		{
			AimDot->SetVisibility(false);
		}

		// One frame of "off", not every frame, so the Blueprint hides the beam and is then left alone.
		if (bAimBeamActive)
		{
			bAimBeamActive = false;
			OnAimBeamUpdate(FVector::ZeroVector, FVector::ZeroVector, /*bActive=*/false);
		}

		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetAimViewPoint(ViewLocation, ViewRotation);

	const FVector Direction = ViewRotation.Vector();

	// Where the line is DRAWN from is the weapon's depth along the view ray, never the muzzle itself. The
	// laser is the view ray drawn as a line, so it has to lie ON that ray or it stops agreeing with the
	// crosshair: a line from the muzzle to a point on the ray leaves the ray, and it leaves it further the
	// closer the point is -- against a wall a metre away it is nearly perpendicular to the barrel, which is
	// the pistol pointing one way and the laser another. Projecting the muzzle onto the ray keeps the line
	// starting at the gun's distance and ending exactly where the crosshair is, at every range.
	FVector BeamStart = ViewLocation;
	if (const AL2H_WeaponActor* Weapon = Weapons->GetActiveWeapon())
	{
		const FVector Muzzle = Weapon->GetFireSocketTransform().GetLocation();
		BeamStart = ViewLocation + Direction * FMath::Max(FVector::DotProduct(Muzzle - ViewLocation, Direction), 0.0f);
	}

	FCollisionQueryParams Params(FName(TEXT("L2H_AimBeam")), /*bTraceComplex=*/false, this);

	// Long enough to be a laser rather than a sphere of influence. The beam is only ever a read-out --
	// the shot's own range still comes from the weapon's data asset.
	// The point is found on the VIEW ray, never on the ray the laser is drawn along. Tracing from the muzzle
	// put the whole ray parallel to the view, a muzzle's width to one side of it, so the laser and the
	// crosshair landed on different parts of the same wall -- and the hook, which does trace the view ray,
	// took the player to a point the laser had never been pointing at. One ray decides the point; where the
	// line is drawn from is a separate question, answered below.
	const FVector AimEnd = ViewLocation + Direction * AimBeamRange;

	FHitResult Hit;
	const bool bHit = GetWorld() &&
		GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, AimEnd, ECC_Visibility, Params);

	// Drawn along the view ray between the weapon's depth and the point the ray found, so the line and the
	// crosshair are the same line. The muzzle is only its depth: the gun's own aim is the pose's to state.
	OnAimBeamUpdate(BeamStart, bHit ? Hit.ImpactPoint : AimEnd, /*bActive=*/true);
	bAimBeamActive = true;

	// The dot replaces the crosshair: it sits on whatever the laser touches, and grows a little with
	// distance so it still reads across a room. Nothing touched, no dot.
	if (AimDot)
	{
		if (bHit)
		{
			// About the same size on screen at any range: 1.5 percent of the distance across (measured: the
			// old 7 cm dot at 8 m was two pixels in split screen and nobody could find it).
			const float Distance = FVector::Dist(ViewLocation, Hit.ImpactPoint);
			const float Scale = FMath::Clamp(Distance * 0.00015f, 0.05f, 0.8f);
			AimDot->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * (Scale * 25.0f));
			AimDot->SetWorldScale3D(FVector(Scale));
		}
		AimDot->SetVisibility(bHit);
	}
}

void AL2H_BaseCharacter::StartHookSwing(FVector Anchor, FVector SurfaceNormal, AActor* HitActor)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !Move || !World || IsDowned() || IsDead())
	{
		return;
	}

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Start = GetActorLocation();

	// The landing point is decided first, as a traversal system would: standing on top of a surface
	// that faces up, or just off a wall with the hands at the hooked height. Unknown normals fall back
	// to a point short of the anchor along the approach, which is never inside the thing that was hit.
	FVector Land;
	if (SurfaceNormal.Z > 0.7f)
	{
		Land = Anchor + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
	}
	else
	{
		FVector Off = SurfaceNormal.GetSafeNormal2D();
		if (Off.IsNearlyZero())
		{
			Off = (Start - Anchor).GetSafeNormal2D();
		}
		// Just off the wall with the hands at the hooked height. Deliberately not on top of it: getting
		// over a ledge, onto a bar or along a wall is the movement system's job, and a jump press during
		// or after the flight starts it (the hook stops steering as soon as one takes over).
		Land = Anchor + Off * HookWallStandOff - FVector(0.0f, 0.0f, HalfHeight * 0.6f);
	}

	// Climbable geometry (DPTS tags it `climb`): the travel ends where DPTS can take over. Its in-air search only
	// catches a ledge it faces with the capsule centre a little above the edge, and the wall landing above
	// (hands at the hooked height) left the centre near a metre under it -- the body slid down past the ledge.
	// So the edge is found (the top of the thing just behind the hooked point), and the body aims for it.
	bHookToLedge = false;
	if (HitActor && HitActor->ActorHasTag(TEXT("climb")) && SurfaceNormal.Z <= 0.7f)
	{
		const FVector Normal2D = SurfaceNormal.GetSafeNormal2D();
		const FVector Probe = Anchor - Normal2D * 10.0f;
		FCollisionQueryParams TopParams(FName(TEXT("L2H_HookLedge")), /*bTraceComplex=*/false, this);
		FHitResult Top;
		if (!Normal2D.IsNearlyZero()
			&& World->LineTraceSingleByChannel(Top, Probe + FVector(0.0f, 0.0f, 250.0f), Probe - FVector(0.0f, 0.0f, 5.0f), ECC_Visibility, TopParams)
			&& Top.ImpactNormal.Z > 0.7f && !Top.bStartPenetrating && Top.ImpactPoint.Z >= Anchor.Z - 5.0f)
		{
			const FVector Face = FVector(Anchor.X, Anchor.Y, 0.0f) + Normal2D * HookWallStandOff;
			Land = FVector(Face.X, Face.Y, Top.ImpactPoint.Z + HookLedgeAboveEdge);
			bHookToLedge = true;
			HookFaceYaw = (-Normal2D).Rotation().Yaw;
		}
	}

	// Never below the floor under the landing point: a hook low on a wall would otherwise aim the body
	// into the ground and the flight would end early, short of the wall.
	{
		FCollisionQueryParams FloorParams(FName(TEXT("L2H_HookFloor")), /*bTraceComplex=*/false, this);
		FHitResult Floor;
		if (World->LineTraceSingleByChannel(Floor, Land + FVector(0.0f, 0.0f, HalfHeight), Land - FVector(0.0f, 0.0f, HalfHeight + 5.0f), ECC_Visibility, FloorParams)
			&& Floor.ImpactNormal.Z > 0.7f)
		{
			Land.Z = FMath::Max(Land.Z, Floor.ImpactPoint.Z + HalfHeight + 2.0f);
		}
	}

	// The arc that reaches the landing point under the world's own gravity. That is the whole "gravity"
	// of the hook: the flight is a real throw, and the body is stopped where the arc ends.
	FVector Launch = FVector::ZeroVector;
	const bool bSolved = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
		this, Launch, Start, Land, /*OverrideGravityZ=*/World->GetGravityZ() * Move->GravityScale, HookArc);
	if (!bSolved || Launch.IsNearlyZero())
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Hook: no arc reaches %s"), *Land.ToCompactString()));
		return;
	}

	HookAnchor = Anchor;
	HookLandPoint = Land;
	bHookSwinging = true;
	OnRep_HookSwing();

	LaunchCharacter(Launch, /*bXYOverride=*/true, /*bZOverride=*/true);

	// The flight is a fall as far as the movement system is concerned, so the in-air ledge catch and wall
	// run search for the whole of it: passing a ledge or a run wall on the way catches it. The owner
	// starts its own search from ClientHookLaunch.
	StartAirParkourSearch();

	// A launch the owning client never hears about is undone by its own prediction and then snapped back.
	if (!IsLocallyControlled())
	{
		ClientHookLaunch(Launch);
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Hook: %s travelling to %s (%.0f cm/s)"),
		*GetNameSafe(this), *Land.ToCompactString(), Launch.Size()));
}

void AL2H_BaseCharacter::ReleaseHookSwing()
{
	if (!bHookSwinging)
	{
		return;
	}

	if (HasAuthority())
	{
		EndHookSwing(/*bArrived=*/false);
	}
	else
	{
		ServerReleaseHookSwing();
	}
}

void AL2H_BaseCharacter::ServerReleaseHookSwing_Implementation()
{
	EndHookSwing(/*bArrived=*/false);
}

void AL2H_BaseCharacter::EndHookSwing(bool bArrived)
{
	if (!HasAuthority() || !bHookSwinging)
	{
		return;
	}

	bHookSwinging = false;
	OnRep_HookSwing();

	// Arrival stops the body at the point. A cancel lets the flight carry on as a plain fall.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bArrived && Move)
	{
		Move->Velocity *= HookArrivalKeep;

		if (bHookToLedge)
		{
			HookCatchUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + HookLedgeCatchWindow;
		}

		// Stopped beside a wall in the air: search again from here, so a ledge or a run wall within reach
		// of where the hook left the body catches it on the way down.
		StartAirParkourSearch();
		if (!IsLocallyControlled())
		{
			ClientHookLaunch(Move->Velocity);
		}
	}

	L2HTime::LogImportant(FString::Printf(TEXT("Hook: %s %s"), *GetNameSafe(this), bArrived ? TEXT("arrived") : TEXT("let go")));
}

void AL2H_BaseCharacter::OnRep_HookSwing()
{
	const UWorld* World = GetWorld();
	HookSwingStartTime = World ? World->GetTimeSeconds() : 0.0f;
	UpdateHookVisuals();
}

void AL2H_BaseCharacter::UpdateHookSwing(float DeltaSeconds)
{
	UpdateHookVisuals();
	UpdateHookLedgeCatch();

	if (!bHookSwinging || !HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Elapsed = World ? World->GetTimeSeconds() - HookSwingStartTime : 0.0f;

	if (IsDowned() || IsDead())
	{
		EndHookSwing(/*bArrived=*/false);
		return;
	}

	const UCharacterMovementComponent* Move = GetCharacterMovement();

	// Something else took the body mid-flight -- a ledge grab, a bar, a wall run, a ladder. It owns the
	// movement from here, so the hook lets go without touching the velocity.
	if (Move && Elapsed > 0.1f && !Move->IsFalling() && !Move->IsMovingOnGround())
	{
		EndHookSwing(/*bArrived=*/false);
		return;
	}

	// Arrived: at the landing point, or already on the ground past the first moments of the flight.
	const float ToLand = FVector::Dist(GetActorLocation(), FVector(HookLandPoint));
	const bool bLandedEarly = Move && Move->IsMovingOnGround() && Elapsed > 0.35f;
	if (ToLand <= HookArriveRadius || bLandedEarly)
	{
		EndHookSwing(/*bArrived=*/true);
		return;
	}

	// Something on the way stopped the flight: give up rather than hang there.
	if (Elapsed > HookMaxFlightTime)
	{
		EndHookSwing(/*bArrived=*/false);
	}
}

void AL2H_BaseCharacter::UpdateHookLedgeCatch()
{
	if (!bHookToLedge || !(HasAuthority() || IsLocallyControlled()))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const bool bNearLedge = bHookSwinging && FVector::Dist(GetActorLocation(), FVector(HookLandPoint)) < 250.0f;
	const bool bCatching = Now < HookCatchUntil;
	if (!bNearLedge && !bCatching)
	{
		return;
	}
	if (IsParkourHanging())
	{
		// Caught: DPTS owns the body now.
		HookCatchUntil = -1.0f;
		return;
	}

	// Facing the wall on the last stretch and while the catch is asked for: the search only looks ahead.
	SetActorRotation(FRotator(0.0f, HookFaceYaw, 0.0f));
	if (bCatching)
	{
		StartAirParkourSearch();
	}
}

void AL2H_BaseCharacter::UpdateHookVisuals()
{
	if (!HookRope || !HookMarker)
	{
		return;
	}

	if (!bHookSwinging || !GetMesh())
	{
		HookRope->SetVisibility(false);
		HookMarker->SetVisibility(false);
		return;
	}

	// A solid line from the right hand to the point, so where the hook caught is never in doubt. The
	// engine cylinder is 100 units tall along Z, so the rope is that mesh stretched to the distance.
	const FVector From = GetMesh()->GetSocketLocation(TEXT("hand_r"));
	const FVector To = HookAnchor;
	const FVector Span = To - From;
	const float Length = Span.Size();
	if (Length < 1.0f)
	{
		HookRope->SetVisibility(false);
	}
	else
	{
		HookRope->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
		HookRope->SetWorldScale3D(FVector(0.025f, 0.025f, Length / 100.0f));
		HookRope->SetVisibility(true);
	}

	// The marker sits on the caught point and pulses, so it reads from across the room.
	const UWorld* World = GetWorld();
	const float Pulse = 0.22f + 0.06f * FMath::Sin((World ? World->GetTimeSeconds() : 0.0f) * 10.0f);
	HookMarker->SetWorldLocation(To);
	HookMarker->SetWorldScale3D(FVector(Pulse));
	HookMarker->SetVisibility(true);
}

void AL2H_BaseCharacter::ServerSetWeaponAiming_Implementation(bool bNewAiming)
{
	if (UL2H_WeaponComponent* Weapons = FindComponentByClass<UL2H_WeaponComponent>())
	{
		Weapons->ServerSetAiming(bNewAiming);
	}
}

void AL2H_BaseCharacter::ClientHookLaunch_Implementation(FVector_NetQuantize LaunchVelocity)
{
	LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);
	// The arrival stop, not the launch: only once the body is at the ledge.
	if (bHookToLedge && FVector::Dist(GetActorLocation(), FVector(HookLandPoint)) < 150.0f)
	{
		HookCatchUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + HookLedgeCatchWindow;
	}
	StartAirParkourSearch();
}

void AL2H_BaseCharacter::StartAirParkourSearch()
{
	// In the air only: the jump button calls this on every press, and on the ground a jump is DPTS's
	// own ground search, not this one.
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || !Move->IsFalling())
	{
		return;
	}

	// Its ExecuteINAIRSearches event takes no parameters.
	if (UActorComponent* Parkour = FindParkourComponent())
	{
		if (UFunction* Search = Parkour->FindFunction(TEXT("ExecuteINAIRSearches")))
		{
			if (Search->ParmsSize == 0)
			{
				Parkour->ProcessEvent(Search, nullptr);
			}
		}
	}
}

UActorComponent* AL2H_BaseCharacter::FindParkourComponent() const
{
	// The DPTS component is a Blueprint class, so it is reached by name: the C++ module has no type to cast to.
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName().Contains(TEXT("DynamicParkourTraceSystem")))
		{
			return Comp;
		}
	}
	return nullptr;
}

bool AL2H_BaseCharacter::IsParkourHanging() const
{
	// ActiveParkourState is a Blueprint enum on the DPTS component; 0 is NONE (free movement), every other
	// value is a climbing state the jump button treats as hanging.
	const UActorComponent* Parkour = FindParkourComponent();
	const FProperty* StateProp = Parkour ? Parkour->GetClass()->FindPropertyByName(TEXT("ActiveParkourState")) : nullptr;
	if (!StateProp)
	{
		return false;
	}
	const void* Value = StateProp->ContainerPtrToValuePtr<void>(Parkour);
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(StateProp))
	{
		return ByteProp->GetPropertyValue(Value) != 0;
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(StateProp))
	{
		return EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value) != 0;
	}
	return false;
}

void AL2H_BaseCharacter::BeginHangJumpPress()
{
	const UWorld* World = GetWorld();
	if (!IsLocallyControlled() || !World)
	{
		return;
	}
	bHangJumpPending = true;
	HangJumpPressTime = World->GetTimeSeconds();
}

bool AL2H_BaseCharacter::EndHangJumpPress()
{
	// Still pending on release: the hold never reached the free-jump time, so this was a tap.
	const bool bTap = bHangJumpPending;
	bHangJumpPending = false;
	return bTap;
}

void AL2H_BaseCharacter::HangDirectionalJump()
{
	if (!IsLocallyControlled() || !IsParkourHanging())
	{
		return;
	}

	// The stick as the player holds it (x right, y forward/up). Read from Enhanced Input directly: while
	// hanging the movement input goes to DPTS, not to the movement component.
	FVector2D Stick = FVector2D::ZeroVector;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (const UEnhancedInputLocalPlayerSubsystem* Input = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				static const UInputAction* StickAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Move.IA_Move"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (StickAction && Input->GetPlayerInput())
				{
					Stick = Input->GetPlayerInput()->GetActionValue(StickAction).Get<FVector2D>();
				}
			}
		}
	}
	if (Stick.Size() < 0.25f)
	{
		Stick = FVector2D(0.0f, 1.0f);   // No direction held: straight up.
	}
	Stick = Stick.GetSafeNormal();

	// While hanging the body faces the wall: its forward is into the wall, its right runs along it.
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();

	FVector Velocity;
	if (Stick.Y < -0.5f)
	{
		// Down: let go and drop, clear of the wall.
		Velocity = -Forward * (HangJumpOut * 1.5f) - FVector(0.0f, 0.0f, 50.0f);
	}
	else
	{
		Velocity = Right * (Stick.X * HangJumpSide)
			- Forward * HangJumpOut
			+ FVector(0.0f, 0.0f, HangJumpUp + FMath::Max(Stick.Y, 0.0f) * HangJumpUpExtra);
	}

	ApplyHangDirectionalJump(Velocity);
	if (!HasAuthority())
	{
		ServerHangDirectionalJump(Velocity);
	}
}

void AL2H_BaseCharacter::ServerHangDirectionalJump_Implementation(FVector_NetQuantize Velocity)
{
	ApplyHangDirectionalJump(Velocity);
}

void AL2H_BaseCharacter::ApplyHangDirectionalJump(const FVector& Velocity)
{
	// DPTS lets go first (DisengageFromState, no animation), so the launch lands on a free body.
	if (UActorComponent* Parkour = FindParkourComponent())
	{
		if (UFunction* Disengage = Parkour->FindFunction(TEXT("DisengageFromState")))
		{
			TArray<uint8> Params;
			Params.SetNumZeroed(FMath::Max<int32>(Disengage->ParmsSize, 1));   // UseAnimation? = false
			Parkour->ProcessEvent(Disengage, Params.GetData());
		}
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Falling);
	}
	LaunchCharacter(Velocity, /*bXYOverride=*/true, /*bZOverride=*/true);

	// A ledge on the way (or the same one, after an upward hop) catches the body again.
	StartAirParkourSearch();

	L2HTime::LogImportant(FString::Printf(TEXT("Hang: %s free jump %s"), *GetNameSafe(this), *Velocity.ToCompactString()));
}

// The climb-down clip (ATS braced climb down) turns the root to face the wall by 0.7 s, having dropped it 141 cm
// by then (171 cm by the end). The DPTS catch search only finds a ledge it faces, with the capsule centre a little
// above the edge (caught at +13 cm in testing; at -42 cm it missed), so the clip's root motion is scaled to arrive
// there as the turn completes, and the body is let go at that moment for the DPTS catch to settle it.
constexpr float ClimbDownReleaseTime = 0.7f;
static float ClimbDownRootScale(float HalfHeight)
{
	constexpr float ClipDropAtRelease = 141.3f;
	constexpr float ReleaseAboveEdge = 13.0f;
	return FMath::Clamp((HalfHeight - ReleaseAboveEdge) / ClipDropAtRelease, 0.3f, 1.0f);
}

bool AL2H_BaseCharacter::TryClimbDownToLedge()
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!IsLocallyControlled() || !World || !Move || !Move->IsMovingOnGround() || bClimbDownMoving || IsParkourHanging() || IsCarrying())
	{
		return false;
	}

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Location = GetActorLocation();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float FeetZ = Location.Z - HalfHeight;

	FCollisionQueryParams Params(FName(TEXT("L2H_ClimbDown")), /*bTraceComplex=*/false, this);

	// The edge: the first spot ahead (within 1.6 m) where the floor drops away by more than a step.
	float EdgeDistance = -1.0f;
	for (float Distance = 30.0f; Distance <= 160.0f; Distance += 10.0f)
	{
		const FVector Probe = Location + Forward * Distance;
		FHitResult Floor;
		const bool bFloor = World->LineTraceSingleByChannel(Floor, FVector(Probe.X, Probe.Y, FeetZ + 40.0f), FVector(Probe.X, Probe.Y, FeetZ - 150.0f), ECC_Visibility, Params);
		if (!bFloor || Floor.ImpactPoint.Z < FeetZ - 120.0f)
		{
			EdgeDistance = Distance;
			break;
		}
	}
	if (EdgeDistance < 0.0f)
	{
		return false;
	}

	// The wall face just under the edge, traced back toward the body from the open side. It runs on past the
	// body: standing at the very lip (as after a climb-up and a turn) the capsule centre can be level with, or
	// beyond, the face, and a trace that stopped at the centre missed it -- the "works only once" report.
	const FVector Outside = Location + Forward * (EdgeDistance + 40.0f);
	const FVector Inside = Location - Forward * 60.0f;
	FHitResult Face;
	if (!World->LineTraceSingleByChannel(Face, FVector(Outside.X, Outside.Y, FeetZ - 30.0f), FVector(Inside.X, Inside.Y, FeetZ - 30.0f), ECC_Visibility, Params))
	{
		return false;
	}
	const FVector Normal = Face.ImpactNormal.GetSafeNormal2D();
	if (Normal.IsNearlyZero())
	{
		return false;
	}

	// The clip's forward travel (scaled, see ClimbDownRootScale) ends just outside the wall.
	constexpr float ClipForward = 72.2f;
	const float Radius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 35.0f;
	const FVector Hang = FVector(Face.ImpactPoint.X, Face.ImpactPoint.Y, Location.Z) + Normal * (Radius + 14.0f);
	const FVector Start = Hang - Normal * (ClipForward * ClimbDownRootScale(HalfHeight));
	const float FromYaw = GetActorRotation().Yaw;
	const float ToYaw = Normal.Rotation().Yaw;

	StartClimbDownMove(Location, Start, FromYaw, ToYaw);
	if (!HasAuthority())
	{
		ServerClimbDownToLedge(Location, Start, FromYaw, ToYaw);
	}
	return true;
}

void AL2H_BaseCharacter::ServerClimbDownToLedge_Implementation(FVector_NetQuantize From, FVector_NetQuantize To, float FromYaw, float ToYaw)
{
	StartClimbDownMove(From, To, FromYaw, ToYaw);
}

// AM_L2H_LedgeClimbDown is the bare clip. The older AM_L2H_ClimbDown carries a DPTS motion-warp window aimed at
// the "height" target, which nothing sets for this move: whatever DPTS left there (often near the world origin)
// dragged the body metres across the level, or up into the sky. The clip's own root motion is enough.
static UAnimMontage* GetClimbDownMontage()
{
	static UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/_Project/Parkour/Montages/AM_L2H_LedgeClimbDown.AM_L2H_LedgeClimbDown"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	return Montage;
}

void AL2H_BaseCharacter::StartClimbDownMove(const FVector& From, const FVector& To, float FromYaw, float ToYaw)
{
	ClimbDownFrom = From;
	ClimbDownTo = To;
	ClimbDownFromYaw = FromYaw;
	ClimbDownToYaw = ToYaw;
	ClimbDownAlpha = 0.0f;
	ClimbDownTime = 0.0f;
	bClimbDownMontage = false;
	bClimbDownMoving = true;

	// Held in the air for the move, so neither gravity nor the walk logic fights it; flying also lets the
	// montage's root motion take the body down, which walking would throw away.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
	}
}

void AL2H_BaseCharacter::UpdateClimbDownMove(float DeltaSeconds)
{
	if (!bClimbDownMoving)
	{
		return;
	}

	ClimbDownTime += DeltaSeconds;

	if (!bClimbDownMontage)
	{
		// A quick step to the clip's start spot, turning to face out over the edge.
		constexpr float AlignDuration = 0.12f;
		ClimbDownAlpha = FMath::Min(ClimbDownAlpha + DeltaSeconds / AlignDuration, 1.0f);
		const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, ClimbDownAlpha, 2.0f);
		SetActorLocation(FMath::Lerp(ClimbDownFrom, ClimbDownTo, Eased), /*bSweep=*/false, nullptr, ETeleportType::None);
		SetActorRotation(FRotator(0.0f, ClimbDownFromYaw + FRotator::NormalizeAxis(ClimbDownToYaw - ClimbDownFromYaw) * Eased, 0.0f));

		if (ClimbDownAlpha >= 1.0f)
		{
			UAnimMontage* Montage = GetClimbDownMontage();
			if (!Montage)
			{
				FinishClimbDownMove();
				return;
			}
			const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
			SetAnimRootMotionTranslationScale(ClimbDownRootScale(HalfHeight));
			PlayAnimMontage(Montage);
			bClimbDownMontage = true;
			ClimbDownTime = 0.0f;
		}
		return;
	}

	// Let go when the turn is done (or if something else took the slot).
	const UAnimMontage* Montage = GetClimbDownMontage();
	const UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const bool bPlaying = Anim && Montage && Anim->Montage_IsPlaying(Montage);
	if (!bPlaying || Anim->Montage_GetPosition(Montage) >= ClimbDownReleaseTime || ClimbDownTime > 2.0f)
	{
		FinishClimbDownMove();
	}
}

void AL2H_BaseCharacter::FinishClimbDownMove()
{
	bClimbDownMoving = false;
	bClimbDownMontage = false;
	SetAnimRootMotionTranslationScale(1.0f);

	// Square to the wall for the catch search, which looks straight ahead (the clip is within ~10 degrees).
	SetActorRotation(FRotator(0.0f, ClimbDownToYaw + 180.0f, 0.0f));
	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->Montage_Stop(0.2f, GetClimbDownMontage());
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Falling);
		Move->Velocity = FVector(0.0f, 0.0f, -50.0f);
	}
	StartAirParkourSearch();
}

void AL2H_BaseCharacter::ClientInteractFoundNothing_Implementation()
{
	OnInteractFoundNothing();
}

void AL2H_BaseCharacter::MulticastShotFX_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHook)
{
	OnShotFX(Start, End, bHook);
}

void AL2H_BaseCharacter::RestoreWalkSpeed()
{
	SprintScale = 1.0f;
	CarryScale = 1.0f;
	AimScale = 1.0f;

	if (bWalkSpeedCached)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxWalkSpeed = CachedWalkSpeed;
		}
		bWalkSpeedCached = false;
	}
}

// -------------------------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::SetDead(bool bNewDead)
{
	if (!HasAuthority() || bDead == bNewDead)
	{
		return;
	}

	bDead = bNewDead;

	// The free revive window opens with the death and closes with any way out of it.
	FreeReviveDeadline = 0.0f;
	if (bDead)
	{
		const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
		const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (LevelConfig && GameState && LevelConfig->FreeReviveWindow > 0.0f)
		{
			FreeReviveDeadline = static_cast<float>(GameState->GetServerWorldTimeSeconds()) + LevelConfig->FreeReviveWindow;
		}
	}

	// A body on the floor spends no time; the clock stops with the fall and runs again on the way up.
	SyncDownDrainPause();

	if (bDead)
	{
		// Death reuses the downed physics, because a body goes limp the same way either time. What
		// differs is everything after: nothing picks a dead pawn back up except a revive, and
		// AreAllPlayersDowned must not count it -- which is why this is a separate flag, not a pose.
		EndCarry();
		ApplyDownedPhysics(true);
	}
	else if (!bDowned && !bBeingCarried)
	{
		// A revive has to undo the ragdoll death applied, or the player is "alive" and still limp.
		// Left alone while downed (time is still out) or carried (the carry owns the body until it
		// puts it down).
		ApplyDownedPhysics(false);
	}

	// The server runs the same hook a client's OnRep does, so a Blueprint graph has one path in
	// rather than two that can quietly drift apart.
	OnDeadStateChanged(bDead);
}

void AL2H_BaseCharacter::OnRep_bDead()
{
	OnDeadStateChanged(bDead);

	if (bDead && !bBeingCarried)
	{
		// The ragdoll is a physics state, not a replicated property, so a client has to reproduce it
		// from the flag -- the same reason OnRep_CarriedActor redoes the attach rather than receiving it.
		ApplyDownedPhysics(true);
	}
	else if (!bDead && !bDowned && !bBeingCarried)
	{
		ApplyDownedPhysics(false);
	}
}

void AL2H_BaseCharacter::ReviveTo(float HealthAmount, float TimeSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	// The director owns every number that moves, Health and Time alike, so this asks it rather than
	// writing two attributes here: a second writer would be a second rule for what "revived" means.
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->RevivePawn(this, HealthAmount, TimeSeconds);
	}
}

void AL2H_BaseCharacter::NotifyHealthDamaged()
{
	// The director calls this after every landed hit. Authority is re-checked rather than assumed, so
	// the function stays safe if a Blueprint graph ever calls it directly too.
	if (!HasAuthority())
	{
		return;
	}

	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	if (!LevelConfig || LevelConfig->HealthRegenRate <= 0.0f)
	{
		return;
	}

	// A pending tick is dropped rather than allowed to run: every hit restarts the delay from itself,
	// so sustained fire keeps pushing regeneration out instead of letting a stale tick slip through.
	GetWorldTimerManager().ClearTimer(HealthRegenTickTimer);

	GetWorldTimerManager().SetTimer(HealthRegenDelayTimer, this, &AL2H_BaseCharacter::StartHealthRegen,
		FMath::Max(LevelConfig->HealthRegenDelay, 0.0f), false);
}

void AL2H_BaseCharacter::StartHealthRegen()
{
	// A one second period, re-armed by TickHealthRegen rather than set to loop: regeneration has to be
	// able to stop when the bar is full and start again only when the next hit lands.
	GetWorldTimerManager().SetTimer(HealthRegenTickTimer, this, &AL2H_BaseCharacter::TickHealthRegen, 1.0f, false);
}

void AL2H_BaseCharacter::TickHealthRegen()
{
	if (!HasAuthority() || bDead || bDowned)
	{
		return;
	}

	const UL2H_LevelTimeConfig* LevelConfig = AL2H_TimeDirector::GetLevelConfig(this);
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!LevelConfig || !Director)
	{
		return;
	}

	const float Current = AL2H_TimeDirector::GetHealthFor(this);
	if (Current < 0.0f || Current >= LevelConfig->MaxHealth)
	{
		// Full, or this pawn has no health layer at all: stop here. The next hit is what restarts this.
		return;
	}

	Director->ApplyHealthRestore(this, LevelConfig->HealthRegenRate);

	// Re-armed only while there is still a gap, so a healed player is not ticking against a full bar
	// once a second for the rest of the run.
	const float After = AL2H_TimeDirector::GetHealthFor(this);
	if (After >= 0.0f && After < LevelConfig->MaxHealth)
	{
		GetWorldTimerManager().SetTimer(HealthRegenTickTimer, this, &AL2H_BaseCharacter::TickHealthRegen, 1.0f, false);
	}
}

void AL2H_BaseCharacter::SetDowned(bool bNewDowned)
{
	if (!HasAuthority() || bDowned == bNewDowned)
	{
		return;
	}

	bDowned = bNewDowned;
	SyncDownDrainPause();

	// A downed pawn cannot keep holding anything, and nothing should keep riding on a collapsing
	// ragdoll. Dropping here is what stops an object hovering over a downed player.
	if (bNewDowned)
	{
		EndCarry();
	}

	// Runs immediately on the server; clients get it through OnRep_bDowned. A carried body is left to
	// the carry, and a dead one stays limp even if its clock comes back.
	if (!bBeingCarried && !(bDead && !bNewDowned))
	{
		ApplyDownedPhysics(bNewDowned);
	}

	// OnRep does not fire on the server, so the server raises the event itself.
	OnDownedStateChanged(bNewDowned);
	RefreshTimeClock();

	L2HTime::LogImportant(FString::Printf(TEXT("%s -> Downed=%s"),
		*GetNameSafe(this), bNewDowned ? TEXT("true") : TEXT("false")));
}

void AL2H_BaseCharacter::SetBeingCarried(bool bNewBeingCarried)
{
	if (!HasAuthority() || bBeingCarried == bNewBeingCarried)
	{
		return;
	}

	bBeingCarried = bNewBeingCarried;
	ApplyCarriedPose(bNewBeingCarried);
	OnCarriedStateChanged(bNewBeingCarried);
}

void AL2H_BaseCharacter::ApplyCarriedPose(bool bCarried)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!MeshComp || !Move || !Capsule)
	{
		return;
	}

	if (bCarried)
	{
		// A simulating ragdoll detaches from its capsule and would stay on the floor while the capsule
		// rides away, so the body is pulled back onto the capsule for the ride.
		MeshComp->bBlendPhysics = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetAllBodiesSimulatePhysics(false);
		MeshComp->SetCollisionProfileName(StandingCollisionProfile);
		if (MeshComp->GetAttachParent() != Capsule)
		{
			MeshComp->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
		}

		MeshComp->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);

		// Piggyback on the rider clip, played single-node. The AnimBP is bypassed on purpose: its Offset
		// Root Bone stands any re-posed body back upright (measured: rotating the mesh never rendered).
		// With no rider clip assigned the body falls back to a partial ragdoll hanging from the pelvis.
		if (RiderIdle)
		{
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			MeshComp->PlayAnimation(RiderIdle, /*bLooping=*/true);
			CurrentRiderAnim = RiderIdle;
		}
		else
		{
			MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
			MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), /*bNewSimulate=*/true, /*bIncludeSelf=*/false);
			MeshComp->WakeAllRigidBodies();
		}

		// The carrier's camera probes for walls on the Camera and Visibility channels; a body on their
		// shoulders sits right in that probe and pulled the camera in to the carrier's head.
		MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

		// No collision while carried: the body would otherwise shove its own carrier around.
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Move->StopMovementImmediately();
		Move->DisableMovement();
		return;
	}

	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Back to the AnimBP for walking -- or for the ragdoll that follows if the player is still out.
	if (MeshComp->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
	{
		MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}
	CurrentRiderAnim = nullptr;

	// The ride left the capsule lying across someone's shoulders; a capsule has to stand upright.
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));

	// Put down: back to the ragdoll if the player is still out, back on their feet otherwise.
	ApplyDownedPhysics(bDowned || bDead);
}

void AL2H_BaseCharacter::AttachCarriedActor(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Cast<AL2H_BaseCharacter>(Target))
	{
		// A partner rides the capsule, across the shoulders. The hand socket is for objects: a whole body
		// hung off one hand reads as a mistake, and swings through the carrier's legs.
		Target->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Target->SetActorRelativeLocation(CarriedBodyOffset);
		Target->SetActorRelativeRotation(CarriedBodyRotation);
		return;
	}

	// An object that named its own socket gets exactly that. One that did not would hang its centre on
	// the right hand -- a metre crate then sits beside the body down to the floor and swallows the
	// carrier. It is held in front of the chest instead, as far out as its own size needs.
	if (ActiveCarrySocket.IsNone() || ActiveCarrySocket == DefaultCarrySocket)
	{
		const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		const FVector Extent = Root ? Root->Bounds.BoxExtent : FVector(25.0f);
		const float Radius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;

		Target->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
		Target->SetActorRelativeLocation(FVector(Radius + FMath::Max(Extent.X, Extent.Y) + 5.0f, 0.0f, 15.0f));
		Target->SetActorRelativeRotation(FRotator::ZeroRotator);
		return;
	}

	Target->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, ActiveCarrySocket);
}

void AL2H_BaseCharacter::ServerSetBeingCarried_Implementation(bool bNewBeingCarried)
{
	SetBeingCarried(bNewBeingCarried);
}

void AL2H_BaseCharacter::ServerTryActivateAbilityByClass_Implementation(TSubclassOf<UGameplayAbility> AbilityClass)
{
	// A body on the floor does nothing. State.Downed is not wired into any ability's blocked tags, so
	// this is the gate that actually holds -- and death has no tag at all.
	if (!AbilitySystemComponent || !AbilityClass || IsDowned() || IsDead())
	{
		return;
	}

	// The base abilities are ServerOnly, so a client's own component would refuse to start them.
	// Routing the request makes the client path identical to the one the host already takes.
	AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
}

void AL2H_BaseCharacter::Input_Menu(const FInputActionValue& Value)
{
	// A simulated proxy has no screen of its own, so only the owning machine may act on this.
	if (!IsLocallyControlled())
	{
		return;
	}

	// D-pad Up moves the time menu's selection while it is open.
	if (TimeQuickMenu)
	{
		MoveTimeQuickMenu(-1);
		return;
	}

	// One key opens and closes, so the same press that got you in gets you out.
	if (TerminalMenu)
	{
		CloseTerminalMenu();
		return;
	}

	// Away from a terminal the key pings where the view looks.
	if (!CurrentTerminal)
	{
		TryPing();
		return;
	}

	TryOpenTerminalMenu();
}

void AL2H_BaseCharacter::Input_RequestAccept(const FInputActionValue& Value)
{
	// D-pad Right confirms whatever the time menu has selected while it is open.
	if (IsLocallyControlled() && TimeQuickMenu)
	{
		ConfirmTimeQuickMenu();
		return;
	}

	if (!IsLocallyControlled() || !PendingRequest.bActive)
	{
		return;
	}

	ServerRespondToTimeRequest(true);
}

void AL2H_BaseCharacter::Input_RequestDecline(const FInputActionValue& Value)
{
	if (!IsLocallyControlled() || !PendingRequest.bActive)
	{
		return;
	}

	ServerRespondToTimeRequest(false);
}

// -------------------------------------------------------------------------------------------
// Time quick menu
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::Input_TimeQuickMenu(const FInputActionValue& Value)
{
	// Allowed while downed or dead on purpose: asking the partner for time is exactly what a player
	// on the floor needs to do.
	if (IsLocallyControlled())
	{
		ToggleTimeQuickMenu();
	}
}

void AL2H_BaseCharacter::ToggleTimeQuickMenu()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = (PC && PC->GetLocalPlayer())
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())
		: nullptr;

	if (TimeQuickMenu)
	{
		TimeQuickMenu->RemoveFromParent();
		TimeQuickMenu = nullptr;

		if (InputSubsystem && TimeQuickMenuContext)
		{
			InputSubsystem->RemoveMappingContext(TimeQuickMenuContext);
		}
		return;
	}

	// The menu's keys go on top for as long as it is open: X confirms instead of jumping, Circle closes
	// instead of dodging, and the D-pad stops switching weapons.
	if (InputSubsystem && TimeQuickMenuContext)
	{
		InputSubsystem->AddMappingContext(TimeQuickMenuContext, /*Priority=*/100);
	}
	const TSubclassOf<UL2H_TimeQuickMenuWidget> MenuClass =
		TimeQuickMenuClass ? TimeQuickMenuClass : TSubclassOf<UL2H_TimeQuickMenuWidget>(UL2H_TimeQuickMenuWidget::StaticClass());
	if (!PC)
	{
		return;
	}

	TimeQuickMenu = CreateWidget<UL2H_TimeQuickMenuWidget>(PC, MenuClass);
	if (!TimeQuickMenu)
	{
		return;
	}

	// AddToPlayerScreen, not AddToViewport: in split screen each player's menu belongs to their half.
	TimeQuickMenu->AddToPlayerScreen(20);

	// A pending request is the likely reason the menu was opened, so the answer starts selected.
	TimeQuickSelection = PendingRequest.bActive ? GetTimeQuickOptions().IndexOfByKey(ETimeQuickOption::Accept) : 0;
	TimeQuickSelection = FMath::Max(TimeQuickSelection, 0);
	TimeQuickStatus = FText::GetEmpty();
	RefreshTimeQuickMenu();
}

TArray<AL2H_BaseCharacter::ETimeQuickOption> AL2H_BaseCharacter::GetTimeQuickOptions() const
{
	TArray<ETimeQuickOption> Options;
	if (PendingRequest.bActive)
	{
		Options.Add(ETimeQuickOption::Accept);
		Options.Add(ETimeQuickOption::Decline);
	}
	if (bHubBankOpen)
	{
		Options.Add(ETimeQuickOption::CarryMore);
		Options.Add(ETimeQuickOption::CarryLess);
	}
	TimeQuickArgs.Init(-1, Options.Num());

	// The shop, in a hub only.
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	if (Config && CurrentTerminal && CurrentTerminal->SavesOnArrival())
	{
		for (TActorIterator<AL2H_BaseCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && (It->IsDead() || It->IsDowned()))
			{
				Options.Add(ETimeQuickOption::ReviveHere);
				TimeQuickArgs.Add(-1);
				break;
			}
		}
		for (int32 Index = 0; Index < Config->ShopItems.Num(); ++Index)
		{
			if (Config->ShopItems[Index])
			{
				Options.Add(ETimeQuickOption::Buy);
				TimeQuickArgs.Add(Index);
			}
		}
	}

	// Tools in the bag, usable anywhere.
	if (const UL2H_InventoryComponent* Bag = FindComponentByClass<UL2H_InventoryComponent>())
	{
		const TArray<FL2H_ItemInstance> Slots = Bag->GetSlots();
		for (int32 Cell = 0; Cell < Slots.Num(); ++Cell)
		{
			if (Slots[Cell].IsSet() && Slots[Cell].Data->Data.UseEffect != EL2H_ItemUseEffect::None)
			{
				Options.Add(ETimeQuickOption::Use);
				TimeQuickArgs.Add(Cell);
			}
		}
	}

	Options.Add(ETimeQuickOption::Give);
	Options.Add(ETimeQuickOption::Request);
	Options.Add(ETimeQuickOption::Close);
	TimeQuickArgs.SetNum(Options.Num());
	for (int32 Row = 0; Row < Options.Num(); ++Row)
	{
		if (Options[Row] != ETimeQuickOption::Buy && Options[Row] != ETimeQuickOption::Use)
		{
			TimeQuickArgs[Row] = -1;
		}
	}
	return Options;
}

void AL2H_BaseCharacter::ServerBuyPartnerRevive_Implementation()
{
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->BuyPartnerReviveAtHub(this);
	}
}

void AL2H_BaseCharacter::ServerBuyShopItem_Implementation(int32 Index)
{
	if (AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this))
	{
		Director->BuyShopItem(this, Index);
	}
}

void AL2H_BaseCharacter::MoveTimeQuickMenu(int32 Direction)
{
	const int32 Count = GetTimeQuickOptions().Num();
	if (!TimeQuickMenu || Count == 0)
	{
		return;
	}

	TimeQuickSelection = (TimeQuickSelection + Direction + Count) % Count;
	RefreshTimeQuickMenu();
}

void AL2H_BaseCharacter::ConfirmTimeQuickMenu()
{
	const TArray<ETimeQuickOption> Options = GetTimeQuickOptions();
	if (!TimeQuickMenu || !Options.IsValidIndex(TimeQuickSelection))
	{
		return;
	}

	switch (Options[TimeQuickSelection])
	{
	case ETimeQuickOption::Give:
		ServerTryActivateAbilityByTag(GetTransferAbilityTag());
		TimeQuickStatus = NSLOCTEXT("L2HTimeQuickMenu", "GiveSent", "Time sent to your partner.");
		break;

	case ETimeQuickOption::Request:
		ServerRequestTimeFromPartner();
		TimeQuickStatus = NSLOCTEXT("L2HTimeQuickMenu", "RequestSent", "Asked your partner for time.");
		break;

	case ETimeQuickOption::Accept:
		ServerRespondToTimeRequest(true);
		TimeQuickStatus = NSLOCTEXT("L2HTimeQuickMenu", "Accepted", "Request accepted.");
		break;

	case ETimeQuickOption::Decline:
		ServerRespondToTimeRequest(false);
		TimeQuickStatus = NSLOCTEXT("L2HTimeQuickMenu", "Declined", "Request declined.");
		break;

	case ETimeQuickOption::CarryMore:
		ServerAdjustCarry(+1);
		TimeQuickStatus = FText::GetEmpty();
		break;

	case ETimeQuickOption::CarryLess:
		ServerAdjustCarry(-1);
		TimeQuickStatus = FText::GetEmpty();
		break;

	case ETimeQuickOption::ReviveHere:
		ServerBuyPartnerRevive();
		TimeQuickStatus = FText::GetEmpty();
		break;

	case ETimeQuickOption::Buy:
	{
		// Said here, before asking: a refused purchase used to look like a menu that stopped working.
		const int32 Index = TimeQuickArgs.IsValidIndex(TimeQuickSelection) ? TimeQuickArgs[TimeQuickSelection] : -1;
		const UL2H_LevelTimeConfig* ShopConfig = AL2H_TimeDirector::GetLevelConfig(this);
		const UL2H_ItemDataAsset* Item = (ShopConfig && ShopConfig->ShopItems.IsValidIndex(Index)) ? ShopConfig->ShopItems[Index].Get() : nullptr;
		if (Item && Chrons < Item->Data.Price)
		{
			TimeQuickStatus = FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "BankShort", "Not enough in the bank ({0} of {1}). The bank fills when a stage ends at a hub."),
				FText::AsNumber(FMath::RoundToInt(Chrons)), FText::AsNumber(Item->Data.Price));
		}
		else
		{
			ServerBuyShopItem(Index);
			TimeQuickStatus = Item ? FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Bought", "Bought {0}."), Item->Data.DisplayName) : FText::GetEmpty();
		}
		break;
	}

	case ETimeQuickOption::Use:
		if (UL2H_InventoryComponent* Bag = FindComponentByClass<UL2H_InventoryComponent>())
		{
			Bag->RequestUse(TimeQuickArgs.IsValidIndex(TimeQuickSelection) ? TimeQuickArgs[TimeQuickSelection] : -1);
		}
		TimeQuickStatus = FText::GetEmpty();
		break;

	case ETimeQuickOption::Close:
	default:
		ToggleTimeQuickMenu();
		return;
	}

	// Kept open after an action, so the result is readable and a second gift is one press away.
	// The rows may change shape (an answered request disappears), so the selection is clamped.
	TimeQuickSelection = FMath::Clamp(TimeQuickSelection, 0, GetTimeQuickOptions().Num() - 1);
	RefreshTimeQuickMenu();
}

void AL2H_BaseCharacter::RefreshTimeQuickMenu()
{
	if (!TimeQuickMenu)
	{
		return;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float Gift = Config ? Config->TransferAmount : 0.0f;
	const AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	const float Cost = Director ? Director->GetTransferCost(this) : Gift;

	TArray<FText> Rows;
	const TArray<ETimeQuickOption> AllOptions = GetTimeQuickOptions();
	const TArray<FL2H_ItemInstance> BagSlots = FindComponentByClass<UL2H_InventoryComponent>() ? FindComponentByClass<UL2H_InventoryComponent>()->GetSlots() : TArray<FL2H_ItemInstance>();
	for (int32 Row = 0; Row < AllOptions.Num(); ++Row)
	{
		const ETimeQuickOption Option = AllOptions[Row];
		const int32 Arg = TimeQuickArgs.IsValidIndex(Row) ? TimeQuickArgs[Row] : -1;
		switch (Option)
		{
		case ETimeQuickOption::ReviveHere:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "ReviveHere", "Bring your partner here and stand them up -- {0} (bank {1})"),
				FText::AsNumber(FMath::RoundToInt(Config ? Config->HubRevivePrice : 0.0f)), FText::AsNumber(FMath::RoundToInt(Chrons))));
			break;
		case ETimeQuickOption::Buy:
			if (Config && Config->ShopItems.IsValidIndex(Arg) && Config->ShopItems[Arg])
			{
				Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Buy", "Buy {0} -- {1} (bank {2})"),
					Config->ShopItems[Arg]->Data.DisplayName, FText::AsNumber(Config->ShopItems[Arg]->Data.Price),
					FText::AsNumber(FMath::RoundToInt(Chrons))));
			}
			else
			{
				Rows.Add(FText::GetEmpty());
			}
			break;
		case ETimeQuickOption::Use:
			if (BagSlots.IsValidIndex(Arg) && BagSlots[Arg].IsSet())
			{
				Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Use", "Use {0} (x{1})"),
					BagSlots[Arg].Data->Data.DisplayName, FText::AsNumber(BagSlots[Arg].Quantity)));
			}
			else
			{
				Rows.Add(FText::GetEmpty());
			}
			break;
		case ETimeQuickOption::Give:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Give", "Give partner {0}s (costs you {1}s)"),
				FText::AsNumber(FMath::RoundToInt(Gift)), FText::AsNumber(FMath::RoundToInt(Cost))));
			break;
		case ETimeQuickOption::Request:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Request", "Ask partner for {0}s"),
				FText::AsNumber(FMath::RoundToInt(Gift))));
			break;
		case ETimeQuickOption::Accept:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "Accept", "Accept: give {0}s (costs you {1}s)"),
				FText::AsNumber(FMath::RoundToInt(PendingRequest.Amount)), FText::AsNumber(FMath::RoundToInt(PendingRequest.Cost))));
			break;
		case ETimeQuickOption::Decline:
			Rows.Add(NSLOCTEXT("L2HTimeQuickMenu", "Decline", "Decline the request"));
			break;
		case ETimeQuickOption::CarryMore:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "CarryMore", "Carry {0}s more into the next stage (carrying {1}/{2}s, bank {3})"),
				FText::AsNumber(FMath::RoundToInt(Config ? Config->CarryStep : 0.0f)), FText::AsNumber(FMath::RoundToInt(HubCarried)),
				FText::AsNumber(FMath::RoundToInt(HubCarryAllowance)), FText::AsNumber(FMath::RoundToInt(Chrons))));
			break;
		case ETimeQuickOption::CarryLess:
			Rows.Add(FText::Format(NSLOCTEXT("L2HTimeQuickMenu", "CarryLess", "Bank {0}s back"),
				FText::AsNumber(FMath::RoundToInt(Config ? Config->CarryStep : 0.0f))));
			break;
		case ETimeQuickOption::Close:
			Rows.Add(NSLOCTEXT("L2HTimeQuickMenu", "Close", "Close"));
			break;
		}
	}

	TimeQuickSelection = FMath::Clamp(TimeQuickSelection, 0, FMath::Max(Rows.Num() - 1, 0));
	TimeQuickMenu->SetOptions(Rows, TimeQuickSelection, TimeQuickStatus);
}

// -------------------------------------------------------------------------------------------
// Checkpoint terminals
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::SetCurrentTerminal(AL2H_CheckpointTerminal* InTerminal)
{
	CurrentTerminal = InTerminal;

	// Leaving the terminal the menu was opened at closes it: the menu belongs to the place, not to the
	// pawn, so walking out is a valid way to put it down.
	if (!CurrentTerminal && TerminalMenu)
	{
		CloseTerminalMenu();
	}
}

bool AL2H_BaseCharacter::TryOpenTerminalMenu()
{
	if (!IsLocallyControlled())
	{
		return false;
	}

	if (!CurrentTerminal)
	{
		L2HTime::LogImportant(TEXT("Menu: refused -- not standing in a checkpoint terminal"));
		return false;
	}

	if (!CurrentTerminal->OffersMenu())
	{
		// A terminal that does not offer the menu should say so rather than eat the key.
		L2HTime::LogImportant(FString::Printf(
			TEXT("Menu: refused -- %s does not offer the menu"),
			*GetNameSafe(CurrentTerminal)));
		return false;
	}

	if (!TerminalMenuClass)
	{
		L2HTime::LogImportant(TEXT("Menu: refused -- TerminalMenuClass is unset on this pawn"));
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	if (!TerminalMenu)
	{
		TerminalMenu = CreateWidget<UL2H_TimeMenuWidget>(PC, TerminalMenuClass);
	}

	if (!TerminalMenu)
	{
		L2HTime::LogImportant(TEXT("Menu: the widget could not be created"));
		return false;
	}

	TerminalMenu->SetTerminal(CurrentTerminal);
	TerminalMenu->AddToPlayerScreen(0);

	// GameAndUI rather than UIOnly: this is co-op, the partner keeps playing while your menu is up and
	// you keep your movement keys. The menu is something you do at a terminal, not a pause.
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(TerminalMenu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;

	return true;
}

void AL2H_BaseCharacter::CloseTerminalMenu()
{
	if (!TerminalMenu)
	{
		return;
	}

	TerminalMenu->RemoveFromParent();
	TerminalMenu = nullptr;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

// -------------------------------------------------------------------------------------------
// Time request
// -------------------------------------------------------------------------------------------

FGameplayTag AL2H_BaseCharacter::GetTransferAbilityTag() const
{
	// One source for both entry points: the F key's AbilityInputBindings row and the menu's transfer
	// row must name the same ability, so they read the same tag rather than repeating the string.
	static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Ability.TimeTransfer")), /*ErrorIfNotFound=*/false);
	return Tag;
}

void AL2H_BaseCharacter::SetPendingRequest(const FL2H_TimeRequest& InRequest)
{
	PendingRequest = InRequest;

	// OnRep does not fire on the server that made the change, so the host's own screen would be the one
	// screen that never showed the question if this were left to replication alone.
	OnRep_PendingRequest();
}

void AL2H_BaseCharacter::ClearPendingRequest()
{
	SetPendingRequest(FL2H_TimeRequest());
}

void AL2H_BaseCharacter::OnRep_PendingRequest()
{
	RefreshRequestPrompt();
	OnTimeRequestChanged(PendingRequest.bActive);
}

void AL2H_BaseCharacter::RefreshRequestPrompt()
{
	// An open time menu grows or loses its Accept/Decline rows as the request comes and goes.
	if (IsLocallyControlled())
	{
		RefreshTimeQuickMenu();
	}

	if (!IsLocallyControlled() || !RequestPrompt)
	{
		return;
	}

	const AL2H_BaseCharacter* Requester = PendingRequest.Requester;

	RequestPrompt->SetRequest(
		PendingRequest.bActive,
		Requester ? FText::FromString(Requester->GetHumanReadableName()) : FText::GetEmpty(),
		PendingRequest.Amount,
		PendingRequest.Cost);
}

void AL2H_BaseCharacter::ServerTryActivateAbilityByTag_Implementation(FGameplayTag AbilityTag)
{
	if (!AbilitySystemComponent || !AbilityTag.IsValid() || IsDowned() || IsDead())
	{
		return;
	}

	// The tag identifies the ability; the component still decides whether it may start, so a row
	// pointing at an ability this pawn was never granted is a silent no-op rather than a bad state.
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AbilityTag);
	AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
}

void AL2H_BaseCharacter::ServerRequestTimeFromPartner_Implementation()
{
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		L2HTime::LogImportant(TEXT("Request: refused -- the level has no time director"));
		return;
	}

	Director->PostTimeRequest(this);
}

void AL2H_BaseCharacter::ServerRespondToTimeRequest_Implementation(bool bAccept)
{
	AL2H_TimeDirector* Director = AL2H_TimeDirector::Get(this);
	if (!Director)
	{
		L2HTime::LogImportant(TEXT("Request: refused -- the level has no time director"));
		return;
	}

	Director->ResolveTimeRequest(this, bAccept);
}

void AL2H_BaseCharacter::ServerRequestOptionalSave_Implementation()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UL2H_SaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UL2H_SaveSubsystem>() : nullptr;

	if (!SaveSubsystem)
	{
		L2HTime::LogImportant(TEXT("Save: refused -- no L2H_SaveSubsystem on this game instance"));
		return;
	}

	// The paid save. It charges, unlike the hub's arrival save, because this one is a choice.
	SaveSubsystem->SaveForTime(this);
}

void AL2H_BaseCharacter::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// The fixed base kit comes from this pawn's own class properties rather than from the set, because
	// those are what the input activates by class. Granting from the place the input already reads keeps
	// one source of truth, and it keeps the base verbs available even on a pawn that has no set
	// assigned. The interact ability is granted too: it is the front door the context button goes
	// through, so a pawn without it would look broken the moment a player pressed it.
	int32 GrantedCount = 0;

	if (UL2H_AbilitySet::GrantAbilityClass(AbilitySystemComponent, InteractAbilityClass, 1, this))
	{
		++GrantedCount;
	}

	if (UL2H_AbilitySet::GrantAbilityClass(AbilitySystemComponent, CarryAbilityClass, 1, this))
	{
		++GrantedCount;
	}

	if (UL2H_AbilitySet::GrantAbilityClass(AbilitySystemComponent, HookAbilityClass, 1, this))
	{
		++GrantedCount;
	}

	// Transfer belongs to the fixed kit rather than to a character set: handing time to a downed
	// partner has to be available to whoever is still standing, whatever super the pawn happens to
	// hold, and whoever they happen to be playing.
	if (UL2H_AbilitySet::GrantAbilityClass(AbilitySystemComponent, TransferAbilityClass, 1, this))
	{
		++GrantedCount;
	}

	// The throw rides the fixed kit for the same reason the transfer does: whoever is holding an object
	// has to be able to put it down, and the fire input must never find nothing on the other end of it.
	if (UL2H_AbilitySet::GrantAbilityClass(AbilitySystemComponent, ThrowAbilityClass, 1, this))
	{
		++GrantedCount;
	}

	// Anything a level adds on top. Optional -- a level with no ability set is a valid level.
	if (DefaultAbilitySet)
	{
		GrantedCount += DefaultAbilitySet->GrantTo(AbilitySystemComponent, this);
	}

	if (GrantedCount > 0)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("%s: granted %d base abilities"),
			*GetNameSafe(this), GrantedCount));
	}
	else if (AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		// Reaching here on the authority means the pawn has no base kit at all, so E and Q are dead
		// keys. That is a misconfigured Blueprint rather than a runtime condition, so it is loud.
		// Gated on authority because a client copy of the pawn is expected to grant nothing.
		L2HTime::LogImportant(FString::Printf(
			TEXT("%s: no base abilities granted -- CarryAbilityClass and HookAbilityClass are unset"),
			*GetNameSafe(this)));
	}
}

// -------------------------------------------------------------------------------------------
// Carry
// -------------------------------------------------------------------------------------------

bool AL2H_BaseCharacter::BeginCarry(AActor* Target, float SpeedScale, FName SocketName)
{
	if (!HasAuthority() || !Target || CarriedActor || Target == this)
	{
		return false;
	}

	UPrimitiveComponent* TargetRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!TargetRoot)
	{
		return false;
	}

	// Stop the body BEFORE attaching it. A simulating body that is also attached fights its own
	// physics and jitters on every machine except the one owning the simulation.
	bCarriedOriginalSimulatePhysics = TargetRoot->IsSimulatingPhysics();
	CarriedOriginalCollision = TargetRoot->GetCollisionEnabled();

	TargetRoot->SetSimulatePhysics(false);
	TargetRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// The socket is decided once, here, and replicated, so OnRep attaches to the same place the
	// server did rather than to a default that may not be what the object asked for.
	ActiveCarrySocket = SocketName.IsNone() ? DefaultCarrySocket : SocketName;

	CarriedActor = Target;
	PreviouslyCarriedActor = Target;

	// The body state goes first so the attach lands on a capsule whose mesh is already back on it.
	AL2H_BaseCharacter* CarriedPawn = Cast<AL2H_BaseCharacter>(Target);
	if (CarriedPawn)
	{
		CarriedPawn->SetBeingCarried(true);
	}

	// Where the object rests before the hands reach it. Sent with the pick-up clip so every machine holds it
	// there until the grab -- attaching snaps it to the hands at once, before the clip has even bent down.
	const FVector RestLocation = Target->GetActorLocation();

	AttachCarriedActor(Target);

	// The weight penalty the ability decided. This composes with sprint rather than replacing it.
	SetCarrySpeedScale(SpeedScale);

	// The pick-up clip that matches the load, sent to every machine: a montage played on the server alone
	// never reaches a client. The authored CarryLiftMontage still wins for light loads when it is set.
	switch (GetCarryPose())
	{
	case 3:  MulticastCarryAnim(CarryPersonStart, true, RestLocation); break;
	case 2:  MulticastCarryAnim(CarryPickupHeavy, true, RestLocation); break;
	default: MulticastCarryAnim(CarryLiftMontage ? static_cast<UAnimSequenceBase*>(CarryLiftMontage.Get()) : CarryPickupLight.Get(), true, RestLocation); break;
	}

	if (Target->Implements<UL2H_GrabbableInterface>())
	{
		IL2H_GrabbableInterface::Execute_OnGrabbed(Target, this);
	}

	return true;
}

void AL2H_BaseCharacter::EndCarry(bool bPlayPutDown)
{
	if (!HasAuthority() || !CarriedActor)
	{
		return;
	}

	// Read before CarriedActor clears: the put-down matches what was being held.
	if (bPlayPutDown && !bDowned && !bDead)
	{
		switch (GetCarryPose())
		{
		case 3:  MulticastCarryAnim(CarryPersonEnd, false, FVector::ZeroVector); break;
		case 2:  MulticastCarryAnim(CarryPutDownHeavy, false, FVector::ZeroVector); break;
		default: MulticastCarryAnim(CarryPutDownLight, false, FVector::ZeroVector); break;
		}
	}

	AActor* Target = CarriedActor;
	CarriedActor = nullptr;

	Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Restore what was captured rather than a default profile: a crate and a downed partner do not
	// go back the same way, and guessing here is what makes a dropped object fall through the floor.
	if (UPrimitiveComponent* TargetRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
	{
		TargetRoot->SetCollisionEnabled(CarriedOriginalCollision);
		TargetRoot->SetSimulatePhysics(bCarriedOriginalSimulatePhysics);
	}

	// Only the carry scale is released. RestoreWalkSpeed would also drop sprint, which the player may
	// well still be holding down while they put the object back.
	SetCarrySpeedScale(1.0f);


	if (AL2H_BaseCharacter* CarriedCharacter = Cast<AL2H_BaseCharacter>(Target))
	{
		CarriedCharacter->SetBeingCarried(false);
	}

	if (Target->Implements<UL2H_GrabbableInterface>())
	{
		IL2H_GrabbableInterface::Execute_OnReleased(Target, this);
	}

	PreviouslyCarriedActor = nullptr;
}

void AL2H_BaseCharacter::OnRep_CarriedActor()
{
	// Attachment is a transform relationship, not a replicated property, so every machine has to
	// reproduce it. Without this the carried object stays where the server last put it in world space
	// on a client, and visibly falls behind the carrier that is holding it.
	if (PreviouslyCarriedActor && PreviouslyCarriedActor != CarriedActor)
	{
		PreviouslyCarriedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// Objects go back to simulating. A partner's capsule must never simulate -- the body's own
		// OnRep_bBeingCarried puts it back on its feet or into the ragdoll instead.
		if (!Cast<AL2H_BaseCharacter>(PreviouslyCarriedActor))
		{
			if (UPrimitiveComponent* OldRoot = Cast<UPrimitiveComponent>(PreviouslyCarriedActor->GetRootComponent()))
			{
				OldRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				OldRoot->SetSimulatePhysics(true);
			}
		}
	}

	bCarryDropRequested = false;

	if (CarriedActor)
	{
		if (UPrimitiveComponent* NewRoot = Cast<UPrimitiveComponent>(CarriedActor->GetRootComponent()))
		{
			NewRoot->SetSimulatePhysics(false);
		}

		AttachCarriedActor(CarriedActor);
	}

	PreviouslyCarriedActor = CarriedActor;
}

void AL2H_BaseCharacter::OnRep_bDowned()
{
	if (!bBeingCarried && !(bDead && !bDowned))
	{
		ApplyDownedPhysics(bDowned);
	}
	OnDownedStateChanged(bDowned);
	RefreshTimeClock();
}

void AL2H_BaseCharacter::OnRep_bBeingCarried()
{
	ApplyCarriedPose(bBeingCarried);
	OnCarriedStateChanged(bBeingCarried);
}

void AL2H_BaseCharacter::ApplyDownedPhysics(bool bNewDowned)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!MeshComp || !Move)
	{
		return;
	}

	if (bNewDowned)
	{
		// Cache -> change. Exiting restores the authored speed rather than assuming a number.
		RestoreWalkSpeed();
		SetCarrySpeedScale(0.0f);

		Move->StopMovementImmediately();
		Move->DisableMovement();

		MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		MeshComp->bBlendPhysics = true;

		StopJumping();
	}
	else
	{
		// Read the lying body before the physics is switched off: where it lies, which way up, and
		// which way it will face once standing. Only a body that was actually a ragdoll gets up.
		const bool bWasRagdoll = MeshComp->IsSimulatingPhysics();
		FVector GetUpLocation = FVector::ZeroVector;
		FRotator GetUpRotation = GetActorRotation();
		UAnimMontage* GetUpMontage = nullptr;
		if (bWasRagdoll)
		{
			GetUpMontage = ComputeGetUp(GetUpLocation, GetUpRotation);
		}

		MeshComp->bBlendPhysics = false;
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetAllBodiesSimulatePhysics(false);
		MeshComp->SetCollisionProfileName(StandingCollisionProfile);

		// Simulating the root body detaches the mesh from the capsule (FBodyInstance::
		// SetInstanceSimulatePhysics), so it has to be put back before the relative snap.
		if (MeshComp->GetAttachParent() != GetCapsuleComponent())
		{
			MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
		MeshComp->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);

		Move->SetMovementMode(MOVE_Walking);
		RestoreWalkSpeed();

		if (GetUpMontage)
		{
			// The capsule stayed where the fall began while the ragdoll slid; it is brought to the body,
			// so the clip's first frame lands on the pose that is lying there. Every machine runs this.
			SetActorLocationAndRotation(GetUpLocation, GetUpRotation, false, nullptr, ETeleportType::TeleportPhysics);
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->Montage_Play(GetUpMontage);
			}
		}
	}
}

UAnimMontage* AL2H_BaseCharacter::ComputeGetUp(FVector& OutLocation, FRotator& OutRotation) const
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	const USkeletalMesh* MeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		return nullptr;
	}

	static const FName PelvisBone(TEXT("pelvis"));
	static const FName HeadBone(TEXT("head"));
	static const FName ChestBone(TEXT("spine_03"));
	const FReferenceSkeleton& RefSkeleton = MeshAsset->GetRefSkeleton();
	const int32 ChestIndex = RefSkeleton.FindBoneIndex(ChestBone);
	if (ChestIndex == INDEX_NONE || MeshComp->GetBoneIndex(PelvisBone) == INDEX_NONE || MeshComp->GetBoneIndex(HeadBone) == INDEX_NONE)
	{
		return nullptr;
	}

	// The chest's front, taken from the reference pose (the mesh faces +Y there), then read on the
	// lying body: pointing up means the body is on its back.
	const FTransform ChestRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, ChestIndex);
	const FVector ChestFrontLocal = ChestRef.InverseTransformVectorNoScale(FVector(0.0f, 1.0f, 0.0f));
	const FVector ChestFront = MeshComp->GetBoneQuaternion(ChestBone, EBoneSpaces::WorldSpace).RotateVector(ChestFrontLocal);
	const bool bFaceUp = ChestFront.Z > 0.0f;

	// Both clips start with the pelvis over the root. Lying on the back the head is behind, so the body
	// stands facing its feet; face down the head is ahead, so it stands facing where the head was.
	const FVector Pelvis = MeshComp->GetBoneLocation(PelvisBone);
	const FVector Head = MeshComp->GetBoneLocation(HeadBone);
	FVector Facing = bFaceUp ? (Pelvis - Head) : (Head - Pelvis);
	Facing.Z = 0.0f;
	OutRotation = Facing.IsNearlyZero() ? GetActorRotation() : FRotator(0.0f, Facing.Rotation().Yaw, 0.0f);

	// Stand the capsule on the floor under the pelvis.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HGetUpFloor), false, this);
	FHitResult Hit;
	const bool bFloor = GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Pelvis + FVector(0.0f, 0.0f, 50.0f),
		Pelvis - FVector(0.0f, 0.0f, 200.0f), ECC_Visibility, Params);
	OutLocation = FVector(Pelvis.X, Pelvis.Y, (bFloor ? Hit.ImpactPoint.Z : Pelvis.Z - 13.0f) + HalfHeight + 1.0f);

	return bFaceUp ? GetUpBackMontage.Get() : GetUpFrontMontage.Get();
}

// -----------------------------------------------------------------------------------------------
// Shards
// -----------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::BeginShardAbsorb(float WindowSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const float Window = FMath::Max(0.0f, WindowSeconds);
	if (Window <= 0.0f)
	{
		// No window means the shard is banked the instant it is picked up, so the slot never closes on
		// anyone. ShardCarryDuration = 0 is documented as "never busy", and being busy is the only
		// thing the cap enforces, so there is nothing to count.
		return;
	}

	++CarriedShardCount;

	UWorld* World = GetWorld();
	if (!World)
	{
		CarriedShardCount = FMath::Max(0, CarriedShardCount - 1);
		return;
	}

	// The window is why a cap of MaxCarriedShards is a rule rather than a formality: the slot stays
	// shut for its whole length, so a pile of shards has to be taken away one at a time. Weak-bound,
	// because the timer outlives neither guarantee and a raw capture would dangle on a destroyed pawn.
	FTimerDelegate Release;
	Release.BindWeakLambda(this, [this]()
	{
		CarriedShardCount = FMath::Max(0, CarriedShardCount - 1);
	});

	World->GetTimerManager().SetTimer(ShardAbsorbTimer, Release, Window, /*bLoop=*/false);
}

// -------------------------------------------------------------------------------------------
// On the floor: the view follows the partner, and anyone can ping
// -------------------------------------------------------------------------------------------

AL2H_BaseCharacter* AL2H_BaseCharacter::FindPartner() const
{
	AL2H_BaseCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AL2H_BaseCharacter> It(World); It; ++It)
		{
			AL2H_BaseCharacter* Other = *It;
			if (!Other || Other == this || Other->IsPendingKillPending())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Other->GetActorLocation(), GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Other;
			}
		}
	}
	return Best;
}

void AL2H_BaseCharacter::UpdateDownView()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const bool bDown = IsDead() || IsDowned();
	if (!bDown)
	{
		DownViewSince = -1.0f;
		bDownViewOwnBody = false;
	}
	else if (DownViewSince < 0.0f)
	{
		DownViewSince = Now;
	}

	// The fall itself is shown on the body; after that the stage is the partner's, so the view goes to
	// them -- unless they are down too (the run is about to rewind) or the player asked for the body.
	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float Delay = Config ? Config->SpectateDelay : 2.0f;
	AL2H_BaseCharacter* Partner = bDown && !bDownViewOwnBody && Now - DownViewSince >= Delay ? FindPartner() : nullptr;
	if (Partner && (Partner->IsDead() || Partner->IsDowned()))
	{
		Partner = nullptr;
	}

	USceneComponent* Rig = DownViewRig.Get();
	if (Partner)
	{
		if (!Rig)
		{
			// GASP's camera rig evaluates around its own component, so moving the component moves the
			// view, and the rig keeps reading this player's control rotation: they orbit the partner with
			// their own stick, and a ping goes where they look.
			TInlineComponentArray<USceneComponent*> Components(this);
			for (USceneComponent* Component : Components)
			{
				if (Component && Component->GetFName() == TEXT("GameplayCamera"))
				{
					Rig = Component;
					break;
				}
			}
			if (!Rig)
			{
				Rig = CameraBoom;
			}
			if (!Rig)
			{
				return;
			}
			DownViewRig = Rig;
			// The authored offset, not the current one: on the floor the rig may already have been moved
			// with the body, and the view at the partner should sit at standing height.
			const USceneComponent* Template = Cast<USceneComponent>(Rig->GetArchetype());
			DownViewRigRelative = Template ? Template->GetRelativeLocation() : Rig->GetRelativeLocation();
			Rig->SetUsingAbsoluteLocation(true);
		}
		// Held at the partner the way it is held at this body.
		Rig->SetWorldLocation(Partner->GetActorTransform().TransformPosition(DownViewRigRelative));
	}
	else if (Rig)
	{
		Rig->SetUsingAbsoluteLocation(false);
		Rig->SetRelativeLocation(DownViewRigRelative);
		DownViewRig.Reset();
	}
}

void AL2H_BaseCharacter::TryPing()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World || !IsLocallyControlled())
	{
		return;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float Now = World->GetTimeSeconds();
	if (Now - LastPingTime < (Config ? Config->PingCooldown : 1.0f))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// Through the players: a view orbiting the partner starts behind them.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HPing), false);
	for (TActorIterator<AL2H_BaseCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	if (CarriedActor)
	{
		Params.AddIgnoredActor(CarriedActor);
	}

	FHitResult Hit;
	const FVector End = ViewLocation + ViewRotation.Vector() * (Config ? Config->PingRange : 6000.0f);
	if (!World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return;
	}

	LastPingTime = Now;
	ServerPing(Hit.ImpactPoint, Hit.GetActor());
}

void AL2H_BaseCharacter::ServerPing_Implementation(FVector_NetQuantize Location, AActor* HitActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UL2H_LevelTimeConfig* Config = AL2H_TimeDirector::GetLevelConfig(this);
	const float Now = World->GetTimeSeconds();
	if (Now - ServerLastPingTime < (Config ? Config->PingCooldown : 1.0f) * 0.5f)
	{
		return;
	}
	ServerLastPingTime = Now;

	// An enemy near the point counts as the target even when the trace caught the floor beside it.
	AActor* Enemy = HitActor && HitActor->IsA<AL2H_EnemyRobot>() ? HitActor : nullptr;
	if (!Enemy)
	{
		for (TActorIterator<AL2H_EnemyRobot> It(World); It; ++It)
		{
			if (!It->IsDead() && FVector::DistSquared(It->GetActorLocation(), FVector(Location)) < FMath::Square(150.0f))
			{
				Enemy = *It;
				break;
			}
		}
	}

	if (AActor* Previous = ActivePing.Get())
	{
		Previous->Destroy();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Where = Enemy ? Enemy->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f) : FVector(Location);
	AL2H_PingMarker* Marker = World->SpawnActor<AL2H_PingMarker>(AL2H_PingMarker::StaticClass(), Where, FRotator::ZeroRotator, SpawnParams);
	if (!Marker)
	{
		return;
	}
	Marker->Setup(Enemy != nullptr, FLinearColor(1.0f, 0.85f, 0.1f));
	Marker->SetLifeSpan(Config ? Config->PingLifetime : 6.0f);
	if (Enemy)
	{
		Marker->AttachToActor(Enemy, FAttachmentTransformRules::KeepWorldTransform);
	}
	ActivePing = Marker;

	L2HTime::LogImportant(FString::Printf(TEXT("Ping: %s marked %s"), *GetNameSafe(this),
		Enemy ? *GetNameSafe(Enemy) : *FVector(Location).ToCompactString()));
}

// -------------------------------------------------------------------------------------------
// Rewind watch history
// -------------------------------------------------------------------------------------------

void AL2H_BaseCharacter::RecordRewindSample()
{
	const UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now < NextRewindSampleTime)
	{
		return;
	}
	NextRewindSampleTime = Now + 0.2f;

	// Only safe ground counts: a place mid-jump, on a ledge or in a climb would put the player back
	// into a move the watch cannot restart. And nothing after the fall, so the watch still reaches
	// back past it.
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (IsDead() || IsDowned() || IsBeingCarried() || IsBusyTraversing() || !Move || !Move->IsMovingOnGround())
	{
		return;
	}

	FRewindSample Sample;
	Sample.Time = Now;
	Sample.Location = GetActorLocation();
	Sample.Rotation = GetActorRotation();
	Sample.Health = AL2H_TimeDirector::GetHealthFor(this);
	RewindHistory.Add(Sample);

	// About twelve seconds is plenty for a watch that goes back five.
	while (RewindHistory.Num() > 0 && Now - RewindHistory[0].Time > 12.0f)
	{
		RewindHistory.RemoveAt(0, EAllowShrinking::No);
	}
}

bool AL2H_BaseCharacter::FindRewindSample(float Seconds, FRewindSample& OutSample) const
{
	if (RewindHistory.Num() == 0)
	{
		return false;
	}

	// On the floor the clock for "how far back" starts at the fall (the last sample), not at now.
	const UWorld* World = GetWorld();
	const float Reference = (IsDead() || IsDowned()) ? RewindHistory.Last().Time : (World ? World->GetTimeSeconds() : 0.0f);
	const float Target = Reference - Seconds;

	// The newest sample at or before the target; failing that the oldest there is.
	OutSample = RewindHistory[0];
	for (const FRewindSample& Sample : RewindHistory)
	{
		if (Sample.Time > Target)
		{
			break;
		}
		OutSample = Sample;
	}
	return true;
}
