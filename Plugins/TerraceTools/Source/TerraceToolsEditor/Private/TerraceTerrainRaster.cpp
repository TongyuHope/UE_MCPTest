#include "TerraceTerrainRaster.h"

#include "TerraceGeometry.h"

#define LOCTEXT_NAMESPACE "TerraceTerrainRaster"

double TerraceTerrainRaster::CalculateSupportPadding(double Spacing, const FTransform& LandscapeTransform)
{
    const FVector Scale = LandscapeTransform.GetScale3D();
    // A query can lie anywhere within a Landscape triangle. Its vertices are at most
    // one grid diagonal away; each vertex's bilinear mask taps add one raster diagonal.
    return FVector2D(Scale.X, Scale.Y).Size() + FMath::Sqrt(2.0) * Spacing + 1.0;
}

bool TerraceTerrainRaster::Build(const TArray<FTerraceBoundarySnapshot>& Snapshots, double Spacing,
    TArray<FTerraceTerrainRaster>& OutRasters, FText& OutError, double SupportPadding)
{
    OutRasters.Reset();
    OutError = FText::GetEmpty();
    if (Snapshots.IsEmpty() || Snapshots.Num() > 16 || !FMath::IsFinite(Spacing) || Spacing < 10.0
        || !FMath::IsFinite(SupportPadding) || SupportPadding < 0.0)
    {
        OutError = LOCTEXT("Input", "地形小样支持 1～16 块田，Terrain Raster Spacing 至少为 10 cm，支撑余量必须是有效非负值。");
        return false;
    }
    double PairChecks = 0.0;
    for (int32 I = 0; I < Snapshots.Num(); ++I)
    {
        const auto& A = Snapshots[I];
        if (!A.Settings.Validate(OutError) || !TerraceGeometry::ValidatePolygon(A.Boundary, OutError)
            || !FMath::IsFinite(A.WorldElevation))
        {
            if (OutError.IsEmpty())
            {
                OutError = LOCTEXT("Height", "田面世界标高无效。");
            }
            return false;
        }
        for (int32 J = I + 1; J < Snapshots.Num(); ++J)
        {
            PairChecks += static_cast<double>(A.Boundary.Num()) * Snapshots[J].Boundary.Num();
        }
    }
    if (PairChecks > 4000000.0)
    {
        OutError = LOCTEXT("PairBudget", "田块关系检查超出预算，请减少田块或简化边界。");
        return false;
    }
    for (int32 I = 0; I < Snapshots.Num(); ++I)
    {
        for (int32 J = I + 1; J < Snapshots.Num(); ++J)
        {
            const double RequiredGap = Snapshots[I].Settings.TransitionWidth
                + Snapshots[J].Settings.TransitionWidth + 2.0 * (Spacing + SupportPadding);
            if (TerraceGeometry::PolygonDistance(Snapshots[I].Boundary, Snapshots[J].Boundary) <= RequiredGap)
            {
                OutError = FText::Format(LOCTEXT("Conflict", "田块 {0} 与 {1} 相交或过渡带太近。需保留大于 {2} cm 的边界间距；首版不混合重叠过渡带。"),
                    I, J, FText::AsNumber(RequiredGap));
                return false;
            }
        }
    }

    TArray<FTerraceTerrainRaster> Rasters;
    int64 TotalPixels = 0;
    double Evaluations = 0.0;
    // Preflight every allocation before rasterizing any plot.
    for (const auto& Snapshot : Snapshots)
    {
        FTerraceTerrainRaster Raster;
        Raster.PlotId = Snapshot.PlotId;
        Raster.Elevation = Snapshot.WorldElevation;
        FBox2D Bounds(ForceInit);
        for (const FVector2D& Point : Snapshot.Boundary)
        {
            Bounds += Point;
        }
        const FVector2D Margin(SupportPadding + Snapshot.Settings.TransitionWidth + Spacing);
        Raster.Bounds = FBox2D(Bounds.Min - Margin, Bounds.Max + Margin);
        const FVector2D Size = Raster.Bounds.GetSize();
        const double Width = FMath::CeilToDouble(Size.X / Spacing) + 1.0;
        const double Height = FMath::CeilToDouble(Size.Y / Spacing) + 1.0;
        if (!FMath::IsFinite(Width) || !FMath::IsFinite(Height) || Width < 2.0 || Height < 2.0
            || Width > 513.0 || Height > 513.0)
        {
            OutError = LOCTEXT("Size", "单块遮罩超过 513×513，请增大 Terrain Raster Spacing 或缩小田块范围。");
            return false;
        }
        Raster.Resolution = FIntPoint(static_cast<int32>(Width), static_cast<int32>(Height));
        TotalPixels += static_cast<int64>(Raster.Resolution.X) * Raster.Resolution.Y;
        Evaluations += 3.0 * Raster.Resolution.X * Raster.Resolution.Y * Snapshot.Boundary.Num();
        if (TotalPixels > 1048576 || Evaluations > 64000000.0)
        {
            OutError = LOCTEXT("Budget", "整组遮罩超过内存/计算预算，请降低采样密度或减少田块。");
            return false;
        }
        Rasters.Add(MoveTemp(Raster));
    }
    for (int32 I = 0; I < Rasters.Num(); ++I)
    {
        auto& Raster = Rasters[I];
        const auto& Snapshot = Snapshots[I];
        Raster.Pixels.SetNumUninitialized(Raster.Resolution.X * Raster.Resolution.Y);
        const FVector2D Step = Raster.Bounds.GetSize() / FVector2D(Raster.Resolution.X - 1, Raster.Resolution.Y - 1);
        for (int32 Y = 0; Y < Raster.Resolution.Y; ++Y)
        {
            for (int32 X = 0; X < Raster.Resolution.X; ++X)
            {
                const FVector2D P = Raster.Bounds.Min + FVector2D(X * Step.X, Y * Step.Y);
                double Alpha = 0.0;
                if (TerraceGeometry::ContainsPoint(P, Snapshot.Boundary))
                {
                    Alpha = 1.0;
                }
                else
                {
                    const double Distance = TerraceGeometry::DistanceToBoundary(P, Snapshot.Boundary);
                    if (Distance <= SupportPadding)
                    {
                        Alpha = 1.0;
                    }
                    else if (Snapshot.Settings.TransitionWidth > 0.0)
                    {
                        const double T = FMath::Clamp(1.0 - (Distance - SupportPadding)
                            / Snapshot.Settings.TransitionWidth, 0.0, 1.0);
                        Alpha = T * T * (3.0 - 2.0 * T);
                    }
                }
                Raster.Pixels[Y * Raster.Resolution.X + X] = FColor(0, 0, 0,
                    static_cast<uint8>(FMath::RoundToInt(Alpha * 255.0)));
            }
        }
    }
    OutRasters = MoveTemp(Rasters);
    return true;
}

#undef LOCTEXT_NAMESPACE
