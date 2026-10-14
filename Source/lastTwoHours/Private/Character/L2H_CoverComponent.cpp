// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/L2H_CoverComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponComponent.h"

namespace
{
	const FName CoverSlot(TEXT("DefaultSlot"));

	UAnimSequenceBase* LoadClip(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/_Project/animation/L2H/Cover/Clips/%s.%s"), Name, Name);
		return LoadObject<UAnimSequenceBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

UL2H_CoverComponent::UL2H_CoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Between the Blueprint's AC_PreCMCTick (which writes the gait speeds and rotation flags every frame)
	// and the movement component: what cover sets here is what the move this frame uses. BeginPlay wires it.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);

	// "Left"/"Right" are the character's own sides with its back to the wall. Measured on the clips: the
	// Left ones lean and travel toward the mesh's +X, which is the character's left.
	HighIdleLeft = LoadClip(TEXT("A_L2H_Cover_Idle_01_IP_UEFN"));
	HighIdleRight = LoadClip(TEXT("A_L2H_Cover_Idle_01_Alt01_IP_UEFN"));
	HighMoveLeft = LoadClip(TEXT("A_L2H_Cover_SneakLeft_02_IP_UEFN"));
	HighMoveRight = LoadClip(TEXT("A_L2H_Cover_SneakRight_01_IP_UEFN"));
	HighPeekLeft = LoadClip(TEXT("A_L2H_Cover_StandLook_IP_UEFN"));
	HighPeekRight = LoadClip(TEXT("A_L2H_Cover_StandLook_Alt01_IP_UEFN"));
	// The Mixamo low idle rather than CoverDown's: the low transitions start and end exactly on it.
	LowIdleLeft = LoadClip(TEXT("A_L2H_Cover_Idle_02_IP_UEFN"));
	LowIdleRight = LoadClip(TEXT("A_L2H_Cover_Idle_02_Alt01_IP_UEFN"));
	LowMoveLeft = LoadClip(TEXT("A_L2H_CoverLow_Loop_Left_UEFN"));
	LowMoveRight = LoadClip(TEXT("A_L2H_CoverLow_Loop_Right_UEFN"));
	LowPeekLeft = LoadClip(TEXT("A_L2H_CoverLow_Look_Left_UEFN"));
	LowPeekRight = LoadClip(TEXT("A_L2H_CoverLow_Look_Right_UEFN"));

	// Matched pose to pose (component-space head, hands and feet): StandToCover_01/02/03/04 end on
	// Idle_01 / Idle_01_Alt01 / Idle_02 / Idle_02_Alt01 turned 180 deg; ToStand_06/01/02 start on
	// Idle_01 / Idle_02 / Idle_02_Alt01. Nothing in the set starts on Idle_01_Alt01, so that exit blends.
	// _Turn copies: the full 180 deg the capsule has to turn is taken out of the pose (scaled from the
	// hips' own turn) into L2H_CoverYaw, so each one starts on the original pose and ends on the idle.
	HighEnterLeft = LoadClip(TEXT("A_L2H_Cover_StandToCover_01_Turn_UEFN"));
	HighEnterRight = LoadClip(TEXT("A_L2H_Cover_StandToCover_02_Turn_UEFN"));
	LowEnterLeft = LoadClip(TEXT("A_L2H_Cover_StandToCover_03_Turn_UEFN"));
	LowEnterRight = LoadClip(TEXT("A_L2H_Cover_StandToCover_04_Turn_UEFN"));
	// The _Turn copies: the body's turn is taken out of the pose and written to the L2H_CoverYaw curve,
	// which turns the capsule instead (see the Exiting branch of TickComponent).
	HighExitLeft = LoadClip(TEXT("A_L2H_Cover_ToStand_06_Turn_UEFN"));
	HighExitRight = nullptr;
	LowExitLeft = LoadClip(TEXT("A_L2H_Cover_ToStand_01_Turn_UEFN"));
	LowExitRight = LoadClip(TEXT("A_L2H_Cover_ToStand_02_Turn_UEFN"));

	// Mixamo's "_L" travels to the character's right (mesh -X), "_R" to its left: named here by travel.
	HighTransferRight = LoadClip(TEXT("A_L2H_Cover_WalkToCover_L_IP_UEFN"));
	HighTransferLeft = LoadClip(TEXT("A_L2H_Cover_WalkToCover_R_IP_UEFN"));
	LowTransferRight = LoadClip(TEXT("A_L2H_Cover_CrouchToCover_L_IP_UEFN"));
	LowTransferLeft = LoadClip(TEXT("A_L2H_Cover_CrouchToCover_R_IP_UEFN"));

	CoverInputContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/L2HTime/Input/IMC_L2H_Cover.IMC_L2H_Cover"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UL2H_CoverComponent::SyncCoverInput()
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer()
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !CoverInputContext || bInCover == bCoverInputAdded)
	{
		return;
	}
	if (bInCover)
	{
		Input->AddMappingContext(CoverInputContext, CoverInputPriority);
	}
	else
	{
		Input->RemoveMappingContext(CoverInputContext);
	}
	bCoverInputAdded = bInCover;
}

