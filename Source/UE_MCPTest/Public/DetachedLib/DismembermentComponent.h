// 断肢核心组件：挂载在角色上，负责隐藏骨骼并在断口处生成掉落的断肢。

#pragma once

#include "CoreMinimal.h"
#include "DetachedLib/DismembermentConfigData.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "DismembermentComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UE_MCPTEST_API UDismembermentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDismembermentComponent();
	virtual void BeginPlay() override;

	/**
	 * 对指定骨骼执行断肢：隐藏该骨骼（连同子骨骼）或销毁整个目标网格组件，并在断口处生成一个
	 * 带物理模拟的断肢 Actor，按 Config 施加冲量。
	 */
	UFUNCTION(BlueprintCallable, Category = "Dismemberment",
		meta = (AutoCreateRefTerm = "BoneName, DismembermentConfig"))
	bool ApplyDismemberment(const FName& BoneName, const FDismembermentConfig& DismembermentConfig);

	UFUNCTION(BlueprintCallable, Category = "Dismemberment", meta = (AutoCreateRefTerm = "Request"))
	bool ApplyRegisteredDismemberment(const FDismembermentRequest& Request);

	/** Ordered batch, returning the number of successful requests. */
	UFUNCTION(BlueprintCallable, Category = "Dismemberment")
	int32 ApplyDismembermentRequests(const TArray<FDismembermentRequest>& Requests);

	UFUNCTION(BlueprintCallable, Category = "Dismemberment|Debug")
	void DumpDismembermentDiagnostics() const;

	void GetDismembermentDiagnostics(TArray<FString>& OutErrors) const;
	EDismembermentLimbMeshType GetResolvedLimbMeshType() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Set from device/quality settings. Does not alter the shared data asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	EDismembermentLimbMeshTypeOverride LimbMeshTypeOverride = EDismembermentLimbMeshTypeOverride::UseAssetDefault;

	/** Runtime component object name; None selects Character::GetMesh or a unique mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FName DrivenSkeletalMeshComponentName = FName("CharacterMesh0");

	/** 断肢配置数据资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment",
		DisplayName = "Dismemberment Config Data")
	TObjectPtr<UDismembermentConfigData> DismembermentConfigData;

	/** 断肢生成后使用临时碰撞预设的时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|PhysicalSimulationAvoidance",
		meta = (ClampMin = 0))
	float PhysicalSimulationAvoidanceTime = 0.1f;

	/** 物理避让期间使用的碰撞预设。默认 Ragdoll 会忽略 Pawn。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|PhysicalSimulationAvoidance")
	FCollisionProfileName PhysicalSimulationAvoidanceCollisionProfileName = FCollisionProfileName(
		TEXT("Ragdoll"));

	/** 临时避让结束后，断肢继续进行物理模拟的时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Physics", meta = (ClampMin = 0, AdvancedDisplay))
	float LimbPhysicsSimulationTime = 5.0f;

	/** 断肢 Actor 的保留时间（秒）；设为 0 时不自动销毁。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Physics", meta = (ClampMin = 0, AdvancedDisplay))
	float DetachedLimbLifeSpan = 30.0f;

private:
	void CollectDismembermentDiagnostics(TArray<FString>& OutErrors, bool bValidateHost) const;
	USkeletalMeshComponent* ResolveDrivenMesh() const;
	USkeletalMeshComponent* FindMeshByName(FName ComponentName) const;
	bool ValidateTarget(FName BoneName, const FDismembermentConfig& Config,
		USkeletalMeshComponent*& OutMesh, FString& OutError) const;
	bool ReportFailure(FName BoneName, FName TargetName, const FString& Reason) const;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> DrivenSkeletalMeshComponent;
};

