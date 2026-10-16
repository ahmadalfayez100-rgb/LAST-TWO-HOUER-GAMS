// Copyright Epic Games, Inc. All Rights Reserved.

#include "VFX/L2H_ContactVFXComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Time/L2HTimeLog.h"
#include "TimerManager.h"
#include "VFX/L2H_VFXLibrary.h"

namespace
{
	/** The enum's own name, so a log line reads the same as the row it could not find. */
	FString MomentName(EL2H_VFXMoment Moment)
	{
		return StaticEnum<EL2H_VFXMoment>()->GetNameStringByValue(static_cast<int64>(Moment));
	}
}

UL2H_ContactVFXComponent::UL2H_ContactVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Before physics, so a footstep effect is already in the world for the frame it belongs to.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Cosmetic only. Every machine plays its own copy of what it can see, so a replicated effect would
	// be bandwidth spent on something with no gameplay consequence.
	SetIsReplicatedByDefault(false);

	// Surface footsteps from ResidentHorrorV2, in the order the project defines its surface types
	// (DefaultEngine.ini: 1 Grass, 2 Wood, 3 Metal, 4 Water, 5 Glass, 6 Snow, 7 Gravel, 8 Mud, 9 Leaves, 10 Ice).
	static const TCHAR* const SurfaceCues[] = {
		TEXT("CUE_grass_1_Cue"), TEXT("CUE_Floor_Mono_05_Cue"), TEXT("CUE_Footstep_Metal_Boots_Jog_1_Cue1"),
		TEXT("CUE_water_6_Cue"), TEXT("CUE_Glass_Walk_1_Cue"), TEXT("CUE_Snow_Mono_01_Cue"), TEXT("CUE_Grit_Mono_01_Cue"),
		TEXT("CUE_Mud_Mono_05_Cue"), TEXT("CUE_FootstepLeaves_Boots_Jog_1_Cue"), TEXT("CUE_Ice_Mono_05_Cue") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SurfaceCues); ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/ResidentHorrorV2/Audio/Cue/%s.%s"), SurfaceCues[Index], SurfaceCues[Index]);
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			FootstepSounds.Add(static_cast<EPhysicalSurface>(SurfaceType1 + Index), Sound);
		}
	}
}

void UL2H_ContactVFXComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Library)
	{
		// Loud on purpose: a pawn with no library looks identical to a pawn whose every row is fine.
		L2HTime::LogImportant(FString::Printf(
			TEXT("ContactVFX %s: no library assigned, so this pawn will play no contact effects"),
			*GetNameSafe(GetOwner())));
	}
}

void UL2H_ContactVFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFootsteps(DeltaTime);
	UpdateFootPlants();
}

void UL2H_ContactVFXComponent::UpdateFootPlants()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Mesh || !Movement || FootstepSounds.Num() == 0)
	{
		return;
	}

	// Off the ground every foot is "lifted", so the first contact after a jump or a climb is a step.
	if (!Movement->IsMovingOnGround())
	{
		bLeftFootLifted = bRightFootLifted = true;
		return;
	}

	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float GroundZ = Character->GetActorLocation().Z - HalfHeight;
	const float Speed = Movement->Velocity.Size2D();

	auto CheckFoot = [&](FName Socket, bool& bLifted)
	{
		const FVector Foot = Mesh->GetSocketLocation(Socket);
		const float Height = Foot.Z - GroundZ;
		if (Height > FootLiftHeight)
		{
			bLifted = true;
			return;
		}
		if (!bLifted || Height > FootPlantHeight)
		{
			return;
		}
		bLifted = false;
		if (Speed < 30.0f)
		{
			return;
		}

		const EPhysicalSurface Surface = ResolveSurface(Foot);
		const TObjectPtr<USoundBase>* Sound = FootstepSounds.Find(Surface);
		if (Sound && *Sound)
		{
			const float Pace = Movement->MaxWalkSpeed > 0.0f ? FMath::Clamp(Speed / Movement->MaxWalkSpeed, 0.0f, 1.0f) : 1.0f;
			UGameplayStatics::PlaySoundAtLocation(this, *Sound, Foot, FMath::Lerp(QuietStepVolume, 1.0f, Pace));
		}
	};
	CheckFoot(LeftFootSocket, bLeftFootLifted);
	CheckFoot(RightFootSocket, bRightFootLifted);
}