void UL2H_CoverComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp != this && Comp->GetClass()->GetName().Contains(TEXT("AC_PreCMCTick")))
		{
			AddTickPrerequisiteComponent(Comp);
		}
	}
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
}

void UL2H_CoverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UL2H_CoverComponent, bInCover);
	DOREPLIFETIME(UL2H_CoverComponent, bHighCover);
	DOREPLIFETIME(UL2H_CoverComponent, CoverNormal);
	DOREPLIFETIME(UL2H_CoverComponent, Phase);
	DOREPLIFETIME(UL2H_CoverComponent, bTransitionRight);
	DOREPLIFETIME(UL2H_CoverComponent, TransferStart);
	DOREPLIFETIME(UL2H_CoverComponent, TransferTarget);
	DOREPLIFETIME(UL2H_CoverComponent, TransferNormal);
	DOREPLIFETIME(UL2H_CoverComponent, bTransferHigh);
}

AL2H_BaseCharacter* UL2H_CoverComponent::GetCharacter() const
{
	return Cast<AL2H_BaseCharacter>(GetOwner());
}

bool UL2H_CoverComponent::IsOwnerAiming() const
{
	const UL2H_WeaponComponent* Weapons = GetOwner() ? GetOwner()->FindComponentByClass<UL2H_WeaponComponent>() : nullptr;
	return Weapons && Weapons->IsAiming();
}

bool UL2H_CoverComponent::IsProtected() const
{
	return bInCover && Phase == EL2H_CoverPhase::Holding && !IsOwnerAiming();
}

void UL2H_CoverComponent::NotifyCoverKeyPressed()
{
	bKeyPressed = true;
}

void UL2H_CoverComponent::NotifyJumpKeyPressed()
{
	bJumpPressed = true;
}

bool UL2H_CoverComponent::FindTransferTarget(bool bRight, FVector& OutLocation, FVector& OutNormal, bool& bOutHigh) const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Character->GetActorLocation();
	const float FeetZ = Location.Z - HalfHeight;
	const FVector Normal = FVector(CoverNormal);
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Normal) * (bRight ? 1.0f : -1.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HCoverTransfer), false, Character);

	// Walk a probe out along the line of the wall: first past this wall's end, then across the gap, until
	// a wall facing the same way turns up.
	auto Probe = [&](float Along, float Height, FHitResult& Hit)
	{
		const FVector Start = FVector(Location.X, Location.Y, FeetZ + Height) + Side * Along;
		return World->LineTraceSingleByChannel(Hit, Start, Start - Normal * (Radius + WallGap + 120.0f), ECC_Visibility, Params)
			&& FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal2D(), Normal) > 0.8f;
	};

	float GapStart = -1.0f;
	for (float Along = Radius; Along <= MaxTransferDistance + Radius; Along += 15.0f)
	{
		FHitResult Hit;
		const bool bWall = Probe(Along, LowCoverMinHeight, Hit);
		if (!bWall)
		{
			if (GapStart < 0.0f)
			{
				GapStart = Along;
			}
			continue;
		}
		if (GapStart < 0.0f || Along - GapStart < MinTransferGap)
		{
			// Still this wall, or a gap too narrow to call a crossing.
			GapStart = -1.0f;
			continue;
		}

		// The next wall starts here; the capsule stands a body's width in from its end, if the wall reaches.
		const float StandAlong = Along + Radius + 20.0f;
		FHitResult Stand;
		if (!Probe(StandAlong, LowCoverMinHeight, Stand))
		{
			return false;
		}
		const FVector WallNormal = Stand.ImpactNormal.GetSafeNormal2D();
		OutNormal = WallNormal;
		OutLocation = FVector(Stand.ImpactPoint.X, Stand.ImpactPoint.Y, Location.Z) + WallNormal * (Radius + WallGap);
		FHitResult High;
		bOutHigh = Probe(StandAlong, HighCoverHeight, High);

		// The way there has to be open for the capsule.
		FHitResult Block;
		FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(L2HCoverTransferSweep), false, Character);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.9f);
		return !World->SweepSingleByChannel(Block, Location, OutLocation, FQuat::Identity, ECC_Pawn, Shape, SweepParams);
	}
	return false;
}

