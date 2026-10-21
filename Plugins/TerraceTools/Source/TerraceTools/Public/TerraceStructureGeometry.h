#pragma once

#include "CoreMinimal.h"
#include "TerraceStructureTypes.h"

namespace TerraceStructureGeometry
{
    /** Creates matched offset rings; rejects collapsed widths and sharp/overlapping corners. */
    TERRACETOOLS_API bool BuildLayout(const FTerraceBoundarySnapshot& Plot, const FTerraceStructureSettings& Settings,
        FTerraceStructureLayout& OutLayout, FText& OutError);

    /** Ground heights are world Z, indexed by ring and then vertex. No world/UObject access. */
    TERRACETOOLS_API bool BuildMesh(const FTerraceStructureLayout& Layout, const FTerraceStructureSettings& Settings,
        const TArray<TArray<double>>& GroundHeights, FTerraceStructureMesh& OutMesh, FText& OutError);
}
