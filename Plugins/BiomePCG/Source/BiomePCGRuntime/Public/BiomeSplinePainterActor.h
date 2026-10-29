#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif
#include "BiomeSplinePainterActor.generated.h"

class UBiomePaintedSplineComponent;
class UPCGComponent;
class USceneComponent;

/** Owns persisted spline instance components; the assigned PCG graph belongs to the user. */
UCLASS(Blueprintable)
class BIOMEPCGRUNTIME_API ABiomeSplinePainterActor : public AActor
{
    GENERATED_BODY()

public:
    ABiomeSplinePainterActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BiomePCG")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BiomePCG")
    TObjectPtr<UPCGComponent> PCGComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BiomePCG|Generation")
    bool bAutoRefreshPCG = true;

    UFUNCTION(BlueprintPure, Category = "BiomePCG|Splines")
    TArray<UBiomePaintedSplineComponent*> GetPaintedSplines() const;

    /** Editor generation only; packaged builds retain the saved splines without running this tool. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "BiomePCG|Generation")
    void RefreshPCG();

#if WITH_EDITOR
    UBiomePaintedSplineComponent* AddPaintedSpline(const TArray<FVector>& WorldPoints, bool bSmooth);
    bool RemovePaintedSpline(UBiomePaintedSplineComponent* Spline);
    void RequestPCGRefresh(bool bForce = false);

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostLoad() override;
    virtual void PostEditMove(bool bFinished) override;
    virtual void PostEditUndo() override;
    virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void BeginDestroy() override;

private:
    bool ProcessPCGRefresh(float DeltaTime);
    FTSTicker::FDelegateHandle RefreshTickerHandle;
    TWeakObjectPtr<UPCGComponent> RefreshSource;
    bool bForcePendingRefresh = false;
    bool bRefreshRequested = false;
    bool bWaitingForCleanup = false;
    bool bNeedsResourceRecovery = true;
#endif
};
