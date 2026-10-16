// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_HidingCloset.h"

#include "Animation/AnimSequenceBase.h"
#include "Character/L2H_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Time/L2HTimeLog.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "L2HCloset"

AL2H_HidingCloset::AL2H_HidingCloset()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	// The Interact trace (ECC_GameTraceChannel4) ignores everything by default.
	Body->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
	DoorLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeft"));
	DoorLeft->SetupAttachment(Body);
	DoorRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorRight"));
	DoorRight->SetupAttachment(Body);
	for (UStaticMeshComponent* Door : { DoorLeft.Get(), DoorRight.Get() })
	{
		Door->SetCollisionProfileName(TEXT("BlockAll"));
		// A door swinging into a pawn must not shove it.
		Door->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// ResidentHorrorV2's closet and its wardrobe sound.
	Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ResidentHorrorV2/Demo/Meshes/SM_Closet_Body.SM_Closet_Body"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	DoorLeft->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ResidentHorrorV2/Demo/Meshes/SM_Closet_Door1.SM_Closet_Door1"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	DoorRight->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ResidentHorrorV2/Demo/Meshes/SM_Closet_Door2.SM_Closet_Door2"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	DoorSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/ResidentHorrorV2/Audio/SoundWaves/WAV_Wardrobe_Close_01.WAV_Wardrobe_Close_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	OpenAnim = LoadObject<UAnimSequenceBase>(nullptr, TEXT("/Game/_Project/animation/L2H/Closet/AS_L2H_OpenCloset.AS_L2H_OpenCloset"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AL2H_HidingCloset::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AL2H_HidingCloset, bDoorsOpen);
	DOREPLIFETIME(AL2H_HidingCloset, Occupant);
	DOREPLIFETIME(AL2H_HidingCloset, bInside);
}

void AL2H_HidingCloset::BeginPlay()
{
	Super::BeginPlay();
	MeasureDoors();
}

void AL2H_HidingCloset::MeasureDoors()
{
	const UStaticMesh* BodyMesh = Body->GetStaticMesh();
	const FBox BodyBox = BodyMesh ? BodyMesh->GetBoundingBox() : FBox(FVector(-50.0f), FVector(50.0f));
	BodyCentre = BodyBox.GetCenter();
	FloorZ = BodyBox.Min.Z;

	UStaticMeshComponent* Doors[2] = { DoorLeft, DoorRight };
	FBox DoorBoxes[2];
	FVector DoorsCentre = FVector::ZeroVector;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const UStaticMesh* Mesh = Doors[Index]->GetStaticMesh();
		const FBox Local = Mesh ? Mesh->GetBoundingBox() : FBox(FVector::ZeroVector, FVector::ZeroVector);
		Hinges[Index].Closed = Doors[Index]->GetRelativeTransform();
		DoorBoxes[Index] = Local.TransformBy(Hinges[Index].Closed);
		DoorsCentre += DoorBoxes[Index].GetCenter() * 0.5f;
	}

	// The front is the side the doors are on.
	FVector Front = FVector(DoorsCentre - BodyCentre).GetSafeNormal2D();
	if (!bFrontFromDoors || Front.IsNearlyZero())
	{
		Front = FRotator(0.0f, FrontYaw, 0.0f).Vector();
	}
	FrontLocal = Front;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Front);
	BodyHalfDepth = FMath::Abs(FVector::DotProduct(BodyBox.GetExtent(), Front.GetAbs()));

	// Each door turns about its outer vertical edge, and outwards.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FBox& Box = DoorBoxes[Index];
		const FVector Centre = Box.GetCenter();
		const float Extent = FMath::Abs(FVector::DotProduct(Box.GetExtent(), Side.GetAbs()));
		const float Outward = FVector::DotProduct(Centre - BodyCentre, Side) >= 0.0f ? 1.0f : -1.0f;
		Hinges[Index].Hinge = Centre + Side * Outward * Extent;
		const FVector Swung = Hinges[Index].Hinge + FQuat(FVector::UpVector, HALF_PI).RotateVector(Centre - Hinges[Index].Hinge);
		Hinges[Index].Sign = FVector::DotProduct(Swung - Centre, Front) >= 0.0f ? 1.0f : -1.0f;
	}
}

