#include "TerracePlotActor.h"

#include "Components/SplineComponent.h"
#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "TerraceGeometry.h"
#include "TerracePreviewComponent.h"

#define LOCTEXT_NAMESPACE "TerracePlot"

namespace
{
    const FName PlotTag(TEXT("TerracePlot"));
    const FName ReadyTag(TEXT("TerracePCGReady"));
    const FName BoundaryTag(TEXT("TerraceBoundary"));
    const FName OutputTag(TEXT("TerracePCGBundSpline"));
}

ATerracePlotActor::ATerracePlotActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(PlotTag);
    Preview = CreateDefaultSubobject<UTerracePreviewComponent>(TEXT("TerracePreview"));
    SetRootComponent(Preview);
    BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
    BoundarySpline->SetupAttachment(Preview);
    BoundarySpline->ClearSplinePoints(false);
    const FVector Points[] = { FVector(-1000, -600, 0), FVector(1000, -600, 0),
        FVector(1000, 600, 0), FVector(-1000, 600, 0) };
    for (const FVector& Point : Points)
    {
        BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
    }
    for (int32 I = 0; I < 4; ++I)
    {
        BoundarySpline->SetSplinePointType(I, ESplinePointType::Linear, false);
    }
    BoundarySpline->SetClosedLoop(true, true);
    BoundarySpline->ComponentTags.AddUnique(BoundaryTag);

    PCGBundSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PCGBundSpline"));
    PCGBundSpline->SetupAttachment(Preview);
    // The source world points already include actor scaling. Keep PCG output point scale at 1.
    PCGBundSpline->SetAbsolute(false, false, true);
    PCGBundSpline->ComponentTags.AddUnique(OutputTag);
    PCGBundSpline->ClearSplinePoints(true);
    PCGBundSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PCGBundSpline->SetCanEverAffectNavigation(false);
    PCGBundSpline->PrimaryComponentTick.bCanEverTick = false;
    PCGBundSpline->bDrawDebug = false;
}

double ATerracePlotActor::GetWorldElevation() const
{
    return GetActorLocation().Z + Settings.ElevationOffset;
}

bool ATerracePlotActor::RefreshPCGOutput(FText& OutReport)
{
    if (bRefreshingPCGOutput || IsTemplate())
    {
        OutReport = LOCTEXT("PCGReentry", "PCG 输入同步正在执行，或当前对象是类模板。");
        return false;
    }
    TGuardValue<bool> Guard(bRefreshingPCGOutput, true);
    Tags.AddUnique(PlotTag);
    Tags.Remove(ReadyTag);
    PCGData = FTerracePCGPlotData();
    if (BoundarySpline) { BoundarySpline->ComponentTags.AddUnique(BoundaryTag); }
    if (!IsValid(PCGBundSpline))
    {
        OutReport = PCGSyncStatus = LOCTEXT("PCGNoSpline", "缺少 PCGBundSpline 组件，请重新加载 Plot 或重新放置。");
        return false;
    }
    PCGBundSpline->ComponentTags.AddUnique(OutputTag);
    auto ClearOutput = [this, &OutReport](const FText& Error)
    {
        PCGBundSpline->ClearSplinePoints(false);
        PCGBundSpline->SetClosedLoop(false, true);
        PCGSyncStatus = OutReport = Error;
        return false;
    };
    if (!bEnablePCGOutput)
    {
        return ClearOutput(LOCTEXT("PCGDisabled", "已关闭此 Plot 的 PCG 输出，并清空派生样条。"));
    }
    FTerraceBoundarySnapshot Snapshot;
    FText Error;
    if (!BuildBoundarySnapshot(Snapshot, Error))
    {
        return ClearOutput(Error);
    }
    if (TerraceGeometry::SignedArea(Snapshot.Boundary) < 0.0)
    {
        Algo::Reverse(Snapshot.Boundary);
    }
    TArray<FVector> Points;
    Points.Reserve(Snapshot.Boundary.Num());
    for (const FVector2D& XY : Snapshot.Boundary)
    {
        Points.Emplace(XY, Snapshot.WorldElevation);
    }
    PCGBundSpline->SetAbsolute(false, false, true);
    PCGBundSpline->SetRelativeTransform(FTransform::Identity);
    PCGBundSpline->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        PCGBundSpline->SetSplinePointType(I, ESplinePointType::Linear, false);
        PCGBundSpline->SetScaleAtSplinePoint(I, FVector::OneVector, false);
    }
    PCGBundSpline->SetClosedLoop(true, true);
    PCGBundSpline->bSplineHasBeenEdited = false;
    PCGData.bValid = true;
    PCGData.PlotId = Snapshot.PlotId.ToString(EGuidFormats::DigitsWithHyphens);
    PCGData.WorldElevation = Snapshot.WorldElevation;
    PCGData.BundWidth = Settings.BundWidth;
    PCGData.BundHeight = Settings.BundHeight;
    PCGData.BundCenterInset = Settings.BundWidth * 0.5;
    PCGData.Seed = Settings.Seed;
    Tags.AddUnique(ReadyTag);
    OutReport = PCGSyncStatus = FText::Format(LOCTEXT("PCGSynced", "PCG 输入已同步：{0} 个闭合边界点，世界标高 {1} cm。只选择组件标签 TerracePCGBundSpline；地形更新完成后手动 Generate PCG。"),
        Points.Num(), FText::AsNumber(Snapshot.WorldElevation));
    return true;
}

