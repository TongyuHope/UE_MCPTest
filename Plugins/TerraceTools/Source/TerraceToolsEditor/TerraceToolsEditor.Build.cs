using UnrealBuildTool;

public class TerraceToolsEditor : ModuleRules
{
    public TerraceToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "TerraceTools", "UnrealEd",
            "PropertyEditor", "Slate", "SlateCore", "InputCore", "RenderCore",
            "Landscape", "LandscapePatch", "ProceduralMeshComponent"
        });
    }
}
