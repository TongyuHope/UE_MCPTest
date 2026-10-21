#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "TerraceGeometry.h"
#include "TerraceStructureGeometry.h"

namespace
{
    FTerraceBoundarySnapshot MakeStructurePlot()
    {
        FTerraceBoundarySnapshot Plot;
        Plot.PlotId = FGuid::NewGuid();
        Plot.WorldElevation = 1000.0;
        Plot.Boundary = { {0, 0}, {2000, 0}, {2000, 1200}, {0, 1200} };
        return Plot;
    }

    TArray<TArray<double>> MakeGround(const FTerraceStructureLayout& Layout, bool bSlope = false)
    {
        TArray<TArray<double>> Ground;
        for (int32 R = 0; R < Layout.Rings.Num(); ++R)
        {
            TArray<double> Row;
            Row.Init(Layout.Origin.Z - (bSlope ? FMath::Max(0.0, Layout.RingOffsets[R]) * 0.5 : 0.0), Layout.Rings[R].Num());
            Ground.Add(MoveTemp(Row));
        }
        return Ground;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceStructureOffsetTest, "TerraceTools.Structures.OffsetAndWinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceStructureOffsetTest::RunTest(const FString& Parameters)
{
    FTerraceBoundarySnapshot Plot = MakeStructurePlot();
    FTerraceStructureSettings Settings;
    FTerraceStructureLayout Layout;
    FText Error;
    if (!TestTrue(TEXT("Rectangle layout"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Layout, Error)))
    {
        return false;
    }
    TestEqual(TEXT("Inner, boundary and three bank rings"), Layout.Rings.Num(), 5);
    TestEqual(TEXT("Inner width offset"), Layout.RingOffsets[0], -60.0);
    TestEqual(TEXT("Bank foot offset"), Layout.RingOffsets.Last(), 150.0);
    TestEqual(TEXT("Local mesh datum"), Layout.Origin.Z, 1000.0);
    for (const auto& Ring : Layout.Rings)
    {
        TestEqual(TEXT("Matched ring count"), Ring.Num(), Layout.Rings[0].Num());
        TestTrue(TEXT("CCW ring"), TerraceGeometry::SignedArea(Ring) > 0.0);
        TestTrue(TEXT("Valid sampled ring"), TerraceGeometry::ValidatePolygon(Ring, Error));
    }
    Algo::Reverse(Plot.Boundary);
    FTerraceStructureLayout Reversed;
    TestTrue(TEXT("Clockwise input supported"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Reversed, Error));
    TestEqual(TEXT("Equivalent sample count"), Reversed.Rings[0].Num(), Layout.Rings[0].Num());

    Plot = MakeStructurePlot();
    const auto Corners = Plot.Boundary;
    Plot.Boundary.Reset();
    for (int32 I = 0; I < Corners.Num(); ++I)
    {
        const int32 Count = FMath::RoundToInt((Corners[(I + 1) % Corners.Num()] - Corners[I]).Size() / 100.0);
        for (int32 J = 0; J < Count; ++J)
        {
            Plot.Boundary.Add(FMath::Lerp(Corners[I], Corners[(I + 1) % Corners.Num()], static_cast<double>(J) / Count));
        }
    }
    FTerraceStructureLayout Subdivided;
    TestTrue(TEXT("Collinear samples do not fold inset corners"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Subdivided, Error));
    TestEqual(TEXT("Subdivision-independent count"), Subdivided.Rings[0].Num(), Layout.Rings[0].Num());
    Plot = MakeStructurePlot();
    Plot.Boundary = { {0, 0}, {2000, 0}, {2000, 800}, {800, 800}, {800, 2000}, {0, 2000} };
    TestTrue(TEXT("Wide concave L supported"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Layout, Error));
    Plot.Boundary = { {0, 0}, {100, 0}, {100, 1000}, {0, 1000} };
    TestFalse(TEXT("Inset collapse rejected"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Layout, Error));
    TestTrue(TEXT("Failure clears layout"), Layout.Rings.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceStructureMeshTest, "TerraceTools.Structures.MeshClosureAndNormals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceStructureMeshTest::RunTest(const FString& Parameters)
{
    FTerraceStructureLayout Layout;
    FTerraceStructureSettings Settings;
    FTerraceStructureMesh Mesh;
    FText Error;
    if (!TestTrue(TEXT("Layout"), TerraceStructureGeometry::BuildLayout(MakeStructurePlot(), Settings, Layout, Error))
        || !TestTrue(TEXT("Mesh"), TerraceStructureGeometry::BuildMesh(Layout, Settings, MakeGround(Layout), Mesh, Error)))
    {
        return false;
    }
    TestEqual(TEXT("Closed ring triangle count"), Mesh.GetTriangleCount(), 4 * Layout.Rings[0].Num() * Layout.Rings.Num());
    TMap<FString, int32> EdgeCounts;
    for (const auto& Section : Mesh.Sections)
    {
        TestEqual(TEXT("Normals count"), Section.Normals.Num(), Section.Vertices.Num());
        TestEqual(TEXT("UV count"), Section.UVs.Num(), Section.Vertices.Num());
        TestEqual(TEXT("Tangent count"), Section.Tangents.Num(), Section.Vertices.Num());
        for (int32 T = 0; T < Section.Triangles.Num(); T += 3)
        {
            const int32 A = Section.Triangles[T], B = Section.Triangles[T + 1], C = Section.Triangles[T + 2];
            TestTrue(TEXT("Indices valid"), Section.Vertices.IsValidIndex(A) && Section.Vertices.IsValidIndex(B) && Section.Vertices.IsValidIndex(C));
            const FVector N = FVector::CrossProduct(Section.Vertices[B] - Section.Vertices[C], Section.Vertices[A] - Section.Vertices[C]).GetSafeNormal();
            TestTrue(TEXT("Unreal winding agrees with normals"), FVector::DotProduct(N, Section.Normals[A]) > 0.99);
            for (int32 E = 0; E < 3; ++E)
            {
                const FVector P0 = Section.Vertices[Section.Triangles[T + E]];
                const FVector P1 = Section.Vertices[Section.Triangles[T + (E + 1) % 3]];
                FString K0 = FString::Printf(TEXT("%.3f,%.3f,%.3f"), P0.X, P0.Y, P0.Z);
                FString K1 = FString::Printf(TEXT("%.3f,%.3f,%.3f"), P1.X, P1.Y, P1.Z);
                if (K0 > K1) { Swap(K0, K1); }
                EdgeCounts.FindOrAdd(K0 + TEXT("/") + K1)++;
            }
        }
    }
    for (const auto& Edge : EdgeCounts)
    {
        TestEqual(TEXT("Each geometric edge has two faces"), Edge.Value, 2);
    }
    for (int32 I = 0; I < Mesh.Sections[0].Vertices.Num(); ++I)
    {
        TestEqual(TEXT("Bund top local height"), Mesh.Sections[0].Vertices[I].Z, 32.0);
        TestTrue(TEXT("Top normal up"), Mesh.Sections[0].Normals[I].Z > 0.99);
    }
    TestTrue(TEXT("Variable terrain slope"), TerraceStructureGeometry::BuildMesh(Layout, Settings, MakeGround(Layout, true), Mesh, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerraceStructureInputTest, "TerraceTools.Structures.InputAndGroundGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerraceStructureInputTest::RunTest(const FString& Parameters)
{
    FTerraceStructureSettings Settings;
    FTerraceStructureLayout Layout;
    FTerraceStructureMesh Mesh;
    FText Error;
    Settings.bGenerateBank = false;
    if (!TestTrue(TEXT("Bund-only layout"), TerraceStructureGeometry::BuildLayout(MakeStructurePlot(), Settings, Layout, Error)))
    {
        return false;
    }
    TestEqual(TEXT("Bund-only rings"), Layout.Rings.Num(), 2);
    TestTrue(TEXT("Bund-only mesh"), TerraceStructureGeometry::BuildMesh(Layout, Settings, MakeGround(Layout), Mesh, Error));
    auto Ground = MakeGround(Layout);
    Ground[0][0] += 100.0;
    TestFalse(TEXT("Outdated collision rejected"), TerraceStructureGeometry::BuildMesh(Layout, Settings, Ground, Mesh, Error));
    TestTrue(TEXT("Report identifies inner edge"), Error.ToString().Contains(TEXT("内沿")));
    TestTrue(TEXT("Report includes actual and target heights"), Error.ToString().Contains(TEXT("1100.00")) && Error.ToString().Contains(TEXT("1000.00")));
    TestEqual(TEXT("No stale mesh"), Mesh.GetTriangleCount(), 0);
    Ground = MakeGround(Layout);
    Ground[1][0] -= 100.0;
    TestFalse(TEXT("Outer edge support also checked"), TerraceStructureGeometry::BuildMesh(Layout, Settings, Ground, Mesh, Error));
    TestTrue(TEXT("Report identifies outer edge"), Error.ToString().Contains(TEXT("外沿")));
    Ground = MakeGround(Layout);
    Ground[0][0] += 5.0;
    Settings.GroundHeightTolerance = 20.0;
    Layout.PlotSettings.BundHeight = 1.0;
    TestFalse(TEXT("Buried top rejected independently of support tolerance"),
        TerraceStructureGeometry::BuildMesh(Layout, Settings, Ground, Mesh, Error));
    TestTrue(TEXT("Buried top has its own report"), Error.ToString().Contains(TEXT("地形盖住埂顶")));
    TestFalse(TEXT("Missing ground samples"), TerraceStructureGeometry::BuildMesh(Layout, Settings, {}, Mesh, Error));
    auto Plot = MakeStructurePlot();
    Plot.Settings.BundWidth = 0.0;
    TestFalse(TEXT("Zero width rejected"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Layout, Error));
    Plot = MakeStructurePlot();
    Settings = FTerraceStructureSettings();
    Plot.Settings.TransitionWidth = 100000.0;
    TestFalse(TEXT("Bank row budget"), TerraceStructureGeometry::BuildLayout(Plot, Settings, Layout, Error));
    Settings = FTerraceStructureSettings();
    Settings.StructureSampleSpacing = 1.0;
    TestFalse(TEXT("Minimum spacing"), Settings.Validate(Error));
    return true;
}

#endif
