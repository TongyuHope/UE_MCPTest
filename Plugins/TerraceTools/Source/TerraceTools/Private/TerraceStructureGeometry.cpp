#include "TerraceStructureGeometry.h"

#include "Algo/Reverse.h"
#include "TerraceGeometry.h"

#define LOCTEXT_NAMESPACE "TerraceStructureGeometry"

namespace
{
    double Cross2D(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    void RemoveCollinearVertices(TArray<FVector2D>& Polygon)
    {
        // Offset corners first, then resample paired edges. Offsetting already subdivided
        // straight edges can make inset corner vertices run past nearby sample vertices.
        bool bChanged = true;
        while (bChanged && Polygon.Num() > 3)
        {
            bChanged = false;
            for (int32 I = 0; I < Polygon.Num(); ++I)
            {
                const FVector2D A = Polygon[I] - Polygon[(I + Polygon.Num() - 1) % Polygon.Num()];
                const FVector2D B = Polygon[(I + 1) % Polygon.Num()] - Polygon[I];
                if (FVector2D::DotProduct(A, B) > 0.0 && FMath::Abs(Cross2D(A, B)) <= 0.01 * (A.Size() + B.Size()))
                {
                    Polygon.RemoveAt(I);
                    bChanged = true;
                    break;
                }
            }
        }
    }

    bool OffsetRing(const TArray<FVector2D>& Polygon, double Offset, TArray<FVector2D>& OutRing, FText& OutError)
    {
        OutRing.Reset();
        for (int32 I = 0; I < Polygon.Num(); ++I)
        {
            const FVector2D Previous = (Polygon[I] - Polygon[(I + Polygon.Num() - 1) % Polygon.Num()]).GetSafeNormal();
            const FVector2D Next = (Polygon[(I + 1) % Polygon.Num()] - Polygon[I]).GetSafeNormal();
            const FVector2D N0(Previous.Y, -Previous.X);
            const FVector2D N1(Next.Y, -Next.X);
            const FVector2D Miter = (N0 + N1).GetSafeNormal();
            const double Denominator = FVector2D::DotProduct(Miter, N0);
            if (Denominator < 0.25)
            {
                OutError = LOCTEXT("Sharp", "边界转角过尖，无法生成可靠田埂；请放缓转角或拆分田块。");
                return false;
            }
            OutRing.Add(Polygon[I] + Miter * (Offset / Denominator));
        }
        if (!TerraceGeometry::ValidatePolygon(OutRing, OutError) || TerraceGeometry::SignedArea(OutRing) <= 0.0)
        {
            OutError = LOCTEXT("Offset", "田埂/护坡偏移轮廓自交或收缩塌陷。请减小宽度、过渡宽度或修改窄部轮廓。");
            return false;
        }
        return true;
    }

    bool AddQuad(FTerraceStructureSection& Section, const FVector& A, const FVector& B, const FVector& C, const FVector& D,
        const FVector2D& UVA, const FVector2D& UVB, const FVector2D& UVC, const FVector2D& UVD)
    {
        const FVector E1 = B - A;
        const FVector E2 = C - A;
        const FVector Normal = (FVector::CrossProduct(E1, E2) + FVector::CrossProduct(E2, D - A)).GetSafeNormal();
        if (A.ContainsNaN() || B.ContainsNaN() || C.ContainsNaN() || D.ContainsNaN() || Normal.IsNearlyZero())
        {
            return false;
        }
        const FVector2D UV1 = UVB - UVA;
        const FVector2D UV2 = UVC - UVA;
        const double Det = Cross2D(UV1, UV2);
        FVector Tangent = D - A;
        FVector Bitangent = B - A;
        if (FMath::Abs(Det) > UE_SMALL_NUMBER)
        {
            Tangent = (E1 * UV2.Y - E2 * UV1.Y) / Det;
            Bitangent = (E2 * UV1.X - E1 * UV2.X) / Det;
        }
        Tangent = (Tangent - Normal * FVector::DotProduct(Tangent, Normal)).GetSafeNormal();
        const bool bFlipY = FVector::DotProduct(FVector::CrossProduct(Normal, Tangent), Bitangent) < 0.0;
        const int32 Start = Section.Vertices.Num();
        Section.Vertices.Append({ A, B, C, D });
        Section.UVs.Append({ UVA, UVB, UVC, UVD });
        // Unreal's front-face winding is opposite the mathematical CCW used by our XY rings.
        Section.Triangles.Append({ Start, Start + 2, Start + 1, Start, Start + 3, Start + 2 });
        for (int32 I = 0; I < 4; ++I)
        {
            Section.Normals.Add(Normal);
            Section.Tangents.Emplace(Tangent, bFlipY);
        }
        return true;
    }
}

bool TerraceStructureGeometry::BuildLayout(const FTerraceBoundarySnapshot& Plot, const FTerraceStructureSettings& Settings,
    FTerraceStructureLayout& OutLayout, FText& OutError)
{
    OutLayout = FTerraceStructureLayout();
    if (!Settings.Validate(OutError) || !Plot.Settings.Validate(OutError)
        || !TerraceGeometry::ValidatePolygon(Plot.Boundary, OutError))
    {
        return false;
    }
    if (!FMath::IsFinite(Plot.WorldElevation) || Plot.Settings.BundWidth < 1.0 || Plot.Settings.BundHeight < 1.0
        || (Settings.bGenerateBank && Plot.Settings.TransitionWidth < 10.0))
    {
        OutError = LOCTEXT("Dimensions", "结构要求有效标高、至少 1 cm 的田埂宽度和高度；启用护坡时过渡宽度至少 10 cm。");
        return false;
    }
    TArray<FVector2D> Boundary = Plot.Boundary;
    if (TerraceGeometry::SignedArea(Boundary) < 0.0)
    {
        Algo::Reverse(Boundary);
    }
    RemoveCollinearVertices(Boundary);
    const double BankCount = Settings.bGenerateBank
        ? FMath::CeilToDouble(Plot.Settings.TransitionWidth / Settings.StructureSampleSpacing) : 0.0;
    if (!FMath::IsFinite(BankCount) || BankCount > 16.0)
    {
        OutError = LOCTEXT("BankBudget", "护坡横向超过 16 段，请增大 Structure Sample Spacing 或减小过渡宽度。");
        return false;
    }
    FTerraceStructureLayout Layout;
    Layout.PlotSettings = Plot.Settings;
    Layout.RingOffsets = { -Plot.Settings.BundWidth, 0.0 };
    for (int32 I = 1; I <= static_cast<int32>(BankCount); ++I)
    {
        Layout.RingOffsets.Add(Plot.Settings.TransitionWidth * I / BankCount);
    }
    TArray<TArray<FVector2D>> BaseRings;
    for (double Offset : Layout.RingOffsets)
    {
        TArray<FVector2D> Ring;
        if (!OffsetRing(Boundary, Offset, Ring, OutError))
        {
            return false;
        }
        BaseRings.Add(MoveTemp(Ring));
    }
    for (int32 R = 0; R + 1 < BaseRings.Num(); ++R)
    {
        for (int32 I = 0; I < Boundary.Num(); ++I)
        {
            const int32 J = (I + 1) % Boundary.Num();
            const FVector2D A = BaseRings[R][I];
            const FVector2D B = BaseRings[R + 1][I];
            const FVector2D C = BaseRings[R + 1][J];
            const FVector2D D = BaseRings[R][J];
            if (Cross2D(B - A, C - A) <= 0.01 || Cross2D(C - A, D - A) <= 0.01
                || !TerraceGeometry::ContainsPoint(A, BaseRings[R + 1])
                || !TerraceGeometry::ContainsPoint((A + D) * 0.5, BaseRings[R + 1]))
            {
                OutError = LOCTEXT("Fold", "田埂/护坡宽度使窄部或转角翻折，请减小宽度或调整轮廓。");
                return false;
            }
        }
    }
    TArray<int32> Counts;
    int32 Total = 0;
    for (int32 I = 0; I < Boundary.Num(); ++I)
    {
        double MaxLength = 0.0;
        for (const auto& Ring : BaseRings)
        {
            MaxLength = FMath::Max(MaxLength, (Ring[(I + 1) % Boundary.Num()] - Ring[I]).Size());
        }
        const double Count = FMath::Max(1.0, FMath::CeilToDouble(MaxLength / Settings.StructureSampleSpacing));
        if (!FMath::IsFinite(Count) || Count > TerraceGeometry::MaxBoundaryVertices - Total)
        {
            OutError = LOCTEXT("OutlineBudget", "结构轮廓超过 512 个点，请增大 Structure Sample Spacing 或缩小田块。");
            return false;
        }
        Counts.Add(static_cast<int32>(Count));
        Total += static_cast<int32>(Count);
    }
    for (const auto& Ring : BaseRings)
    {
        TArray<FVector2D> Samples;
        for (int32 I = 0; I < Boundary.Num(); ++I)
        {
            for (int32 J = 0; J < Counts[I]; ++J)
            {
                Samples.Add(FMath::Lerp(Ring[I], Ring[(I + 1) % Boundary.Num()], static_cast<double>(J) / Counts[I]));
            }
        }
        Layout.Rings.Add(MoveTemp(Samples));
    }
    FBox2D Bounds(ForceInit);
    for (const auto& P : Boundary)
    {
        Bounds += P;
    }
    Layout.Origin = FVector(Bounds.GetCenter(), Plot.WorldElevation);
    Layout.PerimeterDistances.Add(0.0);
    for (int32 I = 0; I < Total; ++I)
    {
        Layout.PerimeterDistances.Add(Layout.PerimeterDistances.Last()
            + (Layout.Rings[1][(I + 1) % Total] - Layout.Rings[1][I]).Size());
    }
    OutLayout = MoveTemp(Layout);
    return true;
}

bool TerraceStructureGeometry::BuildMesh(const FTerraceStructureLayout& Layout, const FTerraceStructureSettings& Settings,
    const TArray<TArray<double>>& GroundHeights, FTerraceStructureMesh& OutMesh, FText& OutError)
{
    OutMesh = FTerraceStructureMesh();
    if (!Settings.Validate(OutError))
    {
        return false;
    }
    const int32 Rows = Layout.Rings.Num();
    const int32 N = Rows >= 2 ? Layout.Rings[0].Num() : 0;
    if (N < 3 || N > 512 || Rows > 18 || GroundHeights.Num() != Rows
        || Layout.RingOffsets.Num() != Rows || Layout.PerimeterDistances.Num() != N + 1)
    {
        OutError = LOCTEXT("Data", "结构布局或地形采样数量不一致。");
        return false;
    }
    TArray<TArray<FVector>> Top, Bottom;
    for (int32 R = 0; R < Rows; ++R)
    {
        if (GroundHeights[R].Num() != N || Layout.Rings[R].Num() != N)
        {
            OutError = LOCTEXT("Samples", "结构地形采样数量不一致。");
            return false;
        }
        TArray<FVector> Upper, Lower;
        for (int32 I = 0; I < N; ++I)
        {
            const double Ground = GroundHeights[R][I];
            const double T = R <= 1 ? 0.0 : Layout.RingOffsets[R] / Layout.RingOffsets.Last();
            const double Z = R <= 1 ? Layout.Origin.Z + Layout.PlotSettings.BundHeight + Settings.SurfaceLift
                : Ground + (Layout.PlotSettings.BundHeight + Settings.SurfaceLift) * (1.0 - T);
            if (!FMath::IsFinite(Ground) || !FMath::IsFinite(Z))
            {
                OutError = LOCTEXT("NonFiniteGround", "结构地形采样或埂顶高度不是有限数值。");
                return false;
            }
            if (R <= 1 && FMath::Abs(Ground - Layout.Origin.Z) > Settings.GroundHeightTolerance)
            {
                OutError = FText::FromString(FString::Printf(
                    TEXT("田埂%s采样点 %d（世界 X=%.1f, Y=%.1f cm）：碰撞高度 %.2f cm，目标田面 %.2f cm，差值 %+.2f cm，容差 %.2f cm。请先用当前版本更新地形并等待碰撞完成；若持续失败，检查工具层上方是否有其他地形编辑层覆盖，并提供此组数值。"),
                    R == 0 ? TEXT("内沿") : TEXT("外沿"), I,
                    Layout.Rings[R][I].X, Layout.Rings[R][I].Y, Ground, Layout.Origin.Z,
                    Ground - Layout.Origin.Z, Settings.GroundHeightTolerance));
                return false;
            }
            if (Z < Ground - 0.01)
            {
                OutError = FText::FromString(FString::Printf(
                    TEXT("地形盖住埂顶：采样点 %d（世界 X=%.1f, Y=%.1f cm），地形 %.2f cm，高于埂顶 %.2f cm。请检查地形编辑层和埂高。"),
                    I, Layout.Rings[R][I].X, Layout.Rings[R][I].Y, Ground, Z));
                return false;
            }
            Upper.Add(FVector(Layout.Rings[R][I], Z) - Layout.Origin);
            Lower.Add(FVector(Layout.Rings[R][I], Ground - Settings.EmbedDepth) - Layout.Origin);
        }
        Top.Add(MoveTemp(Upper));
        Bottom.Add(MoveTemp(Lower));
    }
    FTerraceStructureMesh Mesh;
    Mesh.Origin = Layout.Origin;
    bool bValid = true;
    for (int32 I = 0; I < N; ++I)
    {
        const int32 J = (I + 1) % N;
        const double U0 = Layout.PerimeterDistances[I] / Settings.UVTileSize;
        const double U1 = Layout.PerimeterDistances[I + 1] / Settings.UVTileSize;
        for (int32 R = 0; R + 1 < Rows; ++R)
        {
            const double V0 = (Layout.RingOffsets[R] + Layout.PlotSettings.BundWidth) / Settings.UVTileSize;
            const double V1 = (Layout.RingOffsets[R + 1] + Layout.PlotSettings.BundWidth) / Settings.UVTileSize;
            bValid &= AddQuad(Mesh.Sections[R == 0 ? 0 : 1], Top[R][I], Top[R + 1][I], Top[R + 1][J], Top[R][J],
                {U0, V0}, {U0, V1}, {U1, V1}, {U1, V0});
            bValid &= AddQuad(Mesh.Sections[2], Bottom[R][I], Bottom[R][J], Bottom[R + 1][J], Bottom[R + 1][I],
                {U0, V0}, {U1, V0}, {U1, V1}, {U0, V1});
        }
        bValid &= AddQuad(Mesh.Sections[2], Top[0][I], Top[0][J], Bottom[0][J], Bottom[0][I],
            {U0, Top[0][I].Z / Settings.UVTileSize}, {U1, Top[0][J].Z / Settings.UVTileSize},
            {U1, Bottom[0][J].Z / Settings.UVTileSize}, {U0, Bottom[0][I].Z / Settings.UVTileSize});
        const int32 Last = Rows - 1;
        bValid &= AddQuad(Mesh.Sections[2], Top[Last][I], Bottom[Last][I], Bottom[Last][J], Top[Last][J],
            {U0, Top[Last][I].Z / Settings.UVTileSize}, {U0, Bottom[Last][I].Z / Settings.UVTileSize},
            {U1, Bottom[Last][J].Z / Settings.UVTileSize}, {U1, Top[Last][J].Z / Settings.UVTileSize});
    }
    if (!bValid || Mesh.GetTriangleCount() > 65536)
    {
        OutError = LOCTEXT("Mesh", "结构产生退化面或超过 65,536 三角形，请修改轮廓或降低采样密度。");
        return false;
    }
    OutMesh = MoveTemp(Mesh);
    return true;
}

#undef LOCTEXT_NAMESPACE