void AL2H_HidingCloset::PoseDoors()
{
	UStaticMeshComponent* Doors[2] = { DoorLeft, DoorRight };
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, DoorAlpha, 2.0f);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Hinges[Index].Sign * DoorOpenAngle * Eased));
		const FTransform& Closed = Hinges[Index].Closed;
		Doors[Index]->SetRelativeLocationAndRotation(
			Hinges[Index].Hinge + Turn.RotateVector(Closed.GetLocation() - Hinges[Index].Hinge),
			Turn * Closed.GetRotation());
	}
}

void AL2H_HidingCloset::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bDoorsOpen ? 1.0f : 0.0f;
	if (DoorAlpha != Target)
	{
		DoorAlpha = FMath::FInterpConstantTo(DoorAlpha, Target, DeltaSeconds, DoorSpeed / FMath::Max(DoorOpenAngle, 1.0f));
		PoseDoors();
	}

	if (!HasAuthority() || !Occupant)
	{
		return;
	}
	// Pushed back out by going down; let go of by anything else that moved them away (a teleport, a rewind).
	if (bInside && (Occupant->IsDowned() || Occupant->IsDead()))
	{
		Leave();
	}
	else if (bInside && FVector::DistSquared2D(Occupant->GetActorLocation(), GetActorTransform().TransformPosition(BodyCentre)) > FMath::Square(BodyHalfDepth + 80.0f))
	{
		L2HTime::LogImportant(FString::Printf(TEXT("Closet: %s left %s by other means"), *GetNameSafe(Occupant), *GetNameSafe(this)));
		if (UCharacterMovementComponent* Move = Occupant->GetCharacterMovement(); Move && Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		Occupant->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
		Occupant = nullptr;
		bInside = false;
		SetDoorsOpen(false);
	}
}

bool AL2H_HidingCloset::IsHiding(const AActor* Who)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AL2H_HidingCloset> It(World); It; ++It)
	{
		if (It->Occupant == Who && It->bInside && !It->bDoorsOpen)
		{
			return true;
		}
	}
	return false;
}

FVector AL2H_HidingCloset::GetFront() const
{
	return GetActorTransform().TransformVectorNoScale(FrontLocal).GetSafeNormal2D();
}

FVector AL2H_HidingCloset::GetOutsidePoint() const
{
	const FVector Centre = GetActorTransform().TransformPosition(BodyCentre);
	return Centre + GetFront() * (BodyHalfDepth + OutsideDistance);
}

void AL2H_HidingCloset::L2H_GetInteractPrompt_Implementation(bool& bCanInteract, FText& OutVerb, FText& OutTargetName)
{
	OutTargetName = LOCTEXT("Name", "Closet");
	if (!Occupant)
	{
		bCanInteract = true;
		OutVerb = LOCTEXT("Hide", "Hide");
		return;
	}
	// Only the one inside can open it again; the prompt itself does not know who is asking.
	bCanInteract = bInside && !bDoorsOpen;
	OutVerb = LOCTEXT("Leave", "Leave");
}

void AL2H_HidingCloset::L2H_Interact_Implementation(AActor* Interactor)
{
	AL2H_BaseCharacter* Character = Cast<AL2H_BaseCharacter>(Interactor);
	if (!HasAuthority() || !Character)
	{
		return;
	}
	if (!Occupant)
	{
		Enter(Character);
	}
	else if (Occupant == Character && bInside && !bDoorsOpen)
	{
		Leave();
	}
}

