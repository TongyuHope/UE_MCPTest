#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraceTypes.h"
#include "TerraceTerrainOutputActor.generated.h"

class ALandscape;
class ATerraceGroupActor;
class ULandscapeTexturePatch;
class UTexture2D;

USTRUCT()
struct FTerraceAppliedTerrainPlot
{
    GENERATED_BODY()
    UPROPERTY() FGuid PlotId;
    UPROPERTY() TArray<FVector2D> Boundary;
    UPROPERTY() double Elevation = 0.0;
    UPROPERTY() double TransitionWidth = 0.0;
};

/** Editor-only output. Textures are persistent subobjects of this actor, not transient resources. */
UCLASS(NotPlaceable, NotBlueprintable)
class ATerraceTerrainOutputActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraceTerrainOutputActor();
    virtual bool IsEditorOnly() const override { return true; }
    virtual void Destroyed() override;

    UPROPERTY()
    TSoftObjectPtr<ATerraceGroupActor> SourceGroup;

    UPROPERTY()
    TSoftObjectPtr<ALandscape> AppliedLandscape;

    UPROPERTY()
    FGuid LayerGuid;

    UPROPERTY()
    TArray<TObjectPtr<ULandscapeTexturePatch>> Patches;

    UPROPERTY()
    TArray<TObjectPtr<UTexture2D>> MaskTextures;

    UPROPERTY() TArray<FTerraceAppliedTerrainPlot> AppliedPlots;
    UPROPERTY() double AppliedRasterSpacing = 0.0;
    /** Negative for older outputs that did not reserve a flat support band. */
    UPROPERTY() double AppliedSupportPadding = -1.0;
    UPROPERTY() FTransform AppliedLandscapeTransform;
    UPROPERTY() TArray<FTransform> AppliedPatchTransforms;

    void CaptureInputs(const TArray<FTerraceBoundarySnapshot>& Snapshots, double RasterSpacing, const FTransform& LandscapeTransform);
    bool MatchesInputs(const TArray<FTerraceBoundarySnapshot>& Snapshots, double RasterSpacing, const FTransform& LandscapeTransform) const;

    void SetContributionEnabled(bool bEnabled);
    bool HasEnabledContribution() const;
};
