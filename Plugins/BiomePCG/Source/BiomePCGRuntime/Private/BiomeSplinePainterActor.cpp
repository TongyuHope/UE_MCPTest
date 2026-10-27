#include "BiomeSplinePainterActor.h"

#include "BiomePaintedSplineComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PCGComponent.h"

ABiomeSplinePainterActor::ABiomeSplinePainterActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    SetRootComponent(SceneRoot);
    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
    PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

TArray<UBiomePaintedSplineComponent*> ABiomeSplinePainterActor::GetPaintedSplines() const
{
    TArray<UBiomePaintedSplineComponent*> Result;
    GetComponents<UBiomePaintedSplineComponent>(Result);
    Result.RemoveAll([](const UBiomePaintedSplineComponent* Spline)
    {
        return !IsValid(Spline) || Spline->IsBeingDestroyed();
    });
    Result.Sort([](const UBiomePaintedSplineComponent& A, const UBiomePaintedSplineComponent& B)
    {
        return A.GetName() < B.GetName();
    });
    return Result;
}

void ABiomeSplinePainterActor::RefreshPCG()
{
#if WITH_EDITOR
    RequestPCGRefresh(true);
#endif
}

#if WITH_EDITOR
UBiomePaintedSplineComponent* ABiomeSplinePainterActor::AddPaintedSpline(
    const TArray<FVector>& WorldPoints, bool bSmooth)
{
    if (WorldPoints.Num() < 2 || !GetWorld() || GetWorld()->WorldType != EWorldType::Editor)
    {
        return nullptr;
    }

    Modify();
    const FName Name = MakeUniqueObjectName(this, UBiomePaintedSplineComponent::StaticClass(), TEXT("PaintedSpline"));
    UBiomePaintedSplineComponent* Spline = NewObject<UBiomePaintedSplineComponent>(this, Name, RF_Transactional);
    Spline->Modify();
    AddInstanceComponent(Spline);
    Spline->OnComponentCreated();
    Spline->SetupAttachment(SceneRoot);
    Spline->StrokeId = FGuid::NewGuid();
    Spline->StrokeLabel = Name.ToString();
    Spline->ClearSplinePoints(false);
    const FTransform ActorTransform = GetActorTransform();
    for (const FVector& Point : WorldPoints)
    {
        Spline->AddSplinePoint(ActorTransform.InverseTransformPosition(Point), ESplineCoordinateSpace::Local, false);
    }
    for (int32 Index = 0; Index < WorldPoints.Num(); ++Index)
    {
        Spline->SetSplinePointType(Index,
            bSmooth ? ESplinePointType::Curve : ESplinePointType::Linear, false);
    }
    Spline->SetClosedLoop(false, false);
    Spline->bSplineHasBeenEdited = true;
    Spline->UpdateSpline();
    Spline->RegisterComponent();
    MarkPackageDirty();
    RequestPCGRefresh();
    return Spline;
}

bool ABiomeSplinePainterActor::RemovePaintedSpline(UBiomePaintedSplineComponent* Spline)
{
    if (!IsValid(Spline) || Spline->GetOwner() != this)
    {
        return false;
    }
    Modify();
    Spline->Modify();
    RemoveInstanceComponent(Spline);
    Spline->DestroyComponent();
    MarkPackageDirty();
    RequestPCGRefresh();
    return true;
}

void ABiomeSplinePainterActor::RequestPCGRefresh(bool bForce)
{
    if (IsTemplate() || !GetWorld() || GetWorld()->WorldType != EWorldType::Editor
        || IsActorBeingDestroyed() || !IsValid(PCGComponent))
    {
        return;
    }
    PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::Actor | EPCGComponentDirtyFlag::Input | EPCGComponentDirtyFlag::Data);
    bForcePendingRefresh |= bForce;
    if ((!bAutoRefreshPCG && !bForcePendingRefresh) || RefreshTickerHandle.IsValid())
    {
        return;
    }

    // Coalesce visualizer edits and undo callbacks, and wait for an in-flight PCG task.
    RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float)
        {
            if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor || IsActorBeingDestroyed()
                || !IsValid(PCGComponent) || !PCGComponent->IsRegistered())
            {
                RefreshTickerHandle.Reset();
                bForcePendingRefresh = false;
                return false;
            }
            if (PCGComponent->IsGenerating() || PCGComponent->IsCleaningUp())
            {
                return true;
            }
            const bool bGenerate = bForcePendingRefresh || bAutoRefreshPCG;
            bForcePendingRefresh = false;
            RefreshTickerHandle.Reset();
            if (bGenerate && PCGComponent->GetGraph() && !PCGComponent->IsManagedByRuntimeGenSystem())
            {
                PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::All);
                PCGComponent->GenerateLocal(true);
            }
            return false;
        }), 0.2f);
}

void ABiomeSplinePainterActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Hand-painted instance components are never rebuilt by the construction script.
    RequestPCGRefresh();
}

void ABiomeSplinePainterActor::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    if (bFinished)
    {
        RequestPCGRefresh();
    }
}

void ABiomeSplinePainterActor::PostEditUndo()
{
    Super::PostEditUndo();
    RequestPCGRefresh();
}

void ABiomeSplinePainterActor::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
    Super::PostEditUndo(TransactionAnnotation);
    RequestPCGRefresh();
}

void ABiomeSplinePainterActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    RequestPCGRefresh();
}

void ABiomeSplinePainterActor::BeginDestroy()
{
    if (RefreshTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(RefreshTickerHandle);
        RefreshTickerHandle.Reset();
    }
    Super::BeginDestroy();
}
#endif
