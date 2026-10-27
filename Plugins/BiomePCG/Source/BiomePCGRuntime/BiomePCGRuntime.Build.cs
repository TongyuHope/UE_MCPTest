using UnrealBuildTool;

public class BiomePCGRuntime : ModuleRules
{
    public BiomePCGRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PCG"
        });

        // Blueprint libraries remain available to ordinary Actor blueprints.
        // Their baking/conversion implementation is compiled only for the editor.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AssetRegistry",
                "Foliage",
                "PhysicsCore",
                "RenderCore",
                "UnrealEd"
            });
        }
    }
}
