#include "DetachedLib/DismembermentComponent.h"

#include "DetachedLib/DismembermentLimbActor.h"
#include "DetachedLib/DismembermentLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	bool IsValidMeshTypeOverride(EDismembermentLimbMeshTypeOverride Type)
	{
		return Type == EDismembermentLimbMeshTypeOverride::UseAssetDefault || Type == EDismembermentLimbMeshTypeOverride::StaticMesh ||
			Type == EDismembermentLimbMeshTypeOverride::SkeletalMesh;
	}
}

UDismembermentComponent::UDismembermentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDismembermentComponent::BeginPlay()
{
	Super::BeginPlay();
	DrivenSkeletalMeshComponent = ResolveDrivenMesh();
	if (!IsValid(DrivenSkeletalMeshComponent))
		ReportFailure(NAME_None, DrivenSkeletalMeshComponentName, TEXT("Driven mesh could not be resolved. Check its runtime object name."));
}

USkeletalMeshComponent* UDismembermentComponent::FindMeshByName(FName ComponentName) const
{
	if (!IsValid(GetOwner())) return nullptr;
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
		if (IsValid(Mesh) && Mesh->GetFName() == ComponentName) return Mesh;
	return nullptr;
}

USkeletalMeshComponent* UDismembermentComponent::ResolveDrivenMesh() const
{
	if (!IsValid(GetOwner())) return nullptr;
	if (!DrivenSkeletalMeshComponentName.IsNone())
	{
		if (IsValid(DrivenSkeletalMeshComponent) && DrivenSkeletalMeshComponent->GetOwner() == GetOwner() &&
			DrivenSkeletalMeshComponent->GetFName() == DrivenSkeletalMeshComponentName) return DrivenSkeletalMeshComponent;
		return FindMeshByName(DrivenSkeletalMeshComponentName);
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner())) return Character->GetMesh();
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	Meshes.RemoveAll([](USkeletalMeshComponent* Mesh) { return !IsValid(Mesh); });
	return Meshes.Num() == 1 ? Meshes[0] : nullptr;
}

bool UDismembermentComponent::ReportFailure(FName BoneName, FName TargetName, const FString& Reason) const
{
	UE_LOG(LogDismemberment, Warning, TEXT("Owner=%s Part=%s Target=%s: %s"),
		*GetNameSafe(GetOwner()), *BoneName.ToString(), *TargetName.ToString(), *Reason);
	return false;
}

EDismembermentLimbMeshType UDismembermentComponent::GetResolvedLimbMeshType() const
{
	switch (LimbMeshTypeOverride)
	{
	case EDismembermentLimbMeshTypeOverride::StaticMesh: return EDismembermentLimbMeshType::StaticMesh;
	case EDismembermentLimbMeshTypeOverride::SkeletalMesh: return EDismembermentLimbMeshType::SkeletalMesh;
	default: return IsValid(DismembermentConfigData) ? DismembermentConfigData->LimbMeshType : EDismembermentLimbMeshType::StaticMesh;
	}
}

bool UDismembermentComponent::ValidateTarget(FName BoneName, const FDismembermentConfig& Config,
	USkeletalMeshComponent*& OutMesh, FString& OutError) const
{
	OutError.Reset();
	// Verify the cache's lifetime/name; resolve again after replacement or name changes.
	USkeletalMeshComponent* DrivenMesh = ResolveDrivenMesh();
	OutMesh = Config.TargetSkeletalMeshComponentName.IsNone() ? DrivenMesh : FindMeshByName(Config.TargetSkeletalMeshComponentName);
	if (!IsValid(OutMesh) || !IsValid(OutMesh->GetSkeletalMeshAsset()))
	{
		OutError = TEXT("Target skeletal mesh component/asset could not be resolved.");
		return false;
	}
	if (Config.GetRemovalMode() == ELimbRemovalMode::DestroyWholeMeshComponent)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Config.TargetSkeletalMeshComponentName.IsNone() || !IsValid(DrivenMesh) || OutMesh == DrivenMesh ||
			(Character && OutMesh == Character->GetMesh()))
		{
			OutError = TEXT("Whole-mesh removal requires a resolved driven mesh and an independent target; the character's main mesh is protected.");
			return false;
		}
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->LeaderPoseComponent.Get() == OutMesh)
			{
				OutError = TEXT("Cannot destroy a mesh that is the Leader Pose source of another component.");
				return false;
			}
		}
	}
	else if (OutMesh->LeaderPoseComponent.IsValid())
	{
		OutError = TEXT("Bone hiding must target the pose-driving mesh, not a Leader Pose follower. Use whole-mesh removal for independent follower limbs.");
		return false;
	}
	else if (OutMesh->GetBoneIndex(BoneName) == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Bone '%s' does not exist in target asset '%s'."), *BoneName.ToString(), *OutMesh->GetSkeletalMeshAsset()->GetName());
		return false;
	}
	return true;
}

