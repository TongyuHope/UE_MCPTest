#include "BiomeSplinePainterActor.h"

#include "BiomePaintedSplineComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PCGComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "PCGManagedResourceContainer.h"
#include "Subsystems/PCGSubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogBiomeSplinePainter, Log, All);
#endif

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
    bRefreshRequested = true;
    bForcePendingRefresh |= bForce;
    if (!RefreshTickerHandle.IsValid())
    {
        RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
            {
                return ProcessPCGRefresh(DeltaTime);
            }), 0.2f);
    }
}

bool ABiomeSplinePainterActor::ProcessPCGRefresh(float DeltaTime)
{
    auto StopRefresh = [this]()
    {
        RefreshTickerHandle.Reset();
        bWaitingForCleanup = false;
        bForcePendingRefresh = false;
        bRefreshRequested = false;
        return false;
    };
    if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor || IsActorBeingDestroyed()
        || !IsValid(PCGComponent))
    {
        return StopRefresh();
    }
    // Defer world/resource mutation until load, save and editor transactions have finished.
    if (!PCGComponent->IsRegistered() || UE::IsSavingPackage(this)
        || (GEditor && (GEditor->PlayWorld || GEditor->IsTransactionActive())))
    {
        return true;
    }
    if (PCGComponent->IsManagedByRuntimeGenSystem())
    {
        return StopRefresh();
    }
    UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem();
    if (!Subsystem)
    {
        return true;
    }
    if (RefreshSource.Get() != PCGComponent.Get())
    {
        RefreshSource = PCGComponent;
        bWaitingForCleanup = false;
        bRefreshRequested = true;
        // Construction-script reinstancing can replace the PCG component and its resource list.
        bNeedsResourceRecovery = true;
    }

    TArray<UPCGComponent*> Sources = { PCGComponent.Get() };
    Subsystem->ForAllRegisteredLocalComponents(PCGComponent, [&Sources](UPCGComponent* Local)
    {
        if (IsValid(Local))
        {
            Sources.AddUnique(Local);
        }
    });
    for (UPCGComponent* Source : Sources)
    {
        if (Source->IsGenerating() || Source->IsCleaningUp() || Source->IsRefreshInProgress()
            || !Source->AreManagedResourcesAccessible())
        {
            return true;
        }
    }

    if (!bAutoRefreshPCG && !bForcePendingRefresh)
    {
        PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::All);
        return StopRefresh();
    }
    if (bWaitingForCleanup)
    {
        bWaitingForCleanup = false;
        // Undo or another edit during asynchronous cleanup may have restored old resources.
        // Clean that newer state too, rather than consuming its request before generation.
        if (!bRefreshRequested && !bNeedsResourceRecovery)
        {
            bForcePendingRefresh = false;
            PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::All);
            PCGComponent->ResetLastGeneratedBounds();
            if (PCGComponent->GetGraph())
            {
                PCGComponent->GenerateLocal(true);
            }
            MarkPackageDirty();
            return true;
        }
    }
    if (!bRefreshRequested)
    {
        return StopRefresh();
    }

    bool bRecoverUnlinkedResources = bNeedsResourceRecovery || bForcePendingRefresh;
    int32 InvalidResourceCount = 0;
    for (UPCGComponent* Source : Sources)
    {
        // Operate only on this component and its own registered partition components.
        // Remove broken records before the engine cleanup task visits them.
        FPCGManagedResourceContainerHelper Resources(Source);
        TArray<TObjectPtr<UPCGManagedResource>> Managed = Resources.GetManagedResourcesCopy();
        const int32 Removed = Managed.RemoveAll([](const TObjectPtr<UPCGManagedResource>& Resource)
        {
            return !IsValid(Resource.Get());
        });
        if (Removed > 0)
        {
            Source->Modify();
            Resources.SetManagedResources(MoveTemp(Managed));
            InvalidResourceCount += Removed;
            bRecoverUnlinkedResources = true;
        }
    }
    if (InvalidResourceCount > 0)
    {
        UE_LOG(LogBiomeSplinePainter, Display,
            TEXT("%s: removed %d invalid PCG resource records; recovering only this painter's generated output."),
            *GetPathName(), InvalidResourceCount);
    }

    PCGComponent->Modify();
    PCGComponent->DirtyGenerated(EPCGComponentDirtyFlag::All);
    bWaitingForCleanup = true;
    bRefreshRequested = false;
    bNeedsResourceRecovery = false;
    if (bRecoverUnlinkedResources)
    {
        // This is the engine's per-component cleanup/purge path, not the global menu operation.
        PCGComponent->CleanupLocalDeleteAllGeneratedObjects({});
    }
    else
    {
        PCGComponent->CleanupLocal(true);
    }
    return true; // Regenerate on a later tick, after cleanup/local partition tasks finish.
}

void ABiomeSplinePainterActor::PostLoad()
{
    Super::PostLoad();
    // Retain the saved vegetation on load; recover it when the user first edits or refreshes.
    bNeedsResourceRecovery = true;
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
    bNeedsResourceRecovery = true;
    RequestPCGRefresh();
}

void ABiomeSplinePainterActor::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
    Super::PostEditUndo(TransactionAnnotation);
    bNeedsResourceRecovery = true;
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
