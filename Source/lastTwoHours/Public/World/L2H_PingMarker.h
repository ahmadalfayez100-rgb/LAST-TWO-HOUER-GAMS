// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_PingMarker.generated.h"

class UL2H_StationMarker;

/**
 * A ping: a short-lived pole one player puts where they look, seen on both screens. It is how a player
 * on the floor keeps a part in the stage (marking an enemy or a route for the partner), and anyone may
 * use it. Spawned by the server, replicated, gone after the level config's PingLifetime.
 *
 * A ping on an enemy rides on it, so the mark stays on the thing that was meant.
 */
UCLASS(NotPlaceable, meta = (DisplayName = "L2H Ping Marker"))
class LASTTWOHOURS_API AL2H_PingMarker : public AActor
{
	GENERATED_BODY()

public:
	AL2H_PingMarker();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only, right after the spawn. */
	void Setup(bool bOnEnemy, const FLinearColor& InColor);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Ping")
	TObjectPtr<UL2H_StationMarker> Marker;

	UPROPERTY(ReplicatedUsing = OnRep_Look)
	bool bEnemy = false;

	UPROPERTY(ReplicatedUsing = OnRep_Look)
	FLinearColor Color = FLinearColor(1.0f, 0.85f, 0.1f);

	UFUNCTION()
	void OnRep_Look();
};
