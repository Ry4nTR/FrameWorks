// MyGame.Build.cs
// PROJECT TEMPLATE - complete Build.cs of a NEW game module that uses the starter kit.
//
// Existing game module? Keep your own file and make sure PublicDependencyModuleNames contains every module
// marked REQUIRED below. Do not replace a Build.cs that already has other dependencies.
// The plugin must also be enabled in the .uproject ("Plugins": [ { "Name": "CoreFramework", "Enabled": true } ]),
// otherwise the build cannot find the CoreFramework module.

using UnrealBuildTool;

public class MyGame : ModuleRules
{
	public MyGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The folders Public/ and Private/ of the module are on the include path automatically.
		// Includes in the kit are relative to Public/, for example #include "Core/MyGameMode.h".

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",           // REQUIRED
			"CoreUObject",    // REQUIRED
			"Engine",         // REQUIRED
			"InputCore",      // REQUIRED
			"EnhancedInput",  // REQUIRED by AMyPlayerController: UInputAction, UInputMappingContext, UEnhancedInputComponent
			"GameplayTags",   // REQUIRED by AMyPlayerController: FGameplayTag
			"CoreFramework"   // REQUIRED by AMyPlayerController: UUIFrameSubsystem
			// PROJECT-SPECIFIC: add the modules your own game code needs (AIModule, UMG, ...).
		});
	}
}
