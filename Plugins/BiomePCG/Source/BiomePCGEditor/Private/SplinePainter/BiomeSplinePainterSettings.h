#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "BiomeSplinePainterSettings.generated.h"

class ABiomeSplinePainterActor;
class UPCGGraphInterface;

UCLASS(Transient)
class UBiomeSplinePainterSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Target", meta = (DisplayName = "目标 Actor"))
    TWeakObjectPtr<ABiomeSplinePainterActor> TargetActor;

    /** Empty means create/reuse the default BP_BiomeSplinePainter asset on the first placement. */
    UPROPERTY(EditAnywhere, Category = "Target", meta = (DisplayName = "放置蓝图类（留空自动创建）"))
    TSubclassOf<ABiomeSplinePainterActor> ActorClass;

    UPROPERTY(EditAnywhere, Category = "Drawing", meta = (DisplayName = "控制点最小间距 (cm)", ClampMin = "1.0"))
    double MinPointSpacing = 100.0;

    UPROPERTY(EditAnywhere, Category = "Drawing", meta = (DisplayName = "最短有效笔画 (cm)", ClampMin = "1.0"))
    double MinStrokeLength = 10.0;

    UPROPERTY(EditAnywhere, Category = "Drawing", meta = (DisplayName = "最大连续跨越距离 (cm)", ClampMin = "1.0"))
    double MaxGapDistance = 2000.0;

    UPROPERTY(EditAnywhere, Category = "Drawing", meta = (DisplayName = "平滑曲线"))
    bool bSmooth = true;

    UPROPERTY(EditAnywhere, Category = "Surface", meta = (DisplayName = "地表碰撞通道"))
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, Category = "Surface", meta = (DisplayName = "忽略 PCG 生成物"))
    bool bIgnorePCGGenerated = true;

    UPROPERTY(EditAnywhere, Category = "PCG", meta = (DisplayName = "用户 PCG 图"))
    TObjectPtr<UPCGGraphInterface> Graph;

    UPROPERTY(EditAnywhere, Category = "PCG", meta = (DisplayName = "提交后自动刷新"))
    bool bAutoRefreshPCG = true;
};