void UL2H_ContactVFXComponent::SetLibrary(UL2H_VFXLibrary* InLibrary)
{
	Library = InLibrary;

	// A different identity is a different set of rows, so keeping the old throttle would swallow the
	// first steps of the new look for no reason the player could explain.
	LastSpawnTimes.Reset();
	ReportedSpawned.Reset();
	ReportedMissing.Reset();
}

EPhysicalSurface UL2H_ContactVFXComponent::ResolveSurface(FVector ContactLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}

	// A moment with no contact point still has to answer "what am I standing on", so the pawn's own
	// position is used as the probe origin. The small lift keeps the trace from starting inside the
	// floor it is asking about.
	FVector Origin = ContactLocation;
	if (Origin.IsNearlyZero())
	{
		Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}
	Origin.Z += 4.0f;

	const FVector End = Origin - FVector(0.0f, 0.0f, SurfaceTraceDistance);

	FCollisionQueryParams Params(FName(TEXT("L2HContactVFXSurface")), /*bInTraceComplex=*/false, GetOwner());
	// The physical material is the entire point of this trace; without it every surface answers Default.
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
	{
		return SurfaceType_Default;
	}

	if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
	{
		return PhysMat->SurfaceType.GetValue();
	}

	return SurfaceType_Default;
}

const FL2H_VFXEntry* UL2H_ContactVFXComponent::ResolveEntry(EL2H_VFXMoment Moment, EPhysicalSurface Surface)
{
	if (!Library)
	{
		return nullptr;
	}

	const FL2H_VFXEntry* Found = Library->Resolve(Moment, Surface);
	if (!Found && !ReportedMissing.Contains(Moment))
	{
		// A hole in the library is a data mistake worth seeing, but not one worth a line every step.
		ReportedMissing.Add(Moment);
		L2HTime::LogImportant(FString::Printf(
			TEXT("ContactVFX %s: library has no row for %s"),
			*GetNameSafe(GetOwner()), *MomentName(Moment)));
	}

	return Found;
}

bool UL2H_ContactVFXComponent::PlayMomentVFX(EL2H_VFXMoment Moment, FName SocketOverride, FVector ContactLocation)
{
	if (Moment == EL2H_VFXMoment::None)
	{
		return false;
	}

	const EPhysicalSurface Surface = ResolveSurface(ContactLocation);
	const FL2H_VFXEntry* Entry = ResolveEntry(Moment, Surface);
	if (!Entry || Entry->System.IsNull())
	{
		return false;
	}

	FL2H_VFXKey Key;
	Key.Moment = Moment;
	Key.Surface = Surface;

	// One spawn per (moment, surface) per cooldown. A montage can raise the same moment twice in one
	// frame and a footstep arrives every step, so without this the layer becomes a pulse per frame.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Cooldown = Entry->CooldownSeconds * CooldownScale;
	if (Cooldown > 0.0f)
	{
		if (const float* Last = LastSpawnTimes.Find(Key))
		{
			if (Now - *Last < Cooldown)
			{
				if (L2HTime::IsDebugEnabled())
				{
					L2HTime::LogVerbose(FString::Printf(
						TEXT("ContactVFX %s: %s throttled (%.0f of %.0f ms)"),
						*GetNameSafe(GetOwner()), *MomentName(Moment), (Now - *Last) * 1000.0f, Cooldown * 1000.0f));
				}
				return false;
			}
		}
	}
	LastSpawnTimes.Add(Key, Now);

	const FName Socket = SocketOverride.IsNone() ? Entry->SocketName : SocketOverride;
	UNiagaraComponent* Spawned = SpawnEntry(*Entry, Socket, ContactLocation);
	if (!Spawned)
	{
		return false;
	}

	// The first time a row resolves is the line worth always seeing, because it is the proof the
	// library and the pawn agree. Everything after it is noise at footstep frequency.
	if (!ReportedSpawned.Contains(Key))
	{
		ReportedSpawned.Add(Key);
		L2HTime::LogImportant(FString::Printf(
			TEXT("ContactVFX %s: %s on surface %d -> %s at %s"),
			*GetNameSafe(GetOwner()),
			*MomentName(Moment),
			static_cast<int32>(Surface),
			*GetNameSafe(Spawned->GetAsset()),
			*Socket.ToString()));
	}
	else if (L2HTime::IsDebugEnabled())
	{
		L2HTime::LogVerbose(FString::Printf(
			TEXT("ContactVFX %s: %s -> %s at %s"),
			*GetNameSafe(GetOwner()), *MomentName(Moment), *GetNameSafe(Spawned->GetAsset()), *Socket.ToString()));
	}

	return true;
}