void UL2H_CoverComponent::BeginTransfer(const FVector& Target, const FVector& Normal, bool bHigh, bool bRight)
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	if (!Character || !bInCover || Phase != EL2H_CoverPhase::Holding)
	{
		return;
	}
	TransferStart = Character->GetActorLocation();
	TransferTarget = Target;
	TransferNormal = Normal;
	bTransferHigh = bHigh;
	bTransitionRight = bRight;
	Phase = EL2H_CoverPhase::Transferring;
	StartPhaseVisuals();
	L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s crossing %.0f cm to the %s to %s cover"), *GetNameSafe(GetOwner()),
		FVector::Dist2D(FVector(TransferStart), Target), bRight ? TEXT("right") : TEXT("left"), bHigh ? TEXT("high") : TEXT("low")));
}

void UL2H_CoverComponent::ServerBeginTransfer_Implementation(FVector_NetQuantize Target, FVector_NetQuantizeNormal Normal, bool bHigh, bool bRight)
{
	// The server finds its own way across and goes only if it lands on the same wall.
	FVector ServerTarget;
	FVector ServerNormal;
	bool bServerHigh = false;
	if (!FindTransferTarget(bRight, ServerTarget, ServerNormal, bServerHigh) || FVector::Dist2D(ServerTarget, FVector(Target)) > 60.0f)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s asked to cross to cover the server cannot find -- refused"), *GetNameSafe(GetOwner())));
		return;
	}
	BeginTransfer(ServerTarget, ServerNormal, bServerHigh, bRight);
}

void UL2H_CoverComponent::FinishTransfer()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	if (!bInCover || Phase != EL2H_CoverPhase::Transferring || !Character)
	{
		return;
	}
	Character->SetActorLocation(FVector(TransferTarget), false, nullptr, ETeleportType::None);
	CoverNormal = TransferNormal;
	const bool bWasLowClip = !bHighCover || !bTransferHigh;
	bHighCover = bTransferHigh;
	// The crossing clips arrive leaning back the way they came.
	bFaceRight = !bTransitionRight;
	SideLockUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5f : 0.0f;
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
	}
	Phase = EL2H_CoverPhase::Holding;
	Character->SetActorRotation(FRotator(0.0f, FVector(CoverNormal).Rotation().Yaw, 0.0f));
	StartPhaseVisuals();
	// The clip ends exactly on the idle of its own height; a change of height between the walls blends.
	bSnapNextClip = bWasLowClip ? !bHighCover : bHighCover;
	if (Character->IsLocallyControlled())
	{
		if (bHighCover)
		{
			Character->UnCrouch();
		}
		else
		{
			Character->Crouch();
		}
	}
}

bool UL2H_CoverComponent::FindCover(FVector& OutPoint, FVector& OutNormal, bool& bOutHigh) const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float FeetZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Base(Character->GetActorLocation().X, Character->GetActorLocation().Y, 0.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HCover), false, Character);
	auto Probe = [&](float Height, FHitResult& Hit)
	{
		const FVector Start = Base + FVector(0.0f, 0.0f, FeetZ + Height);
		return World->LineTraceSingleByChannel(Hit, Start, Start + Forward * (Radius + EnterDistance), ECC_Visibility, Params);
	};

	// The wall has to be there at hiding height and face back toward the character (not a slope or a step).
	FHitResult Low;
	if (!Probe(LowCoverMinHeight, Low) || FMath::Abs(Low.ImpactNormal.Z) > 0.3f
		|| FVector::DotProduct(Low.ImpactNormal.GetSafeNormal2D(), -Forward) < 0.5f)
	{
		return false;
	}

	FHitResult High;
	bOutHigh = Probe(HighCoverHeight, High);
	OutPoint = Low.ImpactPoint;
	OutNormal = Low.ImpactNormal.GetSafeNormal2D();
	return true;
}

bool UL2H_CoverComponent::HasWallAt(float Offset) const
{
	const AL2H_BaseCharacter* Character = GetCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float FeetZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	const FVector Normal = FVector(CoverNormal);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Normal);
	const FVector Start = FVector(Character->GetActorLocation().X, Character->GetActorLocation().Y, FeetZ + LowCoverMinHeight)
		+ Right * Offset;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HCoverEdge), false, Character);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, Start, Start - Normal * (Radius + WallGap + 40.0f), ECC_Visibility, Params);
}

