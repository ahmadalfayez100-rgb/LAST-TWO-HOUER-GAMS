// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "L2H_AreaVolume.generated.h"

class UBoxComponent;
class USoundBase;

/**
 * A named area. A player walking in sees its name across the top of their screen, and the level's ambient
 * music can change to the area's own track while a player on this machine stands inside. Place it, size the
 * box, type the name. Purely local: nothing replicates.
 */
UCLASS(meta = (DisplayName = "L2H Area Volume"))
class LASTTWOHOURS_API AL2H_AreaVolume : public AActor
{
	GENERATED_BODY()

public:
	AL2H_AreaVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Area")
	FText AreaName;

	/** Optional: plays instead of the level's ambient track while a local player is inside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Area")
	TObjectPtr<USoundBase> AmbientMusic;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L2H|Area")
	TObjectPtr<UBoxComponent> Box;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	int32 LocalPlayersInside = 0;
};
