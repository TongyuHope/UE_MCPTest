#pragma once

#include "CoreMinimal.h"
#include "TerraceTypes.generated.h"

class UMaterialInterface;

/** Authoring parameters in cm. Elevation is relative to the plot actor's world Z. */
USTRUCT(BlueprintType)
struct TERRACETOOLS_API FTerracePlotSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(Units="cm"))
    double ElevationOffset = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="1.0", Units="cm"))
    double SampleSpacing = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.0", Units="cm"))
    double BundWidth = 60.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.0", Units="cm"))
    double BundHeight = 30.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools")
    TSoftObjectPtr<UMaterialInterface> BundMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools")
    TSoftObjectPtr<UMaterialInterface> BankMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.0", Units="cm"))
    double TransitionWidth = 150.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(Units="deg"))
    double PlantingYaw = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="1.0", Units="cm"))
    double RowSpacing = 35.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="1.0", Units="cm"))
    double PlantSpacing = 25.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ClampMin="0.0", Units="cm"))
    double EdgeClearance = 80.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools")
    int32 Seed = 1337;

    bool Validate(FText& OutError) const;
};

/** Flat, reflected values that Get Actor Property can extract without a PCG module dependency. */
USTRUCT(BlueprintType)
struct TERRACETOOLS_API FTerracePCGPlotData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    bool bValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    FString PlotId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    double WorldElevation = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    double BundWidth = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    double BundHeight = 0.0;

    /** Recommended inward offset for a mesh whose pivot is at the bund's width center. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    double BundCenterInset = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    int32 Seed = 0;
};

/** Shared input to terrain, structure and PCG backends. */
struct TERRACETOOLS_API FTerraceBoundarySnapshot
{
    FGuid PlotId;
    TArray<FVector2D> Boundary;
    FBox2D Bounds = FBox2D(ForceInit);
    double WorldElevation = 0.0;
    FTerracePlotSettings Settings;
};
