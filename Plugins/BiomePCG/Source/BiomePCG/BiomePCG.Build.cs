using UnrealBuildTool;

public class BiomePCG : ModuleRules
{
    public BiomePCG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "BiomePCGRuntime",
            "Blutility",
            "Foliage",
            "InputCore",
            "Landscape",
            "LevelEditor",
            "PhysicsCore",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "WorkspaceMenuStructure"
        });
    }
}