UNiagaraComponent* UL2H_ContactVFXComponent::SpawnEntry(const FL2H_VFXEntry& Entry, FName Socket, FVector ContactLocation)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UNiagaraSystem* System = Entry.System.LoadSynchronous();
	if (!System)
	{
		L2HTime::LogImportant(FString::Printf(
			TEXT("ContactVFX %s: could not load %s"),
			*GetNameSafe(Owner), *Entry.System.ToString()));
		return nullptr;
	}

	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = Entry.RotationOffset;

	if (Entry.bAttachToSocket)
	{
		// The character mesh, not the actor root, because a socket only exists on the mesh. The
		// location stays zero so the effect rides the socket it named rather than a world offset.
		if (const ACharacter* Character = Cast<ACharacter>(Owner))
		{
			AttachParent = Character->GetMesh();
		}
		AttachSocket = Socket;
	}
	else
	{
		// A world-space effect needs a point to stand on. Spawning it at the world origin instead is a
		// bug that only shows up on a map that is not built around (0,0,0).
		AttachParent = Owner->GetRootComponent();
		if (ContactLocation.IsNearlyZero())
		{
			Location = Owner->GetActorLocation();
			Rotation += Owner->GetActorRotation();
		}
		else
		{
			Location = ContactLocation;
		}
	}

	if (!AttachParent)
	{
		return nullptr;
	}

	// SpawnSystemAttached builds a plain, unregistered-on-the-network component that destroys itself
	// when the system finishes, so this effect exists only on the machine that asked for it.
	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System,
		AttachParent,
		AttachSocket,
		Location,
		Rotation,
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/true,
		/*bAutoActivate=*/true);

	if (!Component)
	{
		return nullptr;
	}

	// The two parameters every system in this layer is authored to read, which is what lets one asset
	// serve both characters and be re-tinted by data alone. A system that does not declare them
	// simply ignores them.
	Component->SetVariableFloat(TEXT("User.Scale"), Entry.Scale);
	// A row with no colour of its own borrows the character's identity colour, and a character that
	// has neither keeps the system's own look. A fully transparent colour is what "not authored"
	// looks like on disk, and taking it for a real tint renders every effect that reads
	// User.PaletteColor invisible -- a failure that reads as a broken system, not a missing colour.
	FLinearColor Tint = Entry.PaletteColor;
	if (Tint.A <= 0.0f && Library)
	{
		Tint = Library->IdentityLook.Primary;
	}

	if (Tint.A > 0.0f)
	{
		Component->SetVariableLinearColor(TEXT("User.PaletteColor"), Tint);
	}

	return Component;
}

void UL2H_ContactVFXComponent::UpdateFootsteps(float DeltaTime)
{
	if (StepDistance <= 0.0f || !Library)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		// Airborne travel is not a footstep, and holding the accumulator through a jump would drop one
		// on the frame of the landing instead of on the step that follows it.
		DistanceSinceStep = 0.0f;
		return;
	}

	const float Speed = Movement->Velocity.Size2D();
	const float Threshold = Movement->MaxWalkSpeed * StepSpeedFraction;
	if (Threshold > 0.0f && Speed < Threshold)
	{
		return;
	}

	DistanceSinceStep += Speed * DeltaTime;
	if (DistanceSinceStep < StepDistance)
	{
		return;
	}

	DistanceSinceStep = 0.0f;

	const FName Socket = bNextStepIsLeft ? LeftFootSocket : RightFootSocket;
	bNextStepIsLeft = !bNextStepIsLeft;

	PlayMomentVFX(EL2H_VFXMoment::Footstep, Socket, FVector::ZeroVector);
}
