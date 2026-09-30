// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FrameWorks : ModuleRules
{
	public FrameWorks(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"EnhancedInput",   // Character framework: UEnhancedInputComponent, UInputAction, UInputMappingContext
			"GameplayTags"     // StateMachine + Animation frameworks: FGameplayTag, native tags
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"FrameWorks",
			"FrameWorks/Variant_Horror",
			"FrameWorks/Variant_Horror/UI",
			"FrameWorks/Variant_Shooter",
			"FrameWorks/Variant_Shooter/AI",
			"FrameWorks/Variant_Shooter/UI",
			"FrameWorks/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
