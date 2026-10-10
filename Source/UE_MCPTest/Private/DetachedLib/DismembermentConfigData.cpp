#include "DetachedLib/DismembermentConfigData.h"

#include "DetachedLib/DismembermentLog.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY(LogDismemberment);

namespace
{
	bool IsValidImpactSpace(ELimbSpace Space)
	{
		return Space == ELimbSpace::WorldSpace || Space == ELimbSpace::BoneSpace || Space == ELimbSpace::CharacterSpace;
	}
	void CopyImpact(const FDismembermentConfig& From, FDismembermentConfig& To)
	{
		To.ImpactDirection = From.ImpactDirection;
		To.ImpactForce = From.ImpactForce;
		To.AngularImpulseDegrees = From.AngularImpulseDegrees;
		To.LimbSpace = From.LimbSpace;
		To.AngularImpulseSpace = From.AngularImpulseSpace;
	}
}

bool FSKMDismembermentConfig::ValidForMeshType(EDismembermentLimbMeshType MeshType) const
{
	if (MeshType == EDismembermentLimbMeshType::StaticMesh)
		return DismembermentMeshes.ContainsByPredicate([](const TObjectPtr<UStaticMesh>& Mesh) { return IsValid(Mesh); });
	if (MeshType == EDismembermentLimbMeshType::SkeletalMesh)
		return DismembermentSkeletalMeshes.ContainsByPredicate([](const TObjectPtr<USkeletalMesh>& Mesh) { return IsValid(Mesh); });
	return false;
}

bool UDismembermentConfigData::ResolveRequest(const FDismembermentRequest& Request,
	FDismembermentConfig& OutConfig, FString& OutError) const
{
	OutError.Reset();
	const FSKMDismembermentConfig* Part = DismembermentConfigs.Find(Request.BoneName);
	if (!Part)
	{
		OutError = FString::Printf(TEXT("Part '%s' is not registered in %s."), *Request.BoneName.ToString(), *GetName());
		return false;
	}
	FDismembermentConfig Resolved = Part->DefaultConfig;
	if (!Request.PresetName.IsNone())
	{
		const FDismembermentImpactPreset* Preset = Part->ImpactPresets.Find(Request.PresetName);
		if (!Preset)
		{
			OutError = FString::Printf(TEXT("Preset '%s' is not registered for part '%s' in %s."),
				*Request.PresetName.ToString(), *Request.BoneName.ToString(), *GetName());
			return false;
		}
		Preset->ApplyTo(Resolved);
	}
	if (Request.bOverrideImpact) CopyImpact(Request.OverrideConfig, Resolved);
	if (Request.bOverrideRemovalMode)
	{
		Resolved.RemovalMode = Request.OverrideConfig.GetRemovalMode();
		Resolved.bHideWholeTargetMesh = false;
	}
	if (Request.bOverrideTargetMesh) Resolved.TargetSkeletalMeshComponentName = Request.OverrideConfig.TargetSkeletalMeshComponentName;
	if (Request.bOverrideWorldScale) Resolved.bUseWorldScale = Request.OverrideConfig.bUseWorldScale;
	if (!ValidateConfig(Resolved, OutError)) return false;
	OutConfig = Resolved;
	return true;
}

bool UDismembermentConfigData::ValidateConfig(const FDismembermentConfig& Config, FString& OutError)
{
	OutError.Reset();
	if (Config.ImpactDirection.ContainsNaN() || Config.AngularImpulseDegrees.ContainsNaN() || !FMath::IsFinite(Config.ImpactForce))
	{
		OutError = TEXT("Impact direction, force and angular impulse must be finite.");
		return false;
	}
	if (!IsValidImpactSpace(Config.LimbSpace) || !IsValidImpactSpace(Config.AngularImpulseSpace) ||
		(Config.RemovalMode != ELimbRemovalMode::HideBone && Config.RemovalMode != ELimbRemovalMode::DestroyWholeMeshComponent))
	{
		OutError = TEXT("Invalid impact space or removal mode.");
		return false;
	}
	if (Config.GetRemovalMode() == ELimbRemovalMode::DestroyWholeMeshComponent && Config.TargetSkeletalMeshComponentName.IsNone())
	{
		OutError = TEXT("DestroyWholeMeshComponent requires an explicit independent target component name.");
		return false;
	}
	return true;
}

