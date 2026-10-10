// 动画通知：在动画播放到指定帧时触发断肢。

#pragma once

#include "CoreMinimal.h"
#include "DetachedLib/DismembermentComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ApplyDismemberment.generated.h"

/**
 * 放置在动画轨道上的断肢通知。
 * 触发时会找到动画播放者所在 Actor 上的 UDismembermentComponent，并按顺序执行请求数组。
 */
UCLASS(DisplayName = "Apply Dismemberment")
class UE_MCPTEST_API UAnimNotify_ApplyDismemberment : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ApplyDismemberment();
	virtual void PostLoad() override;

	/** Editor options only; runtime always uses the owner's component asset. */
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Dismemberment", meta = (DisplayName = "Authoring Config Data"))
	TObjectPtr<UDismembermentConfigData> AuthoringConfigData;
#endif

	UPROPERTY(EditAnywhere, Category = "Dismemberment", meta = (TitleProperty = "BoneName"))
	TArray<FDismembermentRequest> DismembermentRequests;

	UFUNCTION()
	TArray<FString> GetBoneNameOptions() const;

	UFUNCTION()
	TArray<FString> GetPresetNameOptions() const;

	/** Idempotent; PostLoad converts old serialized map/OR switches to explicit overrides. */
	void MigrateLegacyConfigurations();

	/** Legacy serialized map. Migrated on load; hidden from new editor configuration. */
	UPROPERTY()
	TMap<FName, FDismembermentConfig> ApplyDismembermentConfigs;

	/** Legacy notify-level OR switch; folded into each migrated request. */
	UPROPERTY()
	bool bHideWholeTargetMesh = false;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual FString GetNotifyName_Implementation() const override;
#endif
};

