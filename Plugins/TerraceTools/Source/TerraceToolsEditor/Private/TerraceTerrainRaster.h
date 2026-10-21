#pragma once

#include "CoreMinimal.h"
#include "TerraceTypes.h"

struct FTerraceTerrainRaster
{
    FGuid PlotId;
    FBox2D Bounds = FBox2D(ForceInit);
    FIntPoint Resolution = FIntPoint::ZeroValue;
    double Elevation = 0.0;
    /** Red is zero height relative to PatchZ; alpha is the polygon influence. */
    TArray<FColor> Pixels;
};

namespace TerraceTerrainRaster
{
    /** Keep both mask-filter taps and Landscape collision-triangle vertices on the flat plateau. */
    double CalculateSupportPadding(double Spacing, const FTransform& LandscapeTransform);

    bool Build(const TArray<FTerraceBoundarySnapshot>& Snapshots, double Spacing,
        TArray<FTerraceTerrainRaster>& OutRasters, FText& OutError, double SupportPadding = 0.0);
}
