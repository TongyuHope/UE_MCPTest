#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraceTypes.h"
#include "TerracePlotActor.generated.h"

class USplineComponent;
class UTerracePreviewComponent;

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Terrace Plot"))
class TERRACETOOLS_API ATerracePlotActor : public AActor
{
    GENERATED_BODY()

public:
    ATerracePlotActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TObjectPtr<UTerracePreviewComponent> Preview;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TObjectPtr<USplineComponent> BoundarySpline;

    /** Derived CCW polyline at the terrace elevation. Edit BoundarySpline, then refresh this output. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TObjectPtr<USplineComponent> PCGBundSpline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools")
    bool bEnablePCGOutput = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    FTerracePCGPlotData PCGData;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    FText PCGSyncStatus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="梯田工具 / Terrace Tools", meta=(ShowOnlyInnerProperties))
    FTerracePlotSettings Settings;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    FGuid PlotId;

    UFUNCTION(BlueprintPure, Category="梯田工具 / Terrace Tools")
    double GetWorldElevation() const;

    /** Refreshes input geometry only; does not modify Landscape or generate any PCG graph. */
    UFUNCTION(BlueprintCallable, Category="梯田工具 / Terrace Tools")
    bool RefreshPCGOutput(FText& OutReport);
    /** Cheap sampling for editor preview; no polygon validity guarantee. */
    bool SampleBoundary(TArray<FVector2D>& OutBoundary, FText& OutError) const;
    /** On failure the output is reset rather than retaining old data. */
    bool BuildBoundarySnapshot(FTerraceBoundarySnapshot& OutSnapshot, FText& OutError) const;

    virtual void PostActorCreated() override;
    virtual void PostLoad() override;
    virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
    virtual void PostEditImport() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditMove(bool bFinished) override;
    virtual void PostEditUndo() override;
#endif

private:
    bool bRefreshingPCGOutput = false;
};
