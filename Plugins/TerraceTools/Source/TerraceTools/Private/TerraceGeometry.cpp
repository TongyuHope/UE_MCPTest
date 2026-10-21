#include "TerraceGeometry.h"

#define LOCTEXT_NAMESPACE "TerraceGeometry"

namespace
{
    double Cross(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    double SegmentDistanceSquared(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A;
        const double LengthSquared = AB.SizeSquared();
        const double T = LengthSquared > 0.0
            ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LengthSquared, 0.0, 1.0) : 0.0;
        return (P - (A + T * AB)).SizeSquared();
    }

    bool SegmentsTouch(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
    {
        const double ToleranceSquared = FMath::Square(TerraceGeometry::BoundaryTolerance);
        if (SegmentDistanceSquared(A, C, D) <= ToleranceSquared
            || SegmentDistanceSquared(B, C, D) <= ToleranceSquared
            || SegmentDistanceSquared(C, A, B) <= ToleranceSquared
            || SegmentDistanceSquared(D, A, B) <= ToleranceSquared)
        {
            return true;
        }
        const double C1 = Cross(B - A, C - A);
        const double C2 = Cross(B - A, D - A);
        const double C3 = Cross(D - C, A - C);
        const double C4 = Cross(D - C, B - C);
        return ((C1 > 0.0 && C2 < 0.0) || (C1 < 0.0 && C2 > 0.0))
            && ((C3 > 0.0 && C4 < 0.0) || (C3 < 0.0 && C4 > 0.0));
    }
}

double TerraceGeometry::SignedArea(const TArray<FVector2D>& Polygon)
{
    if (Polygon.Num() < 3)
    {
        return 0.0;
    }
    const FVector2D Origin = Polygon[0];
    double TwiceArea = 0.0;
    for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
    {
        TwiceArea += Cross(Polygon[I] - Origin, Polygon[I + 1] - Origin);
    }
    return TwiceArea * 0.5;
}

double TerraceGeometry::DistanceToBoundary(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
    if (Polygon.IsEmpty())
    {
        return TNumericLimits<double>::Max();
    }
    double MinSquared = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Polygon.Num(); ++I)
    {
        MinSquared = FMath::Min(MinSquared,
            SegmentDistanceSquared(Point, Polygon[I], Polygon[(I + 1) % Polygon.Num()]));
    }
    return FMath::Sqrt(MinSquared);
}

bool TerraceGeometry::ContainsPoint(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
    if (Polygon.Num() < 3 || !FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
    {
        return false;
    }
    if (DistanceToBoundary(Point, Polygon) <= BoundaryTolerance)
    {
        return true;
    }
    bool bInside = false;
    for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
    {
        const FVector2D A = Polygon[I] - Point;
        const FVector2D B = Polygon[J] - Point;
        if ((A.Y > 0.0) != (B.Y > 0.0)
            && 0.0 < (B.X - A.X) * (-A.Y) / (B.Y - A.Y) + A.X)
        {
            bInside = !bInside;
        }
    }
    return bInside;
}

bool TerraceGeometry::ValidatePolygon(const TArray<FVector2D>& Polygon, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (Polygon.Num() < 3 || Polygon.Num() > MaxBoundaryVertices)
    {
        OutError = LOCTEXT("Count", "A boundary must have between 3 and 512 sampled vertices.");
        return false;
    }
    for (const FVector2D& P : Polygon)
    {
        if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y))
        {
            OutError = LOCTEXT("Finite", "Boundary positions must contain finite numbers.");
            return false;
        }
    }
    const double ToleranceSquared = FMath::Square(BoundaryTolerance);
    for (int32 I = 0; I < Polygon.Num(); ++I)
    {
        const int32 Next = (I + 1) % Polygon.Num();
        const FVector2D& A = Polygon[I];
        const FVector2D& B = Polygon[Next];
        if ((B - A).SizeSquared() <= ToleranceSquared)
        {
            OutError = LOCTEXT("ShortEdge", "Boundary contains repeated vertices or edges shorter than 0.1 cm.");
            return false;
        }
        const FVector2D& C = Polygon[(I + 2) % Polygon.Num()];
        if (SegmentDistanceSquared(C, A, B) <= ToleranceSquared
            || SegmentDistanceSquared(A, B, C) <= ToleranceSquared)
        {
            OutError = LOCTEXT("Backtrack", "Adjacent boundary edges overlap or turn back on themselves.");
            return false;
        }
        for (int32 J = I + 1; J < Polygon.Num(); ++J)
        {
            const int32 JNext = (J + 1) % Polygon.Num();
            if (J == Next || JNext == I)
            {
                continue;
            }
            if (SegmentsTouch(A, B, Polygon[J], Polygon[JNext]))
            {
                OutError = LOCTEXT("Intersection", "Boundary intersects or touches itself. Use a simple closed outline.");
                return false;
            }
        }
    }
    const double Area = SignedArea(Polygon);
    if (!FMath::IsFinite(Area) || FMath::Abs(Area) < 1.0)
    {
        OutError = LOCTEXT("Area", "Boundary area must be at least 1 square cm.");
        return false;
    }
    return true;
}

double TerraceGeometry::PolygonDistance(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
{
    if (A.Num() < 3 || B.Num() < 3)
    {
        return 0.0;
    }
    if (ContainsPoint(A[0], B) || ContainsPoint(B[0], A))
    {
        return 0.0;
    }
    double MinSquared = TNumericLimits<double>::Max();
    for (int32 I = 0; I < A.Num(); ++I)
    {
        const FVector2D& A0 = A[I];
        const FVector2D& A1 = A[(I + 1) % A.Num()];
        for (int32 J = 0; J < B.Num(); ++J)
        {
            const FVector2D& B0 = B[J];
            const FVector2D& B1 = B[(J + 1) % B.Num()];
            if (SegmentsTouch(A0, A1, B0, B1))
            {
                return 0.0;
            }
            MinSquared = FMath::Min(MinSquared, SegmentDistanceSquared(A0, B0, B1));
            MinSquared = FMath::Min(MinSquared, SegmentDistanceSquared(A1, B0, B1));
            MinSquared = FMath::Min(MinSquared, SegmentDistanceSquared(B0, A0, A1));
            MinSquared = FMath::Min(MinSquared, SegmentDistanceSquared(B1, A0, A1));
        }
    }
    return FMath::Sqrt(MinSquared);
}

#undef LOCTEXT_NAMESPACE
