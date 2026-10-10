// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_MCPTest : ModuleRules
{
	public UE_MCPTest(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"UE_MCPTest",
			"UE_MCPTest/Variant_Platforming",
			"UE_MCPTest/Variant_Platforming/Animation",
			"UE_MCPTest/Variant_Combat",
			"UE_MCPTest/Variant_Combat/AI",
			"UE_MCPTest/Variant_Combat/Animation",
			"UE_MCPTest/Variant_Combat/Gameplay",
			"UE_MCPTest/Variant_Combat/Interfaces",
			"UE_MCPTest/Variant_Combat/UI",
			"UE_MCPTest/Variant_SideScrolling",
			"UE_MCPTest/Variant_SideScrolling/AI",
			"UE_MCPTest/Variant_SideScrolling/Gameplay",
			"UE_MCPTest/Variant_SideScrolling/Interfaces",
			"UE_MCPTest/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