bool ATerracePlotActor::SampleBoundary(TArray<FVector2D>& OutBoundary, FText& OutError) const
{
    OutBoundary.Reset();
    OutError = FText::GetEmpty();
    if (!Settings.Validate(OutError))
    {
        return false;
    }
    const FVector Scale = GetActorScale3D();
    const FRotator Rotation = GetActorRotation();
    if (GetActorTransform().ContainsNaN() || !FMath::IsFinite(GetWorldElevation())
        || Scale.X <= UE_SMALL_NUMBER || Scale.Y <= UE_SMALL_NUMBER || Scale.Z <= UE_SMALL_NUMBER
        || !FMath::IsNearlyZero(Rotation.Pitch, 0.01) || !FMath::IsNearlyZero(Rotation.Roll, 0.01))
    {
        OutError = LOCTEXT("Transform", "Keep the plot horizontal with finite coordinates and positive scale.");
        return false;
    }
    if (!IsValid(BoundarySpline) || !BoundarySpline->IsClosedLoop()
        || BoundarySpline->GetNumberOfSplinePoints() < 3)
    {
        OutError = LOCTEXT("Closed", "The boundary must be a closed spline with at least three control points.");
        return false;
    }
    const FVector SplineScale = BoundarySpline->GetComponentScale();
    const FRotator SplineRotation = BoundarySpline->GetComponentRotation();
    if (BoundarySpline->GetComponentTransform().ContainsNaN()
        || SplineScale.X <= UE_SMALL_NUMBER || SplineScale.Y <= UE_SMALL_NUMBER || SplineScale.Z <= UE_SMALL_NUMBER
        || !FMath::IsNearlyZero(SplineRotation.Pitch, 0.01) || !FMath::IsNearlyZero(SplineRotation.Roll, 0.01))
    {
        OutError = LOCTEXT("SplineTransform", "Keep the boundary component horizontal with finite coordinates and positive scale.");
        return false;
    }
    const double Length = BoundarySpline->GetSplineLength();
    if (!FMath::IsFinite(Length) || Length <= 0.0
        || BoundarySpline->GetNumberOfSplinePoints() > TerraceGeometry::MaxBoundaryVertices)
    {
        OutError = LOCTEXT("Length", "Invalid spline length or too many control points.");
        return false;
    }
    for (int32 I = 0; I < BoundarySpline->GetNumberOfSplineSegments(); ++I)
    {
        const double Start = BoundarySpline->GetDistanceAlongSplineAtSplinePoint(I);
        const double End = I + 1 < BoundarySpline->GetNumberOfSplinePoints()
            ? BoundarySpline->GetDistanceAlongSplineAtSplinePoint(I + 1) : Length;
        if (!FMath::IsFinite(Start) || !FMath::IsFinite(End) || End <= Start)
        {
            OutBoundary.Reset();
            OutError = LOCTEXT("SegmentLength", "Boundary has invalid or zero-length spline segments.");
            return false;
        }
        const double Count = FMath::Max(1.0, FMath::CeilToDouble((End - Start) / Settings.SampleSpacing));
        if (!FMath::IsFinite(Count) || Count > TerraceGeometry::MaxBoundaryVertices - OutBoundary.Num())
        {
            OutBoundary.Reset();
            OutError = LOCTEXT("Budget", "Boundary exceeds 512 samples. Increase Sample Spacing or simplify the spline.");
            return false;
        }
        for (int32 J = 0; J < static_cast<int32>(Count); ++J)
        {
            const FVector P = BoundarySpline->GetLocationAtDistanceAlongSpline(
                static_cast<float>(FMath::Lerp(Start, End, J / Count)), ESplineCoordinateSpace::World);
            if (P.ContainsNaN())
            {
                OutBoundary.Reset();
                OutError = LOCTEXT("Position", "Spline contains invalid positions.");
                return false;
            }
            OutBoundary.Emplace(P.X, P.Y);
        }
    }
    return true;
}