float UL2H_CoverComponent::EdgeLookAhead(float Radius) const
{
	// Part of the capsule may hang past the end, plus one frame's travel so a slow frame cannot step off.
	const UWorld* World = GetWorld();
	const float Frame = World ? World->GetDeltaSeconds() : 0.0f;
	return Radius * 0.6f + MoveSpeed * FMath::Min(Frame * 1.25f, 1.0f) + 5.0f;
}

FVector UL2H_CoverComponent::ConstrainMovementInput(const FVector& WorldDirection, float& InOutScale) const
{
	if (!bInCover || IsOwnerAiming())
	{
		return WorldDirection;
	}

	// The body stays put while it turns into or out of cover.
	if (Phase != EL2H_CoverPhase::Holding)
	{
		return FVector::ZeroVector;
	}

	// Only the part along the wall survives, at the clips' pace whatever gait the locomotion picked.
	const FVector Normal = FVector(CoverNormal);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Normal);
	const AL2H_BaseCharacter* Character = GetCharacter();
	const float Radius = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
	const float Size = WorldDirection.Size();
	float Along = FVector::DotProduct(WorldDirection, Right);

	// The stick arrives camera-relative, one axis per call. With the camera looking along the wall (as it
	// does out past an edge), left/right on the stick points into or away from the wall and has no part
	// along it: the player was stuck in the edge look until the stick happened to point along the wall.
	// At an edge, such a push now walks back along the wall, away from the open end.
	if (Size > KINDA_SMALL_NUMBER && FMath::Abs(Along) < 0.4f * Size)
	{
		// Which end is open is read from the wall, not from the body's facing: the facing turns as soon as
		// the step back starts, and the step must carry on until the look has ended.
		const bool bOpenRight = !HasWallAt(Radius + 15.0f);
		const bool bOpenLeft = !HasWallAt(-(Radius + 15.0f));
		if (bOpenRight == bOpenLeft)
		{
			return FVector::ZeroVector;
		}
		Along = bOpenRight ? -Size : Size;
	}
	if (FMath::Abs(Along) < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// The side the player is pushing toward, even when the wall's end stops the step: it is what turns the
	// body to look out past that end.
	IntentRight = Along > 0.0f ? 1 : -1;
	IntentFrame = GFrameCounter;

	// Nothing past the end of the wall: a step that would leave it is dropped.
	if (!HasWallAt(FMath::Sign(Along) * EdgeLookAhead(Radius)))
	{
		return FVector::ZeroVector;
	}

	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	const float MaxSpeed = Move ? FMath::Max(Move->GetMaxSpeed(), 1.0f) : MoveSpeed;
	InOutScale *= FMath::Clamp(MoveSpeed / MaxSpeed, 0.0f, 1.0f);
	return Right * Along;
}

void UL2H_CoverComponent::EnterCover(const FVector& Normal, bool bHigh, bool bRight)
{
	bInCover = true;
	bHighCover = bHigh;
	CoverNormal = Normal.GetSafeNormal2D();
	bStoodUpToAim = false;
	bTransitionRight = bRight;
	Phase = EL2H_CoverPhase::Entering;
	StartPhaseVisuals();
	L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s took %s cover"), *GetNameSafe(GetOwner()), bHigh ? TEXT("high") : TEXT("low")));
}

void UL2H_CoverComponent::ExitCover()
{
	if (!bInCover)
	{
		return;
	}
	bInCover = false;
	Phase = EL2H_CoverPhase::None;
	StopCoverMontage();
	L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s left cover"), *GetNameSafe(GetOwner())));
}

void UL2H_CoverComponent::BeginExit()
{
	if (!bInCover || Phase != EL2H_CoverPhase::Holding)
	{
		return;
	}
	bTransitionRight = bFaceRight;
	Phase = EL2H_CoverPhase::Exiting;
	StartPhaseVisuals();
	L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s leaving %s cover on the %s side (%s)"), *GetNameSafe(GetOwner()),
		bHighCover ? TEXT("high") : TEXT("low"), bTransitionRight ? TEXT("right") : TEXT("left"), *GetNameSafe(PlayingAnim.Get())));
}

