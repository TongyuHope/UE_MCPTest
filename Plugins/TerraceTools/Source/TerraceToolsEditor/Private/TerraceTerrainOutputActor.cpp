#include "TerraceTerrainOutputActor.h"

#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "Landscape.h"
#include "LandscapeTexturePatch.h"
#include "TerraceGroupActor.h"
#include "TerraceStructureActor.h"
#include "TerraceTerrainRaster.h"

ATerraceTerrainOutputActor::ATerraceTerrainOutputActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ATerraceTerrainOutputActor::SetContributionEnabled(bool bEnabled)
{
    Modify();
    for (ULandscapeTexturePatch* Patch : Patches)
    {
        if (IsValid(Patch))
        {
            Patch->Modify();
            Patch->SetIsEnabled(bEnabled);
            Patch->RequestLandscapeUpdate(true);
        }
    }
    MarkPackageDirty();
}

bool ATerraceTerrainOutputActor::HasEnabledContribution() const
{
    for (const ULandscapeTexturePatch* Patch : Patches)
    {
        if (IsValid(Patch) && Patch->IsEnabled())
        {
            return true;
        }
    }
    return false;
}

void ATerraceTerrainOutputActor::Destroyed()
{
    // Request removal while the patch bindings are still valid, before actor teardown/GC.
    SetContributionEnabled(false);
    ATerraceGroupActor* Group = SourceGroup.Get();
    if (IsValid(Group) && Group->GeneratedTerrainOutput.ToSoftObjectPath() == FSoftObjectPath(this))
    {
        Group->Modify();
        ATerraceStructureActor* Structures = Group->GeneratedStructureOutput.Get();
        if (IsValid(Structures) && Structures->SourceGroup.Get() == Group && Structures->GetOwner() == Group)
        {
            Structures->Modify();
            Structures->Destroy();
        }
        Group->GeneratedTerrainOutput.Reset();
        Group->MarkPackageDirty();
    }
    Super::Destroyed();
}

void ATerraceTerrainOutputActor::CaptureInputs(const TArray<FTerraceBoundarySnapshot>& Snapshots,
    double RasterSpacing, const FTransform& LandscapeTransform)
{
    AppliedPlots.Reset();
    AppliedPatchTransforms.Reset();
    AppliedRasterSpacing = RasterSpacing;
    AppliedSupportPadding = TerraceTerrainRaster::CalculateSupportPadding(RasterSpacing, LandscapeTransform);
    AppliedLandscapeTransform = LandscapeTransform;
    for (const auto& Snapshot : Snapshots)
    {
        FTerraceAppliedTerrainPlot Plot;
        Plot.PlotId = Snapshot.PlotId;
        Plot.Boundary = Snapshot.Boundary;
        Plot.Elevation = Snapshot.WorldElevation;
        Plot.TransitionWidth = Snapshot.Settings.TransitionWidth;
        AppliedPlots.Add(MoveTemp(Plot));
    }
    for (const ULandscapeTexturePatch* Patch : Patches)
    {
        AppliedPatchTransforms.Add(Patch->GetComponentTransform());
    }
}

bool ATerraceTerrainOutputActor::MatchesInputs(const TArray<FTerraceBoundarySnapshot>& Snapshots,
    double RasterSpacing, const FTransform& LandscapeTransform) const
{
    if (AppliedPlots.Num() != Snapshots.Num() || Patches.Num() != AppliedPlots.Num()
        || AppliedPatchTransforms.Num() != Patches.Num()
        || !FMath::IsNearlyEqual(RasterSpacing, AppliedRasterSpacing, 0.01)
        || !FMath::IsNearlyEqual(AppliedSupportPadding,
            TerraceTerrainRaster::CalculateSupportPadding(RasterSpacing, LandscapeTransform), 0.01)
        || !LandscapeTransform.Equals(AppliedLandscapeTransform, 0.01)
        || !GetActorTransform().Equals(FTransform::Identity, 0.01))
    {
        return false;
    }
    for (int32 I = 0; I < Patches.Num(); ++I)
    {
        const auto* Patch = Patches[I].Get();
        if (!IsValid(Patch) || !Patch->IsEnabled() || Patch->GetLandscape() != AppliedLandscape.Get()
            || Patch->GetEditLayerGuid() != LayerGuid
            || !Patch->GetComponentTransform().Equals(AppliedPatchTransforms[I], 0.01))
        {
            return false;
        }
    }
    for (const auto& Snapshot : Snapshots)
    {
        const auto* Plot = AppliedPlots.FindByPredicate([&Snapshot](const auto& P) { return P.PlotId == Snapshot.PlotId; });
        if (!Plot || Plot->Boundary.Num() != Snapshot.Boundary.Num()
            || !FMath::IsNearlyEqual(Plot->Elevation, Snapshot.WorldElevation, 0.01)
            || !FMath::IsNearlyEqual(Plot->TransitionWidth, Snapshot.Settings.TransitionWidth, 0.01))
        {
            return false;
        }
        for (int32 I = 0; I < Plot->Boundary.Num(); ++I)
        {
            if (!Plot->Boundary[I].Equals(Snapshot.Boundary[I], 0.01))
            {
                return false;
            }
        }
    }
    return true;
}