bool UDismembermentComponent::ApplyRegisteredDismemberment(const FDismembermentRequest& Request)
{
	if (!IsValid(DismembermentConfigData)) return ReportFailure(Request.BoneName, NAME_None, TEXT("DismembermentConfigData is not assigned."));
	FDismembermentConfig Config;
	FString Error;
	if (!DismembermentConfigData->ResolveRequest(Request, Config, Error)) return ReportFailure(Request.BoneName, NAME_None, Error);
	return ApplyDismemberment(Request.BoneName, Config);
}

int32 UDismembermentComponent::ApplyDismembermentRequests(const TArray<FDismembermentRequest>& Requests)
{
	int32 SuccessCount = 0;
	for (const FDismembermentRequest& Request : Requests) SuccessCount += ApplyRegisteredDismemberment(Request) ? 1 : 0;
	return SuccessCount;
}

bool UDismembermentComponent::ApplyDismemberment(const FName& BoneName, const FDismembermentConfig& Config)
{
	auto Fail = [this, &BoneName, &Config](const FString& Error) { return ReportFailure(BoneName, Config.TargetSkeletalMeshComponentName, Error); };
	if (!IsValid(GetOwner()) || !IsValid(GetWorld())) return Fail(TEXT("Owner or World is invalid."));
	if (BoneName.IsNone()) return Fail(TEXT("Part name cannot be None."));
	if (!IsValid(DismembermentConfigData)) return Fail(TEXT("DismembermentConfigData is not assigned."));
	FString Error;
	if (!UDismembermentConfigData::ValidateConfig(Config, Error)) return Fail(Error);
	if (!IsValidMeshTypeOverride(LimbMeshTypeOverride)) return Fail(TEXT("Invalid LimbMeshTypeOverride."));
	USkeletalMeshComponent* TargetMesh = nullptr;
	if (!ValidateTarget(BoneName, Config, TargetMesh, Error)) return Fail(Error);
	const FSKMDismembermentConfig* Part = DismembermentConfigData->DismembermentConfigs.Find(BoneName);
	if (!Part) return Fail(FString::Printf(TEXT("Part is not registered in '%s'."), *DismembermentConfigData->GetName()));
	const EDismembermentLimbMeshType MeshType = GetResolvedLimbMeshType();
	if (MeshType != EDismembermentLimbMeshType::StaticMesh && MeshType != EDismembermentLimbMeshType::SkeletalMesh) return Fail(TEXT("Invalid asset LimbMeshType."));
	if (!Part->ValidForMeshType(MeshType))
	{
		return Fail(FString::Printf(TEXT("No valid %s candidates. Fill %s; there is no automatic type fallback."),
			MeshType == EDismembermentLimbMeshType::StaticMesh ? TEXT("StaticMesh") : TEXT("SkeletalMesh"),
			MeshType == EDismembermentLimbMeshType::StaticMesh ? TEXT("DismembermentMeshes") : TEXT("DismembermentSkeletalMeshes")));
	}
	if (!FMath::IsFinite(PhysicalSimulationAvoidanceTime) || !FMath::IsFinite(LimbPhysicsSimulationTime) ||
		!FMath::IsFinite(DetachedLimbLifeSpan) || !FMath::IsFinite(PhysicalSimulationAvoidanceTime + LimbPhysicsSimulationTime) ||
		PhysicalSimulationAvoidanceTime < 0 || LimbPhysicsSimulationTime < 0 || DetachedLimbLifeSpan < 0)
		return Fail(TEXT("Physics times and LifeSpan must be finite and non-negative."));
	if (!UDismembermentConfigData::ValidateCollisionProfile(DismembermentConfigData->DismembermentLimbCollisionProfile.Name, Error) ||
		!UDismembermentConfigData::ValidateCollisionProfile(PhysicalSimulationAvoidanceCollisionProfileName.Name, Error)) return Fail(Error);
	UStaticMesh* StaticMesh = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	if (MeshType == EDismembermentLimbMeshType::StaticMesh)
	{
		StaticMesh = Part->GetRandomDismembermentMesh();
		if (!UDismembermentConfigData::ValidateMesh(StaticMesh, Error)) return Fail(Error);
	}
	else
	{
		SkeletalMesh = Part->GetRandomDismembermentSkeletalMesh();
		if (!UDismembermentConfigData::ValidateMesh(SkeletalMesh, Error)) return Fail(Error);
	}
	const bool bDestroyMesh = Config.GetRemovalMode() == ELimbRemovalMode::DestroyWholeMeshComponent;
	const FTransform CutTransform = bDestroyMesh ? TargetMesh->GetComponentTransform() : TargetMesh->GetBoneTransform(BoneName);
	FTransform SpawnTransform = CutTransform;
	if (!Config.bUseWorldScale) SpawnTransform.SetScale3D(FVector::OneVector);
	if (SpawnTransform.ContainsNaN()) return Fail(TEXT("Cut transform contains invalid values."));
	auto ToWorld = [this, &CutTransform](const FVector& Vector, ELimbSpace Space, bool bApplyScale)
	{
		if (Space == ELimbSpace::BoneSpace) return bApplyScale ? CutTransform.TransformVector(Vector) : CutTransform.TransformVectorNoScale(Vector);
		if (Space == ELimbSpace::CharacterSpace) return GetOwner()->GetActorTransform().TransformVectorNoScale(Vector);
		return Vector;
	};
	const FVector Direction = ToWorld(Config.ImpactDirection, Config.LimbSpace, true);
	const FVector AngularImpulse = ToWorld(Config.AngularImpulseDegrees, Config.AngularImpulseSpace, false);
	if (Direction.ContainsNaN() || AngularImpulse.ContainsNaN()) return Fail(TEXT("World-space impact contains invalid values."));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	ADismembermentLimbActor* LimbActor = GetWorld()->SpawnActor<ADismembermentLimbActor>(
		ADismembermentLimbActor::StaticClass(), SpawnTransform, SpawnParameters);
	if (!IsValid(LimbActor)) return Fail(TEXT("Failed to spawn the detached limb Actor."));
	LimbActor->ReInitializeLimb(SpawnTransform, MeshType, StaticMesh, SkeletalMesh, DismembermentConfigData->DismembermentLimbCollisionProfile.Name);
	LimbActor->SetPhysicalSimulationAvoidance(PhysicalSimulationAvoidanceTime, PhysicalSimulationAvoidanceCollisionProfileName.Name);
	LimbActor->SetLimbPhysicsSimulationTime(LimbPhysicsSimulationTime);
	if (!LimbActor->AddImpulse(Direction, Config.ImpactForce, AngularImpulse))
	{
		LimbActor->Destroy();
		return Fail(TEXT("Limb physics did not start. Host removal was cancelled."));
	}
	// Only remove the host after the detached model and physics are ready.
	if (bDestroyMesh) TargetMesh->DestroyComponent();
	else TargetMesh->HideBoneByName(BoneName, PBO_Term);
	LimbActor->SetLifeSpan(DetachedLimbLifeSpan);
	return true;
}

