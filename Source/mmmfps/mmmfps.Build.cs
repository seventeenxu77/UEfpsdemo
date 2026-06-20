// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class mmmfps : ModuleRules
{
	public mmmfps(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"Niagara"        // 开火特效(枪口火光/命中粒子)用 Niagara
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"mmmfps",
			"mmmfps/Variant_Horror",
			"mmmfps/Variant_Horror/UI",
			"mmmfps/Variant_Shooter",
			"mmmfps/Variant_Shooter/AI",
			"mmmfps/Variant_Shooter/UI",
			"mmmfps/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