void UL2H_CoverComponent::StartPhaseVisuals()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	UAnimSequenceBase* Clip = nullptr;
	if (Phase == EL2H_CoverPhase::Entering)
	{
		bFaceRight = bTransitionRight;
		Clip = bHighCover ? (bTransitionRight ? HighEnterRight : HighEnterLeft) : (bTransitionRight ? LowEnterRight : LowEnterLeft);

		// The clip's turn curve runs 0 -> +/-180 (left clips turn one way, right clips the other). The
		// capsule follows it from where it faces now to the wall's outward facing, scaled to the angle
		// actually needed, so neither end snaps: a snapped capsule was spun back round by GASP's Offset
		// Root Bone, which read on screen as a pointless turn on every entry.
		EnterStartYaw = Character->GetActorRotation().Yaw;
		const float ClipTurn = bTransitionRight ? -180.0f : 180.0f;
		float Needed = FRotator::NormalizeAxis(FVector(CoverNormal).Rotation().Yaw - EnterStartYaw);
		if (ClipTurn > 0.0f && Needed < 0.0f)
		{
			Needed += 360.0f;
		}
		else if (ClipTurn < 0.0f && Needed > 0.0f)
		{
			Needed -= 360.0f;
		}
		EnterTurnScale = FMath::Clamp(Needed / ClipTurn, 0.3f, 1.7f);
	}
	else if (Phase == EL2H_CoverPhase::Exiting)
	{
		Clip = bHighCover ? (bTransitionRight ? HighExitRight : HighExitLeft) : (bTransitionRight ? LowExitRight : LowExitLeft);
		if (!Clip)
		{
			// No clip for this side: out on a plain blend.
			PlayingAnim = nullptr;
		}
		ExitStartYaw = Character->GetActorRotation().Yaw;
		ExitTurn = 0.0f;
	}
	else if (Phase == EL2H_CoverPhase::Transferring)
	{
		// Crouched across if either wall is low (the body stays under the lower one); standing between two
		// high walls.
		const bool bLowClip = !bHighCover || !bTransferHigh;
		Clip = bLowClip ? (bTransitionRight ? LowTransferRight : LowTransferLeft) : (bTransitionRight ? HighTransferRight : HighTransferLeft);
	}
	else if (Phase == EL2H_CoverPhase::Holding)
	{
		bSnapNextClip = true;
		PlayingAnim = nullptr;
		return;
	}
	else
	{
		return;
	}

	TransitionEndTime = World->GetTimeSeconds();
	if (Clip && AnimInstance)
	{
		const float Rate = FMath::Max(TransitionRate, 0.1f);
		// The exit does not blend out: its turn curve is weighted by the montage, and a fading clip would
		// hand back part of the turn. Its last pose is already the standing one, facing the capsule.
		const float BlendOut = 0.0f;
		PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Clip, CoverSlot, 0.2f, BlendOut, Rate, 1);
		PlayingAnim = Clip;
		TransitionEndTime += Clip->GetPlayLength() / Rate - 0.03f;
	}
}

void UL2H_CoverComponent::FinishEnter()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	if (!bInCover || Phase != EL2H_CoverPhase::Entering || !Character)
	{
		return;
	}
	Phase = EL2H_CoverPhase::Holding;
	SideLockUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5f : 0.0f;
	// The turn curve has brought the capsule round to the wall's outward facing and the clip has ended on
	// the idle's first pose: settle the last fraction of a degree and start the idle unblended.
	Character->SetActorRotation(FRotator(0.0f, FVector(CoverNormal).Rotation().Yaw, 0.0f));
	StartPhaseVisuals();
}

void UL2H_CoverComponent::FinishExit()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	if (!bInCover || Phase != EL2H_CoverPhase::Exiting || !Character)
	{
		return;
	}

	// The capsule has been turning with the clip's L2H_CoverYaw curve; it keeps the full turn.
	Character->SetActorRotation(FRotator(0.0f, FRotator::NormalizeAxis(ExitStartYaw + ExitTurn), 0.0f));
	ExitCover();
}

void UL2H_CoverComponent::ForceExitCover()
{
	if (!bInCover)
	{
		return;
	}
	AL2H_BaseCharacter* Character = GetCharacter();
	ExitCover();
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority())
	{
		ServerExitCover();
	}
}

void UL2H_CoverComponent::ServerEnterCover_Implementation(FVector_NetQuantizeNormal Normal, bool bHigh, bool bRight)
{
	// Checked again here: the wall must still be in front of the server's copy of the body.
	FVector Point;
	FVector ServerNormal;
	bool bServerHigh = false;
	if (!FindCover(Point, ServerNormal, bServerHigh) || FVector::DotProduct(ServerNormal, FVector(Normal)) < 0.7f)
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Cover: %s asked for cover the server cannot see -- refused"), *GetNameSafe(GetOwner())));
		bInCover = false;
		return;
	}
	EnterCover(ServerNormal, bServerHigh, bRight);
}

