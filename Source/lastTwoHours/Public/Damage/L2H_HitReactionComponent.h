// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Damage/L2H_DamageTypes.h"

#include "L2H_HitReactionComponent.generated.h"

class UL2H_DamageTypeDataAsset;

/**
 * How this actor responds to being hit, per damage type.
 *
 * The damage NUMBER is not decided here -- time only ever moves through AL2H_TimeDirector, and a component
 * that wrote the attribute itself would be a second pathway to the one number the game is built on. What
 * this owns is the response: the shake the shooter feels, the montage the target plays, and the effect and
 * sound at the point of impact.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Hit Reaction"))
class LASTTWOHOURS_API UL2H_HitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_HitReactionComponent();

	/**
	 * The response authored for a damage type, or null when this actor has none.
	 *
	 * Returns a pointer rather than a copy because the caller reads two or three fields of it. Not a
	 * UFUNCTION: the reflection layer has no way to hand a struct pointer back, so the Blueprint path is
	 * TryGetResponse below.
	 */
	const FL2H_DamageResponse* FindResponse(EL2H_DamageType DamageType) const;

	/** The Blueprint-facing form of FindResponse. False leaves OutResponse untouched. */
	UFUNCTION(BlueprintCallable, Category = "L2H|Damage")
	bool TryGetResponse(EL2H_DamageType DamageType, FL2H_DamageResponse& OutResponse) const;

	/**
	 * Server only. Shakes the shooter's screen and tells every machine to play the impact.
	 *
	 * The shake goes to one specific machine and the rest is multicast, which is why this is two calls and
	 * not one: a shake played everywhere would shake the target's screen for the crime of being shot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Damage")
	void ApplyHitReaction(EL2H_DamageType DamageType, AActor* InstigatorActor, bool bHeadshot, FVector HitLocation, FVector ShotDirection);

	/** The reaction the target plays and the effect in the world, on every machine that can see it. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitEffects(EL2H_DamageType DamageType, bool bHeadshot, FVector HitLocation, FVector ShotDirection);

protected:
	/**
	 * Authored responses, one per damage type this actor cares about.
	 *
	 * A map rather than an array so a missing type is an answer rather than a search. Because these are
	 * component defaults, every machine holds the same map without it ever being sent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Damage")
	TMap<EL2H_DamageType, TObjectPtr<UL2H_DamageTypeDataAsset>> DamageResponses;

	/** How much harder the screen kicks on a headshot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "1.0"))
	float HeadshotShakeScale = 1.25f;
};
