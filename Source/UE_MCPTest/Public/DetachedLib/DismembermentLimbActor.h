// 掉落的断肢：一个带物理模拟的静态网格体 Actor。
// 生成后短暂禁用碰撞（物理避让），再恢复正常碰撞，数秒后停止物理模拟。

#pragma once

#include "CoreMinimal.h"
#include "DetachedLib/DismembermentConfigData.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DismembermentLimbActor.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;
class USkeletalMesh;

UCLASS()
class UE_MCPTEST_API ADismembermentLimbActor : public AActor
{
	GENERATED_BODY()

public:
	ADismembermentLimbActor();

	void ReInitializeLimb(const FTransform& NewTransform, EDismembermentLimbMeshType MeshType,
		UStaticMesh* NewStaticMesh, USkeletalMesh* NewSkeletalMesh, FName NewCollisionProfileName);
	void AddImpulse(const FVector& ImpulseDirection, float ImpactForce, const FVector& AngularImpulseDegrees);

	/** 设置物理避让参数（由 UDismembermentComponent 在生成断肢后调用）。 */
	void SetPhysicalSimulationAvoidance(float InAvoidanceTime,
		FName InAvoidanceCollisionProfileName);
	void SetLimbPhysicsSimulationTime(float InPhysicsSimulationTime);

protected:
	void StartLimbPhysicsSimulation();
	void StopLimbPhysicsSimulation() const;
	void EnableLimbCollision() const;
	void DisableLimbCollision() const;

	float LimbPhysicsSimulationTime = 5.0f;
	FTimerHandle ReInitializeTimerHandle;
	FTimerHandle PhysicalSimulationAvoidanceTimerHandle;

	FName CurrentCollisionProfileName;

	/** 当前实际承载断肢资产的组件，避免用 Visibility 推断物理目标。 */
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> ActiveLimbComponent;

	float PhysicalSimulationAvoidanceTime = 0.1f;
	FName PhysicalSimulationAvoidanceCollisionProfileName = FName("Ragdoll");

public:
	UPROPERTY(BlueprintReadOnly, Category = "Limb")
	TObjectPtr<UStaticMeshComponent> LimbMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Limb")
	TObjectPtr<USkeletalMeshComponent> LimbSkeletalMesh;
};