void UL2H_CoverComponent::ServerExitCover_Implementation()
{
	ExitCover();
}

void UL2H_CoverComponent::ServerBeginExit_Implementation(bool bRight)
{
	bFaceRight = bRight;
	BeginExit();
}

void UL2H_CoverComponent::OnRep_InCover()
{
	if (!bInCover)
	{
		StopCoverMontage();
	}
}

void UL2H_CoverComponent::OnRep_Phase(EL2H_CoverPhase OldPhase)
{
	// The owner already started its own transition; the others start theirs from the replicated phase.
	const AL2H_BaseCharacter* Character = GetCharacter();
	if (Character && !Character->IsLocallyControlled())
	{
		if (Phase == EL2H_CoverPhase::Holding && (OldPhase == EL2H_CoverPhase::Transferring || OldPhase == EL2H_CoverPhase::Entering))
		{
			// The hold arrived without FinishTransfer / FinishEnter running here: take their side, or the
			// other machines showed the far side's idle until the next move.
			bFaceRight = OldPhase == EL2H_CoverPhase::Transferring ? !bTransitionRight : bTransitionRight;
			SideLockUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5f : 0.0f;
		}
		StartPhaseVisuals();
	}
}

void UL2H_CoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AL2H_BaseCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	const bool bLocal = Character->IsLocallyControlled();
	const bool bAiming = IsOwnerAiming();

	if (bLocal)
	{
		SyncCoverInput();
	}

	// The key press is read here, after the Blueprint handled the same press: if it became a climb-down
	// (or the body is hanging), the press was the climb's and cover leaves it alone.
	if (bLocal && bKeyPressed)
	{
		bKeyPressed = false;
		if (bInCover)
		{
			// Out through the exit clip from a settled hold; a press mid-transition is ignored.
			if (Phase == EL2H_CoverPhase::Holding && !bAiming)
			{
				const bool bSide = bFaceRight;
				BeginExit();
				if (!Character->HasAuthority())
				{
					ServerBeginExit(bSide);
				}
			}
			else if (Phase == EL2H_CoverPhase::Holding)
			{
				ForceExitCover();
			}
			Character->UnCrouch();
		}
		else if (!Character->IsBusyTraversing() && !Character->IsCarrying() && !Character->IsDowned() && !Character->IsDead()
			&& Character->GetCharacterMovement()->IsMovingOnGround())
		{
			FVector Point;
			FVector Normal;
			bool bHigh = false;
			if (FindCover(Point, Normal, bHigh))
			{
				// The side to settle toward: the way the body was already drifting along the wall, else left.
				const FVector WallRight = FVector::CrossProduct(FVector::UpVector, Normal);
				const bool bRight = FVector::DotProduct(Character->GetVelocity(), WallRight) > 50.0f;
				EnterCover(Normal, bHigh, bRight);
				if (!Character->HasAuthority())
				{
					ServerEnterCover(Normal, bHigh, bRight);
				}
				// The same press toggled the crouch in the Blueprint: a high wall is stood behind.
				if (bHigh)
				{
					Character->UnCrouch();
				}
				else
				{
					Character->Crouch();
				}
			}
		}
	}

	if (!bInCover)
	{
		return;
	}

	// Anything that takes the body off the floor or out of the player's hands ends cover.
	if (Character->IsDowned() || Character->IsDead() || Character->IsBeingCarried() || Character->IsCarrying()
		|| Character->IsBusyTraversing() || !Character->GetCharacterMovement()->IsMovingOnGround())
	{
		if (bLocal || Character->HasAuthority())
		{
			ForceExitCover();
		}
		return;
	}

	const bool bDrives = bLocal || Character->HasAuthority();
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// The transition clips do not blend out, so they are ended a frame and a half before they would run
	// out on their own: otherwise a long frame showed the plain locomotion pose for one frame in between.
	// Read when used, not here: a transition started further down this same tick sets a new end time.
	auto ClipRunningOut = [this, Now, DeltaTime]() { return Now + DeltaTime * 1.5f >= TransitionEndTime; };

	// Circle at a wall's end, looking out past it: across to the next cover on that side, if there is one.
	// Nowhere else does it do anything in cover.
	if (bLocal && bJumpPressed)
	{
		bJumpPressed = false;
		const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const bool bAtEdge = !HasWallAt((bFaceRight ? 1.0f : -1.0f) * (Radius + 15.0f));
		FVector Target;
		FVector Normal;
		bool bHigh = false;
		if (Phase == EL2H_CoverPhase::Holding && !bAiming && bAtEdge && FindTransferTarget(bFaceRight, Target, Normal, bHigh))
		{
			BeginTransfer(Target, Normal, bHigh, bFaceRight);
			if (!Character->HasAuthority())
			{
				ServerBeginTransfer(Target, Normal, bHigh, bFaceRight);
			}
		}
	}

	if (Phase == EL2H_CoverPhase::Transferring)
	{
		if (bDrives)
		{
			// Root motion by hand: L2H_CoverMove is how far along the clip's own trip the body is, applied to
			// the real trip to the next wall. The capsule keeps its back to the walls; the clip turns the body.
			static const FName MoveCurve(TEXT("L2H_CoverMove"));
			const UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
			if (AnimInstance && PlayingAnim)
			{
				const float Progress = AnimInstance->GetCurveValue(MoveCurve);
				const FVector From = FVector(TransferStart);
				const FVector To = FVector(TransferTarget);
				FVector Pos = From + (To - From) * Progress;
				Pos.Z = Character->GetActorLocation().Z;
				Character->SetActorLocation(Pos, false, nullptr, ETeleportType::None);
			}
			if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
			{
				Move->Velocity = FVector(0.0f, 0.0f, Move->Velocity.Z);
				Move->bOrientRotationToMovement = false;
				Move->bUseControllerDesiredRotation = false;
			}
			if (bAiming || ClipRunningOut() || !PlayingAnim)
			{
				FinishTransfer();
			}
		}
		return;
	}

	// The transitions run on a clock of their own; aiming cuts them short, so a shot is never held back.
	if (Phase == EL2H_CoverPhase::Entering)
	{
		if (bDrives)
		{
			HoldToWall(DeltaTime);
			static const FName TurnCurve(TEXT("L2H_CoverYaw"));
			const UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
			if (bInCover && AnimInstance && PlayingAnim)
			{
				const float Turn = AnimInstance->GetCurveValue(TurnCurve) * EnterTurnScale;
				Character->SetActorRotation(FRotator(0.0f, FRotator::NormalizeAxis(EnterStartYaw + Turn), 0.0f));
			}
			if (bInCover && (bAiming || ClipRunningOut()))
			{
				FinishEnter();
			}
		}
		return;
	}
	if (Phase == EL2H_CoverPhase::Exiting)
	{
		if (bDrives)
		{
			// Root motion by hand: the body's turn lives in the clip's L2H_CoverYaw curve, not in its pose.
			static const FName TurnCurve(TEXT("L2H_CoverYaw"));
			const UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
			if (AnimInstance && PlayingAnim)
			{
				ExitTurn = AnimInstance->GetCurveValue(TurnCurve);
				Character->SetActorRotation(FRotator(0.0f, FRotator::NormalizeAxis(ExitStartYaw + ExitTurn), 0.0f));
			}
			if (bAiming || ClipRunningOut())
			{
				FinishExit();
			}
		}
		return;
	}

	if (bLocal)
	{
		UpdateStance(bAiming);
	}
	if (bDrives)
	{
		HoldToWall(DeltaTime);
	}
	// The hold can end cover (the wall ran out); the clip is not restarted after that.
	if (bInCover)
	{
		UpdateAnimation(bAiming);
	}
}

