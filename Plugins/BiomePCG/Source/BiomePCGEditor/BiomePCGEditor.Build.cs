using UnrealBuildTool;

public class BiomePCGEditor : ModuleRules
{
    public BiomePCGEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "GraphEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetRegistry",
                "BiomePCGRuntime",
                "BlueprintGraph",
                "InputCore",
                "Json",
                "Kismet",
                "PCG",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "EditorFramework"
            }
        );
    }
}
