// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CoreFramework : ModuleRules
{
	public CoreFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",     // Layer / screen / slot IDs[cite: 1]
			"UMG",              // UUserWidget[cite: 1]
			"Slate",
			"SlateCore",
			"CommonUI",         // UCommonActivatableWidget, stacks, tab list[cite: 1]
			"CommonInput",      // Input type detection (mouse / keyboard / gamepad)[cite: 1]
			"DeveloperSettings" // Project Settings integration[cite: 1]
		});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
