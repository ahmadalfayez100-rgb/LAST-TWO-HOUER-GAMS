// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_TestCourse.generated.h"

/**
 * Testing aid: a small course built at runtime next to the first PlayerStart, so new features can be tried
 * without saving anything into the level. Spawned by the time director when the level config's
 * bSpawnTestCourse is on.
 *
 * Every machine builds the static parts itself (surface tiles with their labels, a tall climbable block, the
 * area volume); the server spawns the replicated ones (a door, pickups, a time shard, an enemy).
 */
UCLASS(NotPlaceable)
class LASTTWOHOURS_API AL2H_TestCourse : public AActor
{
	GENERATED_BODY()

public:
	AL2H_TestCourse();

protected:
	virtual void BeginPlay() override;

private:
	void BuildLocal();
	void SpawnReplicated();
	void AddLabel(const FVector& Location, const FString& Text, float Size = 30.0f);
};