bool ATerracePlotActor::BuildBoundarySnapshot(FTerraceBoundarySnapshot& OutSnapshot, FText& OutError) const
{
    OutSnapshot = FTerraceBoundarySnapshot();
    if (!PlotId.IsValid())
    {
        OutError = LOCTEXT("Id", "Plot has no valid identity. Recreate the authoring actor.");
        return false;
    }
    TArray<FVector2D> Boundary;
    if (!SampleBoundary(Boundary, OutError) || !TerraceGeometry::ValidatePolygon(Boundary, OutError))
    {
        return false;
    }
    OutSnapshot.PlotId = PlotId;
    OutSnapshot.WorldElevation = GetWorldElevation();
    OutSnapshot.Settings = Settings;
    OutSnapshot.Boundary = MoveTemp(Boundary);
    for (const FVector2D& Point : OutSnapshot.Boundary)
    {
        OutSnapshot.Bounds += Point;
    }
    return true;
}

void ATerracePlotActor::PostActorCreated()
{
    Super::PostActorCreated();
    if (!IsTemplate())
    {
        PlotId = FGuid::NewGuid();
    }
}

void ATerracePlotActor::PostLoad()
{
    Super::PostLoad();
    if (!PlotId.IsValid() && !IsTemplate())
    {
        PlotId = FGuid::NewGuid();
    }
    Tags.AddUnique(PlotTag);
}

void ATerracePlotActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
    Super::PostDuplicate(DuplicateMode);
    if (DuplicateMode != EDuplicateMode::PIE && !IsTemplate())
    {
        PlotId = FGuid::NewGuid();
    }
}

void ATerracePlotActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!IsTemplate())
    {
        FText Report;
        RefreshPCGOutput(Report);
    }
}

void ATerracePlotActor::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
#if WITH_EDITOR
    if (!IsTemplate() && GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
    {
        FText Report;
        RefreshPCGOutput(Report);
    }
#endif
}

#if WITH_EDITOR
void ATerracePlotActor::PostEditImport()
{
    Super::PostEditImport();
    PlotId = FGuid::NewGuid();
    FText Report;
    RefreshPCGOutput(Report);
}

void ATerracePlotActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    FText Report;
    RefreshPCGOutput(Report);
}

void ATerracePlotActor::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    if (bFinished)
    {
        FText Report;
        RefreshPCGOutput(Report);
    }
}

void ATerracePlotActor::PostEditUndo()
{
    Super::PostEditUndo();
    FText Report;
    RefreshPCGOutput(Report);
}
#endif

#undef LOCTEXT_NAMESPACE
