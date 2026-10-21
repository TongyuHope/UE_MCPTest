#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraceTypes.h"
#include "TerraceStructureTypes.h"
#include "TerraceGroupActor.generated.h"

class ALandscape;
class ATerracePlotActor;
class UTerracePreviewComponent;
class ATerraceStructureActor;

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Terrace Group"))
class TERRACETOOLS_API ATerraceGroupActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraceGroupActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TObjectPtr<UTerracePreviewComponent> Preview;

    /** Target for explicit editor terrain updates. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TSoftObjectPtr<ALandscape> TargetLandscape;

    /** Explicit membership. Unloaded members must not silently lose their outputs. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="梯田工具 / Terrace Tools")
    TArray<TSoftObjectPtr<ATerracePlotActor>> Plots;

    /** Mask sample spacing; does not change the Landscape vertex resolution. */
    UPROPERTY(EditInstanceOnly, Category="梯田工具 / Terrace Tools", meta=(ClampMin="10.0", Units="cm"))
    double TerrainRasterSpacing = 100.0;

    UPROPERTY(EditInstanceOnly, Category="梯田工具 / Terrace Tools", meta=(ShowOnlyInnerProperties))
    FTerraceStructureSettings StructureSettings;

    /** Runtime structure actor kept in cooked levels. */
    UPROPERTY(VisibleInstanceOnly, Category="梯田工具 / Terrace Tools")
    TSoftObjectPtr<ATerraceStructureActor> GeneratedStructureOutput;

#if WITH_EDITORONLY_DATA
    /** Owned editor output, excluded from cooked runtime data. */
    UPROPERTY(VisibleInstanceOnly, Category="梯田工具 / Terrace Tools")
    TSoftObjectPtr<AActor> GeneratedTerrainOutput;
#endif

    virtual void Destroyed() override;
    virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#if WITH_EDITOR
    virtual void PostEditImport() override;
#endif

    /** Atomic result: any invalid/unloaded member clears all snapshots. */
    bool BuildBoundarySnapshots(TArray<FTerraceBoundarySnapshot>& OutSnapshots, FText& OutReport) const;
};