void UL2H_CoverComponent::UpdateStance(bool bAiming)
{
	AL2H_BaseCharacter* Character = GetCharacter();
	if (bHighCover || !Character)
	{
		return;
	}

	// Over a low wall the shot needs the body up; it goes back down when the aim is let go.
	if (bAiming && !bStoodUpToAim)
	{
		bStoodUpToAim = true;
		Character->UnCrouch();
	}
	else if (!bAiming && bStoodUpToAim)
	{
		bStoodUpToAim = false;
		Character->Crouch();
	}
}

void UL2H_CoverComponent::HoldToWall(float DeltaTime)
{
	AL2H_BaseCharacter* Character = GetCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float FeetZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	const FVector Normal = FVector(CoverNormal);
	const FVector Location = Character->GetActorLocation();
	const FVector Start(Location.X, Location.Y, FeetZ + LowCoverMinHeight);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(L2HCoverHold), false, Character);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - Normal * (Radius + WallGap + 60.0f), ECC_Visibility, Params))
	{
		// The wall is gone (it ended under the body, or the body was pushed off it).
		ForceExitCover();
		return;
	}

	// The wall's own facing, followed as it curves; the back stays to it unless aiming turns the body.
	const FVector WallNormal = Hit.ImpactNormal.GetSafeNormal2D();
	if (!WallNormal.IsNearlyZero())
	{
		CoverNormal = WallNormal;
	}

	// Pulled back to a fixed gap from the wall, eased so it does not snap.
	const float Distance = FVector::DotProduct(Start - Hit.ImpactPoint, FVector(CoverNormal));
	const float Error = (Radius + WallGap) - Distance;
	if (FMath::Abs(Error) > 0.5f)
	{
		const float Step = FMath::Clamp(Error, -300.0f * DeltaTime, 300.0f * DeltaTime);
		Character->SetActorLocation(Location + FVector(CoverNormal) * Step, true);
	}

	// This frame's move, set after the Blueprint wrote its gait values: the clips' pace (the locomotion
	// ignores input size here, measured 225 cm/s), no turning toward the movement, and no sliding on past
	// the wall's end.
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement(); Move && !IsOwnerAiming())
	{
		Move->MaxWalkSpeed = MoveSpeed;
		Move->MaxWalkSpeedCrouched = MoveSpeed;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;

		const FVector Right = FVector::CrossProduct(FVector::UpVector, FVector(CoverNormal));
		const float Along = FVector::DotProduct(Move->Velocity, Right);
		if (FMath::Abs(Along) > 1.0f && !HasWallAt(FMath::Sign(Along) * EdgeLookAhead(Radius)))
		{
			Move->Velocity = FVector(0.0f, 0.0f, Move->Velocity.Z);
		}
	}

	// While entering, the capsule keeps facing the wall (the clip does the turning).
	if (!IsOwnerAiming() && Phase == EL2H_CoverPhase::Holding)
	{
		const FRotator Facing = FVector(CoverNormal).Rotation();
		Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), FRotator(0.0f, Facing.Yaw, 0.0f), DeltaTime, 12.0f));
	}
}

