#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "TerraceGeometry.h"
#include "TerraceTypes.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerracePolygonTest, "TerraceTools.Geometry.PolygonValidity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerracePolygonTest::RunTest(const FString& Parameters)
{
    FText Error;
    const TArray<FVector2D> Square = { {0, 0}, {100, 0}, {100, 100}, {0, 100} };
    TestTrue(TEXT("Simple square"), TerraceGeometry::ValidatePolygon(Square, Error));
    TestEqual(TEXT("Area"), TerraceGeometry::SignedArea(Square), 10000.0);
    TArray<FVector2D> Reversed = Square;
    Algo::Reverse(Reversed);
    TestTrue(TEXT("Either winding is valid"), TerraceGeometry::ValidatePolygon(Reversed, Error));
    TestEqual(TEXT("Reversed area"), TerraceGeometry::SignedArea(Reversed), -10000.0);
    TestFalse(TEXT("Bow tie"), TerraceGeometry::ValidatePolygon({ {0, 0}, {100, 100}, {0, 100}, {100, 0} }, Error));
    TestFalse(TEXT("Collinear"), TerraceGeometry::ValidatePolygon({ {0, 0}, {100, 0}, {200, 0} }, Error));
    TestFalse(TEXT("Repeated vertex"), TerraceGeometry::ValidatePolygon({ {0, 0}, {100, 0}, {100, 0}, {0, 100} }, Error));
    TestFalse(TEXT("Adjacent overlap"), TerraceGeometry::ValidatePolygon(
        { {0, 0}, {100, 0}, {50, 0}, {100, 100}, {0, 100} }, Error));
    TestFalse(TEXT("Nonadjacent touching"), TerraceGeometry::ValidatePolygon(
        { {0, 0}, {100, 0}, {100, 100}, {50, 0}, {0, 100} }, Error));
    TestFalse(TEXT("Unclosed data repeats first vertex"), TerraceGeometry::ValidatePolygon(
        { {0, 0}, {100, 0}, {100, 100}, {0, 100}, {0, 0} }, Error));
    TestFalse(TEXT("Empty"), TerraceGeometry::ValidatePolygon({}, Error));
    TestFalse(TEXT("Nonfinite"), TerraceGeometry::ValidatePolygon(
        { {0, 0}, {100, 0}, {std::numeric_limits<double>::infinity(), 100} }, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceConcaveTest, "TerraceTools.Geometry.ConcaveAndDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceConcaveTest::RunTest(const FString& Parameters)
{
    const TArray<FVector2D> LShape = { {0, 0}, {300, 0}, {300, 100}, {100, 100}, {100, 300}, {0, 300} };
    FText Error;
    TestTrue(TEXT("Concave polygon supported"), TerraceGeometry::ValidatePolygon(LShape, Error));
    TestTrue(TEXT("Inside vertical arm"), TerraceGeometry::ContainsPoint({50, 200}, LShape));
    TestTrue(TEXT("Inside horizontal arm"), TerraceGeometry::ContainsPoint({200, 50}, LShape));
    TestFalse(TEXT("Concavity excluded"), TerraceGeometry::ContainsPoint({200, 200}, LShape));
    TestTrue(TEXT("Edge included"), TerraceGeometry::ContainsPoint({100, 200}, LShape));
    TestTrue(TEXT("Vertex included"), TerraceGeometry::ContainsPoint({100, 100}, LShape));
    TestEqual(TEXT("Interior clearance"), TerraceGeometry::DistanceToBoundary({50, 200}, LShape), 50.0);
    TestEqual(TEXT("Exterior edge distance"), TerraceGeometry::DistanceToBoundary({200, 200}, LShape), 100.0);
    TestFalse(TEXT("Empty contains nothing"), TerraceGeometry::ContainsPoint({0, 0}, {}));
    TestFalse(TEXT("NaN contains nothing"), TerraceGeometry::ContainsPoint({std::numeric_limits<double>::quiet_NaN(), 0}, LShape));
    TArray<FVector2D> Translated = LShape;
    for (FVector2D& P : Translated)
    {
        P += FVector2D(1000000000.0, -1000000000.0);
    }
    TestTrue(TEXT("Large world coordinates"), TerraceGeometry::ValidatePolygon(Translated, Error));
    TestEqual(TEXT("Translation preserves area"), TerraceGeometry::SignedArea(Translated), 50000.0);
    TestTrue(TEXT("Translation preserves inclusion"), TerraceGeometry::ContainsPoint({1000000050.0, -999999800.0}, Translated));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceSettingsTest, "TerraceTools.Input.SettingsAndBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceSettingsTest::RunTest(const FString& Parameters)
{
    FText Error;
    FTerracePlotSettings Settings;
    TestTrue(TEXT("Defaults valid"), Settings.Validate(Error));
    Settings.RowSpacing = 0;
    TestFalse(TEXT("Zero row spacing"), Settings.Validate(Error));
    Settings = FTerracePlotSettings();
    Settings.BundHeight = -1;
    TestFalse(TEXT("Negative height"), Settings.Validate(Error));
    Settings = FTerracePlotSettings();
    Settings.SampleSpacing = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("NaN spacing"), Settings.Validate(Error));
    TArray<FVector2D> Oversize;
    Oversize.Init(FVector2D::ZeroVector, TerraceGeometry::MaxBoundaryVertices + 1);
    TestFalse(TEXT("Vertex budget"), TerraceGeometry::ValidatePolygon(Oversize, Error));
    return true;
}

#endif
