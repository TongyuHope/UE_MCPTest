#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TerraceGeometry.h"
#include "TerraceTerrainRaster.h"

namespace
{
    FTerraceBoundarySnapshot MakePlot(double X)
    {
        FTerraceBoundarySnapshot Plot;
        Plot.PlotId = FGuid::NewGuid();
        Plot.WorldElevation = 1234.0;
        Plot.Boundary = { {X, 0}, {X + 1000, 0}, {X + 1000, 1000}, {X, 1000} };
        Plot.Settings.TransitionWidth = 200.0;
        return Plot;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerracePlotDistanceTest, "TerraceTools.Terrain.PlotSeparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerracePlotDistanceTest::RunTest(const FString& Parameters)
{
    const auto A = MakePlot(0.0);
    const auto Far = MakePlot(2000.0);
    const auto Touch = MakePlot(1000.0);
    const auto Overlap = MakePlot(500.0);
    TestEqual(TEXT("Separated boundaries"), TerraceGeometry::PolygonDistance(A.Boundary, Far.Boundary), 1000.0);
    TestEqual(TEXT("Touching"), TerraceGeometry::PolygonDistance(A.Boundary, Touch.Boundary), 0.0);
    TestEqual(TEXT("Overlap"), TerraceGeometry::PolygonDistance(A.Boundary, Overlap.Boundary), 0.0);
    TestEqual(TEXT("Containment"), TerraceGeometry::PolygonDistance(A.Boundary,
        { {100, 100}, {200, 100}, {200, 200}, {100, 200} }), 0.0);
    FText Error;
    TArray<FTerraceTerrainRaster> Rasters;
    TestTrue(TEXT("Separated influence regions"), TerraceTerrainRaster::Build({ A, Far }, 100.0, Rasters, Error));
    TestFalse(TEXT("Transition conflict rejected"), TerraceTerrainRaster::Build({ A, MakePlot(1500.0) }, 100.0, Rasters, Error));
    TestTrue(TEXT("Failure clears prior result"), Rasters.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceRasterTest, "TerraceTools.Terrain.MaskAndBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceRasterTest::RunTest(const FString& Parameters)
{
    const auto Plot = MakePlot(0.0);
    TArray<FTerraceTerrainRaster> Rasters;
    FText Error;
    if (!TestTrue(TEXT("Build"), TerraceTerrainRaster::Build({ Plot }, 100.0, Rasters, Error)))
    {
        return false;
    }
    const auto& Raster = Rasters[0];
    // [-300,1300] / 100 gives 17 sample centers including both ends.
    TestEqual(TEXT("Raster width"), Raster.Resolution.X, 17);
    TestEqual(TEXT("Height stored in transform"), Raster.Elevation, 1234.0);
    TestEqual(TEXT("Outside is transparent"), Raster.Pixels[0].A, static_cast<uint8>(0));
    TestEqual(TEXT("Interior fully covered"), Raster.Pixels[8 * 17 + 8].A, static_cast<uint8>(255));
    TestEqual(TEXT("Transition smoothstep midpoint"), Raster.Pixels[8 * 17 + 2].A, static_cast<uint8>(128));
    TestEqual(TEXT("Zero relative height"), Raster.Pixels[8 * 17 + 8].R, static_cast<uint8>(0));
    for (int32 X = 0; X < 17; ++X)
    {
        TestEqual(TEXT("Top border transparent"), Raster.Pixels[X].A, static_cast<uint8>(0));
        TestEqual(TEXT("Bottom border transparent"), Raster.Pixels[16 * 17 + X].A, static_cast<uint8>(0));
    }
    auto Huge = MakePlot(0.0);
    Huge.Boundary = { {0, 0}, {1000000, 0}, {1000000, 1000000}, {0, 1000000} };
    TestFalse(TEXT("Large allocation rejected"), TerraceTerrainRaster::Build({ Huge }, 10.0, Rasters, Error));
    TestTrue(TEXT("No stale raster"), Rasters.IsEmpty());
    TestFalse(TEXT("Spacing range"), TerraceTerrainRaster::Build({ Plot }, 0.0, Rasters, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceSupportPaddingTest, "TerraceTools.Terrain.FlatSupportPadding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceSupportPaddingTest::RunTest(const FString& Parameters)
{
    // An off-grid boundary exposes both mask filtering and Landscape triangle interpolation.
    auto Plot = MakePlot(23.0);
    for (auto& Point : Plot.Boundary) { Point.Y += 47.0; }
    const double Spacing = 100.0;
    const FTransform LandscapeTransform(FQuat::Identity, FVector::ZeroVector, FVector(100.0, 150.0, 100.0));
    const double Padding = TerraceTerrainRaster::CalculateSupportPadding(Spacing, LandscapeTransform);
    TestTrue(TEXT("Padding covers both sampling diagonals"), Padding > FMath::Sqrt(100.0 * 100.0 + 150.0 * 150.0)
        + FMath::Sqrt(2.0) * Spacing);
    TArray<FTerraceTerrainRaster> Old, Padded;
    FText Error;
    if (!TestTrue(TEXT("Original mask"), TerraceTerrainRaster::Build({ Plot }, Spacing, Old, Error))
        || !TestTrue(TEXT("Supported mask"), TerraceTerrainRaster::Build({ Plot }, Spacing, Padded, Error, Padding)))
    {
        return false;
    }
    auto SampleAlpha = [](const FTerraceTerrainRaster& Raster, const FVector2D& Point)
    {
        const FVector2D Pixel = (Point - Raster.Bounds.Min) / Raster.Bounds.GetSize()
            * FVector2D(Raster.Resolution.X - 1, Raster.Resolution.Y - 1);
        const int32 X = FMath::Clamp(FMath::FloorToInt(Pixel.X), 0, Raster.Resolution.X - 2);
        const int32 Y = FMath::Clamp(FMath::FloorToInt(Pixel.Y), 0, Raster.Resolution.Y - 2);
        const double TX = FMath::Clamp(Pixel.X - X, 0.0, 1.0);
        const double TY = FMath::Clamp(Pixel.Y - Y, 0.0, 1.0);
        auto Alpha = [&Raster](int32 PX, int32 PY) { return Raster.Pixels[PY * Raster.Resolution.X + PX].A / 255.0; };
        return FMath::Lerp(FMath::Lerp(Alpha(X, Y), Alpha(X + 1, Y), TX),
            FMath::Lerp(Alpha(X, Y + 1), Alpha(X + 1, Y + 1), TX), TY);
    };
    bool bOldMaskFailsSupport = false;
    for (int32 E = 0; E < Plot.Boundary.Num(); ++E)
    {
        for (int32 S = 0; S <= 20; ++S)
        {
            const FVector2D Point = FMath::Lerp(Plot.Boundary[E], Plot.Boundary[(E + 1) % Plot.Boundary.Num()], S / 20.0);
            const double GridX = FMath::FloorToDouble(Point.X / 100.0) * 100.0;
            const double GridY = FMath::FloorToDouble(Point.Y / 150.0) * 150.0;
            for (int32 DY = 0; DY <= 1; ++DY)
            {
                for (int32 DX = 0; DX <= 1; ++DX)
                {
                    const FVector2D Vertex(GridX + DX * 100.0, GridY + DY * 150.0);
                    const double Alpha = SampleAlpha(Padded[0], Vertex);
                    TestTrue(TEXT("All collision-cell corners fully flattened"), FMath::IsNearlyEqual(Alpha, 1.0, 1.e-9));
                    // Also covers raised and cut terraces with a large pre-existing height difference.
                    TestTrue(TEXT("Raised plateau support"), FMath::IsNearlyEqual(FMath::Lerp(-2000.0, Plot.WorldElevation, Alpha), Plot.WorldElevation, 0.01));
                    TestTrue(TEXT("Cut plateau support"), FMath::IsNearlyEqual(FMath::Lerp(5000.0, Plot.WorldElevation, Alpha), Plot.WorldElevation, 0.01));
                    bOldMaskFailsSupport |= SampleAlpha(Old[0], Vertex) < 0.99;
                }
            }
        }
    }
    TestTrue(TEXT("Reproduces original boundary interpolation failure"), bOldMaskFailsSupport);
    TestEqual(TEXT("Expanded mask border stays transparent"), Padded[0].Pixels[0].A, static_cast<uint8>(0));
    TestFalse(TEXT("Expanded neighboring influence regions cannot overlap"),
        TerraceTerrainRaster::Build({ Plot, MakePlot(2000.0) }, Spacing, Padded, Error, Padding));
    TestTrue(TEXT("Conflict clears output"), Padded.IsEmpty());
    TestFalse(TEXT("Invalid support padding"), TerraceTerrainRaster::Build({ Plot }, Spacing, Padded, Error, -1.0));
    return true;
}

#endif
