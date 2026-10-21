#include "TerraceStructureTypes.h"

bool FTerraceStructureSettings::Validate(FText& OutError) const
{
    OutError = FText::GetEmpty();
    const double Values[] = { StructureSampleSpacing, EmbedDepth, SurfaceLift, GroundHeightTolerance, UVTileSize };
    for (double Value : Values)
    {
        if (!FMath::IsFinite(Value))
        {
            OutError = NSLOCTEXT("TerraceStructure", "Finite", "结构参数必须是有限数值。");
            return false;
        }
    }
    if (StructureSampleSpacing < 10.0 || EmbedDepth < 1.0 || SurfaceLift < 0.1
        || GroundHeightTolerance < 0.1 || GroundHeightTolerance > 20.0 || UVTileSize < 1.0)
    {
        OutError = NSLOCTEXT("TerraceStructure", "Range", "结构采样间距至少 10 cm，埋深至少 1 cm，表面抬高至少 0.1 cm，地面高度容差为 0.1～20 cm，UV 平铺尺寸至少 1 cm。");
        return false;
    }
    return true;
}

int32 FTerraceStructureMesh::GetTriangleCount() const
{
    return (Sections[0].Triangles.Num() + Sections[1].Triangles.Num() + Sections[2].Triangles.Num()) / 3;
}