void UDismembermentComponent::GetDismembermentDiagnostics(TArray<FString>& OutErrors) const
{
	CollectDismembermentDiagnostics(OutErrors, true);
}

void UDismembermentComponent::CollectDismembermentDiagnostics(TArray<FString>& OutErrors, bool bValidateHost) const
{
	if (!IsValid(DismembermentConfigData))
	{
		OutErrors.Add(TEXT("DismembermentConfigData is not assigned."));
		return;
	}
	FString Error;
	if (bValidateHost && !IsValid(ResolveDrivenMesh())) OutErrors.Add(TEXT("Driven mesh could not be resolved; check its object name or ambiguous automatic selection."));
	if (!UDismembermentConfigData::ValidateCollisionProfile(PhysicalSimulationAvoidanceCollisionProfileName.Name, Error)) OutErrors.Add(Error);
	if (!UDismembermentConfigData::ValidateCollisionProfile(DismembermentConfigData->DismembermentLimbCollisionProfile.Name, Error)) OutErrors.Add(Error);
	if (!IsValidMeshTypeOverride(LimbMeshTypeOverride)) OutErrors.Add(TEXT("Invalid LimbMeshTypeOverride."));
	if (!FMath::IsFinite(PhysicalSimulationAvoidanceTime) || !FMath::IsFinite(LimbPhysicsSimulationTime) || !FMath::IsFinite(DetachedLimbLifeSpan) ||
		!FMath::IsFinite(PhysicalSimulationAvoidanceTime + LimbPhysicsSimulationTime) ||
		PhysicalSimulationAvoidanceTime < 0 || LimbPhysicsSimulationTime < 0 || DetachedLimbLifeSpan < 0) OutErrors.Add(TEXT("Physics times and LifeSpan must be finite and non-negative."));
	if (DismembermentConfigData->DismembermentConfigs.IsEmpty()) OutErrors.Add(TEXT("DismembermentConfigs is empty."));
	const EDismembermentLimbMeshType MeshType = GetResolvedLimbMeshType();
	if (MeshType != EDismembermentLimbMeshType::StaticMesh && MeshType != EDismembermentLimbMeshType::SkeletalMesh) OutErrors.Add(TEXT("Invalid asset LimbMeshType."));
	for (const auto& Entry : DismembermentConfigData->DismembermentConfigs)
	{
		const FString Prefix = FString::Printf(TEXT("Part '%s': "), *Entry.Key.ToString());
		if (Entry.Key.IsNone()) OutErrors.Add(Prefix + TEXT("Part name cannot be None."));
		if (!Entry.Value.ValidForMeshType(MeshType)) OutErrors.Add(Prefix + TEXT("No candidates for the component's resolved mesh type."));
		USkeletalMeshComponent* Target = nullptr;
		if (!UDismembermentConfigData::ValidateConfig(Entry.Value.DefaultConfig, Error) ||
			(bValidateHost && !ValidateTarget(Entry.Key, Entry.Value.DefaultConfig, Target, Error))) OutErrors.Add(Prefix + Error);
		for (const auto& Preset : Entry.Value.ImpactPresets)
		{
			FDismembermentConfig ImpactOnly;
			Preset.Value.ApplyTo(ImpactOnly);
			if (Preset.Key.IsNone() || !UDismembermentConfigData::ValidateConfig(ImpactOnly, Error))
				OutErrors.Add(Prefix + TEXT("Preset '") + Preset.Key.ToString() + TEXT("': ") + (Preset.Key.IsNone() ? TEXT("Preset name cannot be None.") : Error));
		}
		if (MeshType == EDismembermentLimbMeshType::StaticMesh)
		{
			for (UStaticMesh* Mesh : Entry.Value.DismembermentMeshes)
				if (IsValid(Mesh) && !UDismembermentConfigData::ValidateMesh(Mesh, Error)) OutErrors.Add(Prefix + Error);
		}
		else
		{
			for (USkeletalMesh* Mesh : Entry.Value.DismembermentSkeletalMeshes)
				if (IsValid(Mesh) && !UDismembermentConfigData::ValidateMesh(Mesh, Error)) OutErrors.Add(Prefix + Error);
		}
	}
}