bool UDismembermentConfigData::ValidateCollisionProfile(FName ProfileName, FString& OutError)
{
	OutError.Reset();
	FCollisionResponseTemplate Profile;
	if (!UCollisionProfile::Get()->GetProfileTemplate(ProfileName, Profile))
	{
		OutError = FString::Printf(TEXT("Collision profile '%s' does not exist."), *ProfileName.ToString());
		return false;
	}
	if (Profile.CollisionEnabled != ECollisionEnabled::QueryAndPhysics && Profile.CollisionEnabled != ECollisionEnabled::PhysicsOnly)
	{
		OutError = FString::Printf(TEXT("Collision profile '%s' must enable physics collision."), *ProfileName.ToString());
		return false;
	}
	return true;
}

bool UDismembermentConfigData::ValidateMesh(UStaticMesh* Mesh, FString& OutError)
{
	OutError.Reset();
	if (!IsValid(Mesh)) { OutError = TEXT("StaticMesh candidate is invalid."); return false; }
	const UBodySetup* Body = Mesh->GetBodySetup();
	if (!IsValid(Body) || Body->AggGeom.GetElementCount() == 0 || Body->CollisionTraceFlag == CTF_UseComplexAsSimple)
	{
		OutError = FString::Printf(TEXT("StaticMesh '%s' needs simple collision for physics simulation (ComplexAsSimple is unsupported)."), *Mesh->GetName());
		return false;
	}
	return true;
}

bool UDismembermentConfigData::ValidateMesh(USkeletalMesh* Mesh, FString& OutError)
{
	OutError.Reset();
	if (!IsValid(Mesh)) { OutError = TEXT("SkeletalMesh candidate is invalid."); return false; }
	const UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset();
	if (IsValid(PhysicsAsset))
	{
		for (const USkeletalBodySetup* Body : PhysicsAsset->SkeletalBodySetups)
		{
			if (IsValid(Body) && Body->AggGeom.GetElementCount() > 0 && Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName) != INDEX_NONE) return true;
		}
	}
	OutError = FString::Printf(TEXT("SkeletalMesh '%s' needs a Physics Asset with a collision body matching its skeleton."), *Mesh->GetName());
	return false;
}

void UDismembermentConfigData::GetConfigurationErrors(TArray<FString>& OutErrors) const
{
	FString Error;
	if (DismembermentConfigs.IsEmpty()) OutErrors.Add(TEXT("DismembermentConfigs is empty."));
	if (LimbMeshType != EDismembermentLimbMeshType::StaticMesh && LimbMeshType != EDismembermentLimbMeshType::SkeletalMesh) OutErrors.Add(TEXT("Invalid LimbMeshType."));
	if (!ValidateCollisionProfile(DismembermentLimbCollisionProfile.Name, Error)) OutErrors.Add(Error);
	for (const auto& Entry : DismembermentConfigs)
	{
		const FString Prefix = FString::Printf(TEXT("Part '%s': "), *Entry.Key.ToString());
		if (Entry.Key.IsNone()) OutErrors.Add(Prefix + TEXT("Part name cannot be None."));
		if (!Entry.Value.ValidForMeshType(LimbMeshType)) OutErrors.Add(Prefix + TEXT("No valid candidates for the asset's default LimbMeshType."));
		if (!ValidateConfig(Entry.Value.DefaultConfig, Error)) OutErrors.Add(Prefix + Error);
		for (const auto& Preset : Entry.Value.ImpactPresets)
		{
			if (Preset.Key.IsNone()) OutErrors.Add(Prefix + TEXT("Preset name cannot be None."));
			FDismembermentConfig ImpactOnly;
			Preset.Value.ApplyTo(ImpactOnly);
			if (!ValidateConfig(ImpactOnly, Error)) OutErrors.Add(Prefix + Preset.Key.ToString() + TEXT(": ") + Error);
		}
		for (UStaticMesh* Mesh : Entry.Value.DismembermentMeshes)
			if (IsValid(Mesh) && !ValidateMesh(Mesh, Error)) OutErrors.Add(Prefix + Error);
		for (USkeletalMesh* Mesh : Entry.Value.DismembermentSkeletalMeshes)
			if (IsValid(Mesh) && !ValidateMesh(Mesh, Error)) OutErrors.Add(Prefix + Error);
	}
}

#if WITH_EDITOR
EDataValidationResult UDismembermentConfigData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FString> Errors;
	GetConfigurationErrors(Errors);
	for (const FString& Error : Errors) Context.AddError(FText::FromString(Error));
	return Errors.IsEmpty() && SuperResult != EDataValidationResult::Invalid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

