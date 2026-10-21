#pragma once

#include "CoreMinimal.h"

/** XY geometry in cm. Polygons omit a repeated closing vertex and may be concave. */
namespace TerraceGeometry
{
    inline constexpr int32 MaxBoundaryVertices = 512;
    inline constexpr double BoundaryTolerance = 0.1;

    TERRACETOOLS_API double SignedArea(const TArray<FVector2D>& Polygon);
    TERRACETOOLS_API double DistanceToBoundary(const FVector2D& Point, const TArray<FVector2D>& Polygon);
    TERRACETOOLS_API bool ContainsPoint(const FVector2D& Point, const TArray<FVector2D>& Polygon);
    TERRACETOOLS_API bool ValidatePolygon(const TArray<FVector2D>& Polygon, FText& OutError);
    /** Zero for overlap/touching/containment. Inputs must already be validated. */
    TERRACETOOLS_API double PolygonDistance(const TArray<FVector2D>& A, const TArray<FVector2D>& B);
}