void AL2H_HidingCloset::Enter(AL2H_BaseCharacter* Character)
{
	UWorld* World = GetWorld();
	if (!World || Character->IsDowned() || Character->IsDead() || Character->IsBeingCarried())
	{
		return;
	}
	Occupant = Character;
	bInside = false;

	// Squared up in front of the doors, then the clip: the hand reaches, and pulls them open at DoorPullTime.
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Stand = GetActorTransform().TransformPosition(BodyCentre) + GetFront() * (BodyHalfDepth + ReachDistance);
	const FVector Floor = GetActorTransform().TransformPosition(FVector(BodyCentre.X, BodyCentre.Y, FloorZ));
	MulticastPlace(Character, FVector(Stand.X, Stand.Y, Floor.Z + HalfHeight + 2.0f), (-GetFront()).Rotation().Yaw, false);
	if (OpenAnim)
	{
		Character->PlayInteractAnim(OpenAnim, FVector::ZeroVector, EnterSeconds + 0.1f, EnterSeconds + 0.1f);
	}

	World->GetTimerManager().SetTimer(DoorTimer, FTimerDelegate::CreateUObject(this, &AL2H_HidingCloset::SetDoorsOpen, true), FMath::Max(OpenAnim ? DoorPullTime : 0.0f, 0.01f), false);
	World->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &AL2H_HidingCloset::StepIn), FMath::Max(EnterSeconds, 0.05f), false);
	L2HTime::LogImportant(FString::Printf(TEXT("Closet: %s opens %s"), *GetNameSafe(Character), *GetNameSafe(this)));
}

void AL2H_HidingCloset::StepIn()
{
	AL2H_BaseCharacter* Character = Occupant;
	UWorld* World = GetWorld();
	if (!Character || !World || Character->IsDowned() || Character->IsDead())
	{
		Occupant = nullptr;
		SetDoorsOpen(false);
		return;
	}

	// In, turned to face out, and held there: no walking, and the closet's own walls are not in the way.
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Inside = GetActorTransform().TransformPosition(FVector(BodyCentre.X, BodyCentre.Y, FloorZ) + InsideOffset) + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	const FRotator Facing(0.0f, GetFront().Rotation().Yaw, 0.0f);
	MulticastPlace(Character, Inside, Facing.Yaw, true);
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->ClientSetRotation(Facing);
	}
	bInside = true;

	World->GetTimerManager().SetTimer(DoorTimer, FTimerDelegate::CreateUObject(this, &AL2H_HidingCloset::SetDoorsOpen, false), FMath::Max(CloseDelay, 0.05f), false);
	L2HTime::LogImportant(FString::Printf(TEXT("Closet: %s hides in %s"), *GetNameSafe(Character), *GetNameSafe(this)));
}

void AL2H_HidingCloset::Leave()
{
	UWorld* World = GetWorld();
	if (!World || !Occupant)
	{
		return;
	}
	SetDoorsOpen(true);
	World->GetTimerManager().ClearTimer(DoorTimer);
	World->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &AL2H_HidingCloset::StepOut), 0.35f, false);
}

void AL2H_HidingCloset::StepOut()
{
	AL2H_BaseCharacter* Character = Occupant;
	Occupant = nullptr;
	bInside = false;
	if (Character)
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Out = GetOutsidePoint();
		const FVector Floor = GetActorTransform().TransformPosition(FVector(BodyCentre.X, BodyCentre.Y, FloorZ));
		MulticastPlace(Character, FVector(Out.X, Out.Y, Floor.Z + HalfHeight + 2.0f), GetFront().Rotation().Yaw, false);
		L2HTime::LogImportant(FString::Printf(TEXT("Closet: %s leaves %s"), *GetNameSafe(Character), *GetNameSafe(this)));
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DoorTimer, FTimerDelegate::CreateUObject(this, &AL2H_HidingCloset::SetDoorsOpen, false), FMath::Max(CloseDelay, 0.05f), false);
	}
}

void AL2H_HidingCloset::SetDoorsOpen(bool bOpen)
{
	if (bDoorsOpen == bOpen)
	{
		return;
	}
	bDoorsOpen = bOpen;
	OnRep_DoorsOpen();
}

void AL2H_HidingCloset::OnRep_DoorsOpen()
{
	if (DoorSound && !bDoorsOpen)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DoorSound, GetActorTransform().TransformPosition(BodyCentre), 0.7f);
	}
}

void AL2H_HidingCloset::MulticastPlace_Implementation(AL2H_BaseCharacter* Character, FVector_NetQuantize Location, float Yaw, bool bHold)
{
	if (!Character)
	{
		return;
	}
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Move->StopMovementImmediately();
	Character->GetCapsuleComponent()->IgnoreActorWhenMoving(this, bHold);
	Character->SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	if (bHold)
	{
		Move->DisableMovement();
	}
	else if (Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

#undef LOCTEXT_NAMESPACE
