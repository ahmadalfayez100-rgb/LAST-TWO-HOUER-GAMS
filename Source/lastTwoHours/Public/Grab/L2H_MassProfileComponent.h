// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "L2H_MassProfileComponent.generated.h"

/** Design-facing weight band, so a designer can reason about an object without knowing its kg. */
UENUM(BlueprintType)
enum class EL2H_CarryClass : uint8
{
	/** One hand, no speed penalty worth noticing. */
	Light	UMETA(DisplayName = "Light"),

	/** Carried solo but it slows the carrier down. */
	Medium	UMETA(DisplayName = "Medium"),

	/** Too heavy alone; needs the second player. */
	Heavy	UMETA(DisplayName = "Heavy"),

	/** Never moves. Set dressing that still blocks the hook. */
	Anchor	UMETA(DisplayName = "Anchor")
};

/**
 * What an object weighs, and how the hook and carry rules treat it.
 *
 * The component holds only per-object facts. The thresholds that decide whether a mass is liftable
 * live on the level's UL2H_LevelTimeConfig, so "what the hook can pull" is retuned per level without
 * touching a single prop.
 */
UCLASS(ClassGroup = (L2H), meta = (BlueprintSpawnableComponent, DisplayName = "L2H Mass Profile"))
class LASTTWOHOURS_API UL2H_MassProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UL2H_MassProfileComponent();

	/**
	 * Mass in kg. 0 means "ask the physics body", which is the right answer for a prop whose mesh
	 * already carries a sensible mass and the wrong answer for anything hand-tuned.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Mass", meta = (ClampMin = "0.0"))
	float Mass = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Mass")
	EL2H_CarryClass CarryClass = EL2H_CarryClass::Light;

	/**
	 * Whether this object may be grabbed at all. A locked door is this flag rather than a subclass,
	 * so the level can unlock it at runtime without anything being rebuilt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry")
	bool bCanBeGrabbed = true;

	/**
	 * Forces the two-player rule regardless of the resolved mass. Use for an object that is light
	 * enough to lift but is meant to need cooperation, such as a two-hand lever.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry")
	bool bRequiresTwoPlayers = false;

	/**
	 * Overrides the mass decision for the hook: the player is reeled in to this object even when it
	 * is light enough to pull. Use for a fixed anchor point rather than a loose prop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Hook")
	bool bHookReelIn = false;

	/**
	 * Socket on the carrier's mesh the object attaches to. None attaches to the mesh origin, which is
	 * predictable on a mannequin that has no hand socket authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "L2H|Carry")
	FName CarrySocket = NAME_None;

	/** Mass in kg: the authored value when set, otherwise the physics body's own. */
	UFUNCTION(BlueprintPure, Category = "L2H|Mass")
	float ResolveMass() const;

	/** This actor's profile, or null when it has none and is therefore not hookable by mass. */
	UFUNCTION(BlueprintPure, Category = "L2H|Mass")
	static UL2H_MassProfileComponent* FindOn(const AActor* Actor);
};
