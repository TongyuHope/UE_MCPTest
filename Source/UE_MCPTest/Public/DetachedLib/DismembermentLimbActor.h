// Independent static/skeletal detached model, with timed collision-profile avoidance.

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
	bool AddImpulse(const FVector& ImpulseDirection, float ImpactForce, const FVector& AngularImpulseDegrees);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 设置物理避让参数（由 UDismembermentComponent 在生成断肢后调用）。 */
	void SetPhysicalSimulationAvoidance(float InAvoidanceTime,
		FName InAvoidanceCollisionProfileName);
	void SetLimbPhysicsSimulationTime(float InPhysicsSimulationTime);

protected:
	bool StartLimbPhysicsSimulation();
	void StopLimbPhysicsSimulation() const;
	void EnableLimbCollision() const;
	void DisableLimbCollision() const;

	float LimbPhysicsSimulationTime = 5.0f;
	FTimerHandle StopPhysicsTimerHandle;
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

