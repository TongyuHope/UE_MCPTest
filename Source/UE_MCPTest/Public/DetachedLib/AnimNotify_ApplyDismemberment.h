// 动画通知：在动画播放到指定帧时触发断肢。

#pragma once

#include "CoreMinimal.h"
#include "DetachedLib/DismembermentComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ApplyDismemberment.generated.h"

/**
 * 放置在动画轨道上的断肢通知。
 * 触发时会找到动画播放者所在 Actor 上的 UDismembermentComponent，并对其调用 ApplyDismemberment。
 */
UCLASS(DisplayName = "Apply Dismemberment")
class UE_MCPTEST_API UAnimNotify_ApplyDismemberment : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ApplyDismemberment();

	/** 需要断肢的骨骼及各自的冲击参数。 */
	UPROPERTY(EditAnywhere, Category = "Dismemberment", meta = (ForceInlineRow))
	TMap<FName, FDismembermentConfig> ApplyDismembermentConfigs;

	/** 销毁每条配置指定的整个目标网格组件，适用于独立的手臂/腿部网格。 */
	UPROPERTY(EditAnywhere, Category = "Dismemberment")
	bool bHideWholeTargetMesh = false;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual FString GetNotifyName_Implementation() const override;
#endif
};