void UL2H_CoverComponent::UpdateAnimation(bool bAiming)
{
	AL2H_BaseCharacter* Character = GetCharacter();
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// Aiming hands the body back to the locomotion and weapon layers.
	if (bAiming)
	{
		StopCoverMontage();
		return;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, FVector(CoverNormal));
	const float Along = FVector::DotProduct(Character->GetVelocity(), Right);
	const bool bMoving = FMath::Abs(Along) > 20.0f;
	const bool bSideLocked = GetWorld() && GetWorld()->GetTimeSeconds() < SideLockUntil;
	if (bSideLocked)
	{
		// The side the last clip ended on holds for a moment (see SideLockUntil).
	}
	else if (bMoving)
	{
		bFaceRight = Along > 0.0f;
	}
	else if (IntentFrame + 2 >= GFrameCounter && IntentRight != 0)
	{
		// Pushing against the wall's end: no movement, but the body turns to that side to look out.
		bFaceRight = IntentRight > 0;
	}

	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const bool bAtEdge = !bMoving && !HasWallAt((bFaceRight ? 1.0f : -1.0f) * (Radius + 15.0f));

	UAnimSequenceBase* Wanted = nullptr;
	if (bHighCover)
	{
		Wanted = bMoving ? (bFaceRight ? HighMoveRight : HighMoveLeft)
			: bAtEdge ? (bFaceRight ? HighPeekRight : HighPeekLeft)
			: (bFaceRight ? HighIdleRight : HighIdleLeft);
	}
	else
	{
		Wanted = bMoving ? (bFaceRight ? LowMoveRight : LowMoveLeft)
			: bAtEdge ? (bFaceRight ? LowPeekRight : LowPeekLeft)
			: (bFaceRight ? LowIdleRight : LowIdleLeft);
	}

	if (!Wanted)
	{
		StopCoverMontage();
		return;
	}

	const bool bStillPlaying = PlayingMontage && AnimInstance->Montage_IsPlaying(PlayingMontage);
	if (Wanted == PlayingAnim && bStillPlaying)
	{
		return;
	}

	// Looped in place: the movement itself comes from the capsule. Straight after entering there is no
	// blend: the entering clip's last pose is this one's first.
	const float BlendIn = bSnapNextClip ? 0.0f : 0.25f;
	bSnapNextClip = false;
	PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Wanted, CoverSlot, BlendIn, 0.25f, 1.0f, 1000);
	PlayingAnim = Wanted;
}

void UL2H_CoverComponent::StopCoverMontage()
{
	AL2H_BaseCharacter* Character = GetCharacter();
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && PlayingMontage && AnimInstance->Montage_IsPlaying(PlayingMontage))
	{
		AnimInstance->Montage_Stop(0.25f, PlayingMontage);
	}
	PlayingMontage = nullptr;
	PlayingAnim = nullptr;
}
