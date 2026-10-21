#include "TerraceGroupActor.h"

#include "Landscape.h"
#include "Engine/World.h"
#include "TerracePlotActor.h"
#include "TerracePreviewComponent.h"
#include "TerraceStructureActor.h"

#define LOCTEXT_NAMESPACE "TerraceGroup"

ATerraceGroupActor::ATerraceGroupActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Preview = CreateDefaultSubobject<UTerracePreviewComponent>(TEXT("TerracePreview"));
    SetRootComponent(Preview);
}

bool ATerraceGroupActor::BuildBoundarySnapshots(TArray<FTerraceBoundarySnapshot>& OutSnapshots, FText& OutReport) const
{
    OutSnapshots.Reset();
    if (Plots.IsEmpty())
    {
        OutReport = LOCTEXT("Empty", "Group has no plots. Add a plot first.");
        return false;
    }
    TSet<FGuid> SeenIds;
    TSet<const ATerracePlotActor*> SeenActors;
    TArray<FTerraceBoundarySnapshot> Snapshots;
    for (int32 I = 0; I < Plots.Num(); ++I)
    {
        const ATerracePlotActor* Plot = Plots[I].Get();
        if (!IsValid(Plot) || Plot->GetWorld() != GetWorld())
        {
            OutReport = FText::Format(LOCTEXT("Missing", "Plot entry {0} is missing, unloaded, or in another world. Load it or repair membership."), I);
            return false;
        }
        if (SeenActors.Contains(Plot) || SeenIds.Contains(Plot->PlotId))
        {
            OutReport = FText::Format(LOCTEXT("Duplicate", "Plot entry {0} duplicates an actor or Plot ID."), I);
            return false;
        }
        FTerraceBoundarySnapshot Snapshot;
        FText Error;
        if (!Plot->BuildBoundarySnapshot(Snapshot, Error))
        {
            OutReport = FText::Format(LOCTEXT("Invalid", "Plot entry {0}: {1}"), I, Error);
            return false;
        }
        SeenActors.Add(Plot);
        SeenIds.Add(Snapshot.PlotId);
        Snapshots.Add(MoveTemp(Snapshot));
    }
    OutSnapshots = MoveTemp(Snapshots);
    OutReport = FText::Format(LOCTEXT("Valid", "{0} 块田的边界有效。此按钮不检查地形覆盖、过渡带冲突或碰撞；更新地形时会进一步预检。"), OutSnapshots.Num());
    return true;
}

#undef LOCTEXT_NAMESPACE

void ATerraceGroupActor::Destroyed()
{
#if WITH_EDITOR
    if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
    {
        AActor* Output = GeneratedTerrainOutput.Get();
        if (IsValid(Output) && Output->GetOwner() == this)
        {
            Output->Modify();
            Output->Destroy();
        }
        ATerraceStructureActor* Structures = GeneratedStructureOutput.Get();
        if (IsValid(Structures) && Structures->SourceGroup.Get() == this && Structures->GetOwner() == this)
        {
            Structures->Modify();
            Structures->Destroy();
        }
    }
#endif
    Super::Destroyed();
}

void ATerraceGroupActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
    Super::PostDuplicate(DuplicateMode);
#if WITH_EDITORONLY_DATA
    if (DuplicateMode != EDuplicateMode::PIE && !IsTemplate())
    {
        GeneratedTerrainOutput.Reset();
        GeneratedStructureOutput.Reset();
    }
#endif
}

#if WITH_EDITOR
void ATerraceGroupActor::PostEditImport()
{
    Super::PostEditImport();
    GeneratedTerrainOutput.Reset();
    GeneratedStructureOutput.Reset();
}
#endif
