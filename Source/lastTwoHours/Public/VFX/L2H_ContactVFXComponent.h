// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "VFX/L2H_VFXTypes.h"

#include "L2H_ContactVFXComponent.generated.h"

class UL2H_VFXLibrary;
class UNiagaraComponent;
class USoundBase;

/**
 * The one place this pawn's contact effects are spawned.
 *
 * Deliberately not a subclass of the Foley component. The anim notifies resolve their target with
 * GetComponentByClass(AC_FoleyEvents), and a component added in a child Blueprint is searched after
 * the one the parent already owns, so a subclass would never be called. Driving off the pawn's own
 * movement, its Landed and OnJumped events and the parkour anim state reaches the same moments while
 * leaving the protected parkour pawn alone.
 *
 * Cosmetic only: nothing here is authoritative and nothing replicates. Each machine plays its own
 * copy of an effect it can see, which is also why the throttle is per machine rather than shared.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Contact VFX"))
class LASTTWOHOURS_API UL2H_ContactVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_ContactVFXComponent();

	/**
	 * Plays the row this pawn's library holds for the moment, and reports whether it spawned.
	 *
	 * ContactLocation is the world point that was touched and is what the surface is resolved from.
	 * Pass FVector::ZeroVector for a moment with no contact point -- a jump, an ability -- and the
	 * character's own position is probed instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "L2H|VFX")
	bool PlayMomentVFX(EL2H_VFXMoment Moment, FName SocketOverride = NAME_None, FVector ContactLocation = FVector::ZeroVector);

	/** The surface under a point, or SurfaceType_Default when nothing was hit. */
	UFUNCTION(BlueprintCallable, Category = "L2H|VFX")
	EPhysicalSurface ResolveSurface(FVector ContactLocation) const;

	/** Swaps this pawn's identity. Clears the throttle so the new look does not start muted. */
	UFUNCTION(BlueprintCallable, Category = "L2H|VFX")
	void SetLibrary(UL2H_VFXLibrary* InLibrary);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The library that gives this pawn its look. One pawn, one library. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX")
	TObjectPtr<UL2H_VFXLibrary> Library;

	/** The socket a left-foot effect attaches to. Named per pawn because the parkour mesh is not the stock one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX")
	FName LeftFootSocket = TEXT("foot_l");

	/** The socket a right-foot effect attaches to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX")
	FName RightFootSocket = TEXT("foot_r");

	/** Ground travel between two footstep effects, in cm. 0 turns automatic footfalls off entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX", meta = (ClampMin = "0.0"))
	float StepDistance = 220.0f;

	/** Below this fraction of MaxWalkSpeed nothing counts as travel, so shuffling in place stays silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StepSpeedFraction = 0.5f;

	/** How far below the contact point the ground is looked for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX", meta = (ClampMin = "0.0"))
	float SurfaceTraceDistance = 200.0f;

	/** Multiplies every row's cooldown. 0 removes the throttle entirely, for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|VFX", meta = (ClampMin = "0.0"))
	float CooldownScale = 1.0f;

	/**
	 * Footstep sound per surface, played at the foot on every plant. The default surface (concrete) is left
	 * to GASP's own foley, which already plays on its anim notifies; these add the other ground types.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Audio")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> FootstepSounds;

	/** A foot counts as planted below this height (foot bone above the capsule's bottom, cm)... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Audio", meta = (ClampMin = "0.0"))
	float FootPlantHeight = 18.0f;

	/** ...and as lifted above this one. The gap keeps a foot hovering at the line from clicking twice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Audio", meta = (ClampMin = "0.0"))
	float FootLiftHeight = 28.0f;

	/** Volume at a slow walk; full volume at MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float QuietStepVolume = 0.45f;

private:
	/** When each row was last allowed to spawn, keyed the way the library is. */
	UPROPERTY(Transient)
	TMap<FL2H_VFXKey, float> LastSpawnTimes;

	/** Rows already announced, so a resolution is logged once instead of once per footstep. */
	UPROPERTY(Transient)
	TSet<FL2H_VFXKey> ReportedSpawned;

	/** Moments already reported as missing from the library. */
	UPROPERTY(Transient)
	TSet<EL2H_VFXMoment> ReportedMissing;

	/** Ground travelled since the last footstep effect. */
	float DistanceSinceStep = 0.0f;

	/** Which foot the next footstep effect attaches to. */
	bool bNextStepIsLeft = true;

	const FL2H_VFXEntry* ResolveEntry(EL2H_VFXMoment Moment, EPhysicalSurface Surface);

	UNiagaraComponent* SpawnEntry(const FL2H_VFXEntry& Entry, FName Socket, FVector ContactLocation);

	void UpdateFootsteps(float DeltaTime);

	/** Foot-plant detection from the foot bones, for the footstep sound. */
	void UpdateFootPlants();
	bool bLeftFootLifted = false;
	bool bRightFootLifted = false;
};
