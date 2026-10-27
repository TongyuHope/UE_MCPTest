#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "BiomePaintedSplineComponent.generated.h"

/** A saved, independently editable stroke. PCG can filter ComponentTags by BiomePaintedSpline. */
UCLASS(ClassGroup = (BiomePCG), meta = (BlueprintSpawnableComponent))
class BIOMEPCGRUNTIME_API UBiomePaintedSplineComponent : public USplineComponent
{
    GENERATED_BODY()

public:
    UBiomePaintedSplineComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BiomePCG")
    FGuid StrokeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BiomePCG")
    FString StrokeLabel;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
    virtual void PostEditComponentMove(bool bFinished) override;
    virtual void PostEditUndo() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    void NotifyPainter();
#endif
};
