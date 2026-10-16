// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class lastTwoHours : ModuleRules
{
	public lastTwoHours(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Deliberately bare. The ledge layer is DPTS now -- a Blueprint actor component on CMC --
		// so nothing here drives movement. Mover and MotionWarping were dependencies of the C++
		// ledge modes that used to live in Source/lastTwoHours/*/Movement, and both went with them.
		//
		// Add a dependency back only when this module actually needs one. Carrying an unused
		// experimental plugin in the build is how a project ends up bound to something it never
		// chose.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// EPhysicalSurface is declared in PhysicsCore, not Engine, and the contact-VFX layer names
			// it in a UPROPERTY key and in a UFUNCTION return. Engine depends on PhysicsCore publicly,
			// yet the generated Z_Construct_UEnum_PhysicsCore_EPhysicalSurface symbol never reached
			// this module's link line -- the same way GameplayTasks did not, further down.
			"PhysicsCore",
			// The couch co-op path resolves the second local player's platform user through
			// IPlatformInputDeviceMapper, which lives in ApplicationCore. Engine depends on it
			// publicly, but this module has already been bitten once by a transitive dependency
			// that never reached the link line, so it is named explicitly.
			"ApplicationCore",
			// UGameMapsSettings: with one gamepad the couch path hands it to player 2.
			"EngineSettings",
			// TimeCore. The time layer is GAS-backed: GameplayAbilities owns the ASC, the
			// attribute set and the gameplay abilities/effects, GameplayTags carries the
			// ability and state tags, EnhancedInput drives the new base character's C++
			// BindAction setup, and UMG builds the time clock HUD. These need a full
			// rebuild + editor restart -- Live Coding cannot pick up new modules.
			"GameplayAbilities",
			// GameplayTasks is a *public* dependency of GameplayAbilities, yet the transitive
			// import lib did not reach this module's link line. UAbilitySystemComponent
			// derives from UGameplayTasksComponent, so the generated vtable for
			// UL2H_AbilitySystemComponent references OnGameplayTaskActivated /
			// OnGameplayTaskDeactivated, which are defined in
			// Engine/Source/Runtime/GameplayTasks/Private/GameplayTasksComponent.cpp.
			// Without the explicit dependency the link dies with LNK2001 on those two symbols.
			"GameplayTasks",
			"GameplayTags",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"InputCore",
			// The hook rope is a looping Niagara beam owned by the pawn and updated from C++ every frame.
			"Niagara",
			// The test enemy is possessed by a plain AAIController so its movement input has a controller.
			"AIModule",
			"NavigationSystem",
		});
	}
}
