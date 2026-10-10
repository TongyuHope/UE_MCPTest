#include "DetachedLib/DismembermentComponent.h"

#include "DetachedLib/DismembermentLimbActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"


UDismembermentComponent::UDismembermentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDismembermentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsValid(GetOwner())) return;

	// 驱动网格只需在开始游戏时解析一次；模块化网格仍按配置名即时查找。
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetOwner()->GetComponents(USkeletalMeshComponent::StaticClass(), SkeletalMeshComponents);
	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent->GetName() == DrivenSkeletalMeshComponentName)
		{
			DrivenSkeletalMeshComponent = SkeletalMeshComponent;
			break;
		}
	}
}

bool UDismembermentComponent::ApplyDismemberment(const FName& BoneName,
	const FDismembermentConfig& DismembermentConfig)
{
	if (!IsValid(GetOwner()) || !IsValid(GetWorld())) return false;
	if (!IsValid(DrivenSkeletalMeshComponent) && DismembermentConfig.TargetSkeletalMeshComponentName.IsNone())
	{
		return false;
	}

	USkeletalMeshComponent* TargetMesh = DrivenSkeletalMeshComponent;
	if (!DismembermentConfig.TargetSkeletalMeshComponentName.IsNone())
	{
		TargetMesh = nullptr;
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		GetOwner()->GetComponents(USkeletalMeshComponent::StaticClass(), SkeletalMeshComponents);
		for (USkeletalMeshComponent* Candidate : SkeletalMeshComponents)
		{
			if (IsValid(Candidate) && Candidate->GetName() == DismembermentConfig.TargetSkeletalMeshComponentName)
			{
				TargetMesh = Candidate;
				break;
			}
		}
	}
	if (!IsValid(TargetMesh)) return false;
	if (DismembermentConfig.bHideWholeTargetMesh &&
		(DismembermentConfig.TargetSkeletalMeshComponentName.IsNone() || TargetMesh == DrivenSkeletalMeshComponent))
	{
		// Whole-component destruction is only valid for an explicitly selected follower/modular mesh.
		return false;
	}

	if (!IsValid(TargetMesh->GetSkeletalMeshAsset()))
	{
		return false;
	}

	if (!IsValid(DismembermentConfigData))
	{
		return false;
	}

	if (!DismembermentConfig.bHideWholeTargetMesh && TargetMesh->GetBoneIndex(BoneName) == INDEX_NONE)
	{
		return false;
	}

	const FSKMDismembermentConfig* LimbConfig = DismembermentConfigData->DismembermentConfigs.Find(BoneName);
	if (LimbConfig == nullptr || !LimbConfig->Valid())
	{
		return false;
	}
	UStaticMesh* SelectedStaticMesh = nullptr;
	USkeletalMesh* SelectedSkeletalMesh = nullptr;
	if (DismembermentConfigData->LimbMeshType == EDismembermentLimbMeshType::StaticMesh)
	{
		SelectedStaticMesh = LimbConfig->GetRandomDismembermentMesh();
		if (!IsValid(SelectedStaticMesh)) return false;
	}
	else
	{
		SelectedSkeletalMesh = LimbConfig->GetRandomDismembermentSkeletalMesh();
		if (!IsValid(SelectedSkeletalMesh)) return false;
	}

	// Preserve the independent modular mesh's world transform before destroying it.
	const FTransform DismembermentWorldTransform = DismembermentConfig.bHideWholeTargetMesh
		? TargetMesh->GetComponentTransform()
		: TargetMesh->GetBoneTransform(BoneName);

	ADismembermentLimbActor* LimbActor = GetWorld()->SpawnActor<ADismembermentLimbActor>(
		ADismembermentLimbActor::StaticClass(), DismembermentWorldTransform);
	if (!IsValid(LimbActor))
	{
		return false;
	}

	if (DismembermentConfig.bHideWholeTargetMesh)
	{
		// This mode is intended for independent modular limb meshes. Do not use it on the Leader Pose mesh.
		TargetMesh->DestroyComponent();
	}
	else
	{
		TargetMesh->HideBoneByName(BoneName, PBO_Term);
	}

	LimbActor->SetLifeSpan(DetachedLimbLifeSpan);

	FVector FinalImpactDirection;
	switch (DismembermentConfig.LimbSpace)
	{
	case ELimbSpace::WorldSpace:
		FinalImpactDirection = DismembermentConfig.ImpactDirection;
		break;
	case ELimbSpace::BoneSpace:
		FinalImpactDirection = DismembermentWorldTransform.TransformVector(DismembermentConfig.ImpactDirection);
		break;
	default:
		checkNoEntry();
		break;
	}

	LimbActor->ReInitializeLimb(DismembermentWorldTransform,
		DismembermentConfigData->LimbMeshType,
		SelectedStaticMesh,
		SelectedSkeletalMesh,
		DismembermentConfigData->DismembermentLimbCollisionProfile.Name);
	LimbActor->SetPhysicalSimulationAvoidance(PhysicalSimulationAvoidanceTime,
		PhysicalSimulationAvoidanceCollisionProfileName.Name);
	LimbActor->SetLimbPhysicsSimulationTime(LimbPhysicsSimulationTime);
	LimbActor->AddImpulse(FinalImpactDirection, DismembermentConfig.ImpactForce,
		DismembermentConfig.AngularImpulseDegrees);

	return true;
}

