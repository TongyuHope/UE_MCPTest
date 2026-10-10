// 断肢核心组件：挂载在角色上，负责隐藏骨骼并在断口处生成掉落的断肢。

#pragma once

#include "CoreMinimal.h"
#include "DetachedLib/DismembermentConfigData.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "DismembermentComponent.generated.h"

class USkeletalMeshComponent;

/** 断肢冲击方向所在的坐标系。 */
UENUM(BlueprintType)
enum class ELimbSpace : uint8
{
	WorldSpace,
	BoneSpace
};

/** 断肢时施加的冲击参数（供蓝图/动画通知传入）。 */
USTRUCT(BlueprintType)
struct FDismembermentConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FName TargetSkeletalMeshComponentName;

	/** 隐藏整个目标网格，而不是隐藏指定骨骼及其子骨骼。适用于独立的手臂/腿部网格。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	bool bHideWholeTargetMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FVector ImpactDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	float ImpactForce = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FVector AngularImpulseDegrees = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	ELimbSpace LimbSpace = ELimbSpace::BoneSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	bool bUseWorldScale = true;
};

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

	/** 目标骨骼网格体组件名（必须与角色上的 SkeletalMeshComponent 名称一致）。 */
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (ClampMin = 0))
	float LimbPhysicsSimulationTime = 5.0f;

	/** 断肢 Actor 的保留时间（秒）；设为 0 时不自动销毁。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (ClampMin = 0))
	float DetachedLimbLifeSpan = 30.0f;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> DrivenSkeletalMeshComponent;
};

