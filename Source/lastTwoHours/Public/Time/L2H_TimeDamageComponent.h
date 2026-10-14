// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "L2H_TimeDamageComponent.generated.h"

class UPrimitiveComponent;

/**
 * A hazard's damage source.
 *
 * Drop this on a spike trap, a falling crate or a damage volume and it will take time off any
 * TimeCore pawn it touches. It never touches the Time attribute itself: every hit goes through
 * AL2H_TimeDirector, which is the single place that decides what a hit is worth and whether it was
 * heavy enough to spill a shard.
 *
 * The TMap throttle is what makes a volume usable at all -- an overlap fires every frame while two
 * bodies remain in contact, and RetriggerDelay turns that into one hit per crossing.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Time Damage"))
class LASTTWOHOURS_API UL2H_TimeDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_TimeDamageComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Takes DamageAmount off Target on the server. Safe to call directly from Blueprint. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "L2H|Damage")
	void ApplyTo(AActor* Target);

protected:
	/** Seconds taken off per hit. Anything at or above the level's ShardThreshold spills a shard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "0.0"))
	float DamageAmount = 20.0f;

	/** Damages any TimeCore pawn that touches the owner's root primitive. Off for call-only hazards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage")
	bool bDamageOnOverlap = true;

	/** Minimum seconds between two hits on the same actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Damage", meta = (ClampMin = "0.0"))
	float RetriggerDelay = 1.0f;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** The owner's root primitive, which is the only thing an overlap can be bound on. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> WatchedPrimitive;

	/** World-seconds of the last hit, per actor. */
	TMap<TWeakObjectPtr<AActor>, float> LastHitTimes;
};
