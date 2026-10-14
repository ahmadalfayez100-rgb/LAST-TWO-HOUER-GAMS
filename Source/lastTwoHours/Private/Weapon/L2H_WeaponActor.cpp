// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/L2H_WeaponActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"
#include "Weapon/L2H_WeaponDataAsset.h"

AL2H_WeaponActor::AL2H_WeaponActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);

	// No collision at all. The weapon sits in front of its owner's eyes, so anything it could block is
	// exactly what the fire ability is trying to trace through.
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	// The second of the weapon's two possible bodies; exactly one is ever visible. Collision is off for the
	// same reason as the skeletal mesh -- the weapon hangs in front of the shot the fire ability traces.
	StaticMeshView = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshView"));
	StaticMeshView->SetupAttachment(SceneRoot);
	StaticMeshView->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshView->SetGenerateOverlapEvents(false);
	StaticMeshView->SetVisibility(false, /*bPropagateToChildren=*/true);
}

void AL2H_WeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AL2H_WeaponActor, WeaponData);
	DOREPLIFETIME(AL2H_WeaponActor, bEquipped);
}

void AL2H_WeaponActor::SetWeaponData(UL2H_WeaponDataAsset* InData)
{
	if (!HasAuthority())
	{
		return;
	}

	WeaponData = InData;
	ApplyVisualState();
}

void AL2H_WeaponActor::SetWeaponOwner(AActor* InOwner)
{
	WeaponOwner = InOwner;
	ApplyVisualState();
}

void AL2H_WeaponActor::SetEquipped(bool bNewEquipped)
{
	if (!HasAuthority() || bEquipped == bNewEquipped)
	{
		return;
	}

	bEquipped = bNewEquipped;
	ApplyVisualState();
}

void AL2H_WeaponActor::OnRep_WeaponData()
{
	ApplyVisualState();
}

void AL2H_WeaponActor::OnRep_Equipped()
{
	ApplyVisualState();
}

void AL2H_WeaponActor::ApplyVisualState()
{
	ACharacter* Character = Cast<ACharacter>(WeaponOwner);
	if (!Character || !WeaponMesh || !StaticMeshView || !WeaponData)
	{
		// Nothing to do yet rather than an error: the owner and the data arrive as separate replicated
		// properties, so this runs again when whichever one is missing lands.
		return;
	}

	const bool bDrawStatic = UsesStaticMesh();

	if (!bDrawStatic && WeaponMesh->GetSkeletalMeshAsset() != WeaponData->Data.Mesh)
	{
		WeaponMesh->SetSkeletalMesh(WeaponData->Data.Mesh);
	}

	if (bDrawStatic && StaticMeshView->GetStaticMesh() != WeaponData->Data.StaticMesh)
	{
		StaticMeshView->SetStaticMesh(WeaponData->Data.StaticMesh);
	}

	// Both bodies exist on every weapon; the authored data decides which one is seen. Set every apply rather
	// than only on a change, so a client that receives the data and the equipped flag out of order still
	// ends up with exactly one body showing.
	// A weapon with no holster socket is put away out of sight rather than snapped to the body root, where
	// it would cut through the torso.
	const bool bShown = bEquipped || WeaponData->Data.Sockets.Holster != NAME_None;
	WeaponMesh->SetVisibility(!bDrawStatic && bShown, /*bPropagateToChildren=*/true);
	StaticMeshView->SetVisibility(bDrawStatic && bShown, /*bPropagateToChildren=*/true);

	USceneComponent* View = GetActiveMeshView();

	USkeletalMeshComponent* Body = Character->GetMesh();
	if (!Body || !View)
	{
		return;
	}

	const FName SocketName = bEquipped ? WeaponData->Data.Sockets.Equip : WeaponData->Data.Sockets.Holster;

	if (SocketName != NAME_None && Body->DoesSocketExist(SocketName))
	{
		View->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
	else
	{
		// Attached to the body root instead of dropped. A weapon that vanished because a socket name was
		// misspelled would look like a spawn failure, and the log line names the real cause.
		View->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
		L2HTime::LogVerbose(FString::Printf(
			TEXT("Weapon %s: socket '%s' missing on %s, attached to the body root instead"),
			*GetName(),
			*SocketName.ToString(),
			*GetNameSafe(Character)));
	}
	
	// Written after the attach and on every apply: SnapToTargetNotIncludingScale leaves the component's own
	// scale alone, so the displayed size is otherwise exactly what the pack exported. The default is 1, so a
	// weapon that needed no correction is unchanged.
	View->SetRelativeScale3D(WeaponData->Data.MeshRelativeScale);
}

FTransform AL2H_WeaponActor::GetFireSocketTransform() const
{
	const FName SocketName = WeaponData ? WeaponData->Data.FireSocketName : NAME_None;

	// Whichever body draws the weapon owns the muzzle: a rigged weapon keeps the socket on its skeleton,
	// a static one on its static mesh. Asking the wrong one would silently fall back to the actor origin.
	USceneComponent* View = GetActiveMeshView();

	if (View && SocketName != NAME_None && View->DoesSocketExist(SocketName))
	{
		return View->GetSocketTransform(SocketName);
	}

	// The mesh, not the actor: only the mesh is attached to the hand, the actor root stays where the
	// weapon was spawned, so a weapon without a muzzle socket would otherwise fire from the spawn point.
	return View ? View->GetComponentTransform() : GetActorTransform();
}

bool AL2H_WeaponActor::UsesStaticMesh() const
{
	// A rigged mesh wins when the pack shipped one, because only it can play the weapon's own fire and
	// reload animations. With neither field authored this answers false and nothing is drawn, which is the
	// same state an unauthored weapon was already in.
	return WeaponData && WeaponData->Data.Mesh == nullptr && WeaponData->Data.StaticMesh != nullptr;
}

USceneComponent* AL2H_WeaponActor::GetActiveMeshView() const
{
	return UsesStaticMesh() ? static_cast<USceneComponent*>(StaticMeshView.Get())
	                        : static_cast<USceneComponent*>(WeaponMesh.Get());
}
