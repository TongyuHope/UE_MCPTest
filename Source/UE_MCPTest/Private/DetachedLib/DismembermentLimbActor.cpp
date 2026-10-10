#include "DetachedLib/DismembermentLimbActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DetachedLib/DismembermentConfigData.h"
#include "Engine/World.h"
#include "TimerManager.h"


ADismembermentLimbActor::ADismembermentLimbActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	LimbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LimbMesh"));
	LimbMesh->SetupAttachment(RootComponent);
	LimbSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LimbSkeletalMesh"));
	LimbSkeletalMesh->SetupAttachment(RootComponent);
	LimbMesh->SetVisibility(false);
	LimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LimbSkeletalMesh->SetVisibility(false);
	LimbSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADismembermentLimbActor::ReInitializeLimb(const FTransform& NewTransform, const EDismembermentLimbMeshType MeshType,
	UStaticMesh* NewStaticMesh, USkeletalMesh* NewSkeletalMesh, const FName NewCollisionProfileName)
{
	// 允许同一个 Actor 被重复初始化时丢弃上一轮尚未触发的计时器。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReInitializeTimerHandle);
		World->GetTimerManager().ClearTimer(PhysicalSimulationAvoidanceTimerHandle);
	}
	SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentCollisionProfileName = NewCollisionProfileName;
	ActiveLimbComponent = nullptr;
	// 先清理另一种网格可能留下的状态，保证重复初始化不会继续参与碰撞或模拟。
	if (LimbMesh)
	{
		LimbMesh->SetVisibility(false);
		LimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LimbMesh->SetSimulatePhysics(false);
	}
	if (LimbSkeletalMesh)
	{
		LimbSkeletalMesh->SetVisibility(false);
		LimbSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LimbSkeletalMesh->SetSimulatePhysics(false);
	}
	if (MeshType == EDismembermentLimbMeshType::StaticMesh && LimbMesh)
	{
		LimbMesh->SetStaticMesh(NewStaticMesh);
		LimbMesh->SetRelativeTransform(FTransform::Identity);
		LimbMesh->SetVisibility(true);
		LimbMesh->SetCollisionProfileName(NewCollisionProfileName);
		LimbMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		LimbSkeletalMesh->SetVisibility(false);
		LimbMesh->SetSimulatePhysics(false);
		ActiveLimbComponent = LimbMesh;
	}
	else if (MeshType == EDismembermentLimbMeshType::SkeletalMesh && LimbSkeletalMesh)
	{
		LimbSkeletalMesh->SetSkeletalMesh(NewSkeletalMesh);
		LimbSkeletalMesh->SetRelativeTransform(FTransform::Identity);
		LimbSkeletalMesh->SetVisibility(true);
		LimbSkeletalMesh->SetSimulatePhysics(false);
		LimbSkeletalMesh->SetCollisionProfileName(NewCollisionProfileName);
		LimbSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		LimbMesh->SetVisibility(false);
		ActiveLimbComponent = LimbSkeletalMesh;
	}

	// 两个网格都挂在根节点上，并使用单位相对变换，因此 Actor 变换即可定位断肢。
}

void ADismembermentLimbActor::AddImpulse(const FVector& ImpulseDirection, const float ImpactForce,
	const FVector& AngularImpulseDegrees)
{
	if (IsValid(ActiveLimbComponent))
	{
		const FVector Impulse = ImpulseDirection.GetSafeNormal() * ImpactForce;

		StartLimbPhysicsSimulation();
		ActiveLimbComponent->WakeAllRigidBodies();
		ActiveLimbComponent->AddImpulse(Impulse, NAME_None, true);
		ActiveLimbComponent->AddAngularImpulseInDegrees(AngularImpulseDegrees, NAME_None, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LimbMesh is not valid!"));
	}
}

void ADismembermentLimbActor::SetPhysicalSimulationAvoidance(const float InAvoidanceTime,
	const FName InAvoidanceCollisionProfileName)
{
	PhysicalSimulationAvoidanceTime = FMath::Max(0.0f, InAvoidanceTime);
	PhysicalSimulationAvoidanceCollisionProfileName = InAvoidanceCollisionProfileName;
}

void ADismembermentLimbActor::SetLimbPhysicsSimulationTime(const float InPhysicsSimulationTime)
{
	LimbPhysicsSimulationTime = FMath::Max(0.0f, InPhysicsSimulationTime);
}

void ADismembermentLimbActor::StartLimbPhysicsSimulation()
{
	if (IsValid(ActiveLimbComponent))
	{
		// 物理避让：先切换到临时碰撞预设，再恢复断肢的正常预设。
		DisableLimbCollision();
		if (PhysicalSimulationAvoidanceTime > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(PhysicalSimulationAvoidanceTimerHandle,
				this, &ThisClass::EnableLimbCollision, PhysicalSimulationAvoidanceTime, false);
		}
		else
		{
			PhysicalSimulationAvoidanceTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
				this, &ThisClass::EnableLimbCollision);
		}

		ActiveLimbComponent->SetSimulatePhysics(true);

		// 生命周期控制：一段时间后停止物理模拟。
		const float StopPhysicsDelay = LimbPhysicsSimulationTime + PhysicalSimulationAvoidanceTime;
		if (StopPhysicsDelay > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				ReInitializeTimerHandle, this, &ThisClass::StopLimbPhysicsSimulation,
				StopPhysicsDelay, false);
		}
		else
		{
			ReInitializeTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
				this, &ThisClass::StopLimbPhysicsSimulation);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LimbMesh is not valid!"));
	}
}

void ADismembermentLimbActor::StopLimbPhysicsSimulation() const
{
	if (IsValid(ActiveLimbComponent))
	{
		ActiveLimbComponent->SetSimulatePhysics(false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LimbMesh is not valid!"));
	}
}

void ADismembermentLimbActor::EnableLimbCollision() const
{
	if (IsValid(ActiveLimbComponent))
	{
		ActiveLimbComponent->SetCollisionProfileName(CurrentCollisionProfileName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LimbMesh is not valid!"));
	}
}

void ADismembermentLimbActor::DisableLimbCollision() const
{
	if (IsValid(ActiveLimbComponent))
	{
		ActiveLimbComponent->SetCollisionProfileName(PhysicalSimulationAvoidanceCollisionProfileName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LimbMesh is not valid!"));
	}
}