void UDismembermentComponent::DumpDismembermentDiagnostics() const
{
	TArray<FString> Errors;
	GetDismembermentDiagnostics(Errors);
	for (const FString& Error : Errors) ReportFailure(NAME_None, DrivenSkeletalMeshComponentName, Error);
	UE_LOG(LogDismemberment, Display, TEXT("Owner=%s Asset=%s MeshType=%s Diagnostics=%d issues"), *GetNameSafe(GetOwner()),
		*GetNameSafe(DismembermentConfigData), *UEnum::GetValueAsString(GetResolvedLimbMeshType()), Errors.Num());
}

#if WITH_EDITOR
EDataValidationResult UDismembermentComponent::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FString> Errors;
	// Blueprint compilation validates SCS/archetype components before there is a
	// constructed Actor. CDO component lists also omit Blueprint-created meshes.
	// Only a concrete owner can supply reliable bones and Leader Pose relationships.
	const AActor* Owner = GetOwner();
	const bool bValidateHost = !IsTemplate() && IsValid(Owner) && !Owner->IsTemplate();
	CollectDismembermentDiagnostics(Errors, bValidateHost);
	for (const FString& Error : Errors) Context.AddError(FText::FromString(Error));
	return Errors.IsEmpty() && SuperResult != EDataValidationResult::Invalid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

