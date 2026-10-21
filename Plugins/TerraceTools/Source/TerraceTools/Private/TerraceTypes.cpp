#include "TerraceTypes.h"

#define LOCTEXT_NAMESPACE "TerraceTypes"

bool FTerracePlotSettings::Validate(FText& OutError) const
{
    OutError = FText::GetEmpty();
    const double Values[] = { ElevationOffset, SampleSpacing, BundWidth, BundHeight,
        TransitionWidth, PlantingYaw, RowSpacing, PlantSpacing, EdgeClearance };
    for (double Value : Values)
    {
        if (!FMath::IsFinite(Value))
        {
            OutError = LOCTEXT("NonFinite", "Parameters must contain finite numbers.");
            return false;
        }
    }
    if (SampleSpacing < 1.0 || RowSpacing < 1.0 || PlantSpacing < 1.0)
    {
        OutError = LOCTEXT("Spacing", "Boundary, row and plant spacing must be at least 1 cm.");
        return false;
    }
    if (BundWidth < 0.0 || BundHeight < 0.0 || TransitionWidth < 0.0 || EdgeClearance < 0.0)
    {
        OutError = LOCTEXT("Negative", "Widths, bund height and edge clearance cannot be negative.");
        return false;
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
