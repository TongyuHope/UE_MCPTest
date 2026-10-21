#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TerraceTypes.h"
#include "TerraceStructureTypes.generated.h"

USTRUCT(BlueprintType)
struct TERRACETOOLS_API FTerraceStructureSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools", meta=(ClampMin="10.0", Units="cm"))
    double StructureSampleSpacing = 50.0;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools")
    bool bGenerateBank = true;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools", meta=(ClampMin="1.0", Units="cm"))
    double EmbedDepth = 10.0;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.1", Units="cm"))
    double SurfaceLift = 2.0;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.1", ClampMax="20.0", Units="cm"))
    double GroundHeightTolerance = 10.0;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools", meta=(ClampMin="1.0", Units="cm"))
    double UVTileSize = 100.0;

    UPROPERTY(EditAnywhere, Category="梯田工具 / Terrace Tools")
    bool bCreateStructureCollision = true;

    bool Validate(FText& OutError) const;
};

struct TERRACETOOLS_API FTerraceStructureLayout
{
    FVector Origin = FVector::ZeroVector;
    /** World XY rings from inner bund edge to outer bank foot. */
    TArray<TArray<FVector2D>> Rings;
    TArray<double> RingOffsets;
    TArray<double> PerimeterDistances;
    FTerracePlotSettings PlotSettings;
};

struct TERRACETOOLS_API FTerraceStructureSection
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
};

struct TERRACETOOLS_API FTerraceStructureMesh
{
    /** 0: bund top; 1: outer bank; 2: inner wall, embedded foot and bottom. */
    FTerraceStructureSection Sections[3];
    FVector Origin = FVector::ZeroVector;
    int32 GetTriangleCount() const;
};
