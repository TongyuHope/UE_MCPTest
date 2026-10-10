using UnrealBuildTool;

public class WorldPartitionRVTBake : ModuleRules
{
    public WorldPartitionRVTBake(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "UnrealEd", "RenderCore", "Renderer",
            "RHI", "VirtualTexturingEditor", "PCG", "Landscape", "Json"
        });
    }
}
