#include "BiomePaintedSplineComponent.h"

#include "BiomeSplinePainterActor.h"

UBiomePaintedSplineComponent::UBiomePaintedSplineComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ComponentTags.AddUnique(TEXT("BiomePaintedSpline"));
    SetMobility(EComponentMobility::Static);
}

#if WITH_EDITOR
void UBiomePaintedSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    TWeakObjectPtr<ABiomeSplinePainterActor> Painter = Cast<ABiomeSplinePainterActor>(GetOwner());
    Super::PostEditChangeProperty(PropertyChangedEvent);
    // Property changes can rerun construction scripts and replace this component.
    if (Painter.IsValid())
    {
        Painter->RequestPCGRefresh();
    }
}

void UBiomePaintedSplineComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    TWeakObjectPtr<ABiomeSplinePainterActor> Painter = Cast<ABiomeSplinePainterActor>(GetOwner());
    Super::PostEditChangeChainProperty(PropertyChangedEvent);
    if (Painter.IsValid())
    {
        Painter->RequestPCGRefresh();
    }
}

void UBiomePaintedSplineComponent::PostEditComponentMove(bool bFinished)
{
    Super::PostEditComponentMove(bFinished);
    if (bFinished)
    {
        NotifyPainter();
    }
}

void UBiomePaintedSplineComponent::NotifyPainter()
{
    if (ABiomeSplinePainterActor* Painter = Cast<ABiomeSplinePainterActor>(GetOwner()))
    {
        Painter->RequestPCGRefresh();
    }
}

void UBiomePaintedSplineComponent::PostEditUndo()
{
    Super::PostEditUndo();
    NotifyPainter();
}

void UBiomePaintedSplineComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    if (!bDestroyingHierarchy)
    {
        NotifyPainter();
    }
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
#endif
