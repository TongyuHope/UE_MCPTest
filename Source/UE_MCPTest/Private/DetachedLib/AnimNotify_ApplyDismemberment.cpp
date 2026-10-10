#include "DetachedLib/AnimNotify_ApplyDismemberment.h"

#include "DetachedLib/DismembermentLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UAnimNotify_ApplyDismemberment::UAnimNotify_ApplyDismemberment()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(197, 23, 46);
#endif
}

void UAnimNotify_ApplyDismemberment::PostLoad()
{
	Super::PostLoad();
	MigrateLegacyConfigurations();
}

void UAnimNotify_ApplyDismemberment::MigrateLegacyConfigurations()
{
	if (ApplyDismembermentConfigs.IsEmpty()) return;
	const int32 LegacyCount = ApplyDismembermentConfigs.Num();
	TArray<FName> Keys;
	ApplyDismembermentConfigs.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	for (FName BoneName : Keys)
	{
		if (DismembermentRequests.ContainsByPredicate([BoneName](const FDismembermentRequest& Request) { return Request.BoneName == BoneName; }))
		{
			UE_LOG(LogDismemberment, Warning, TEXT("Notify=%s Part=%s: explicit new request takes precedence over legacy entry."), *GetPathName(), *BoneName.ToString());
			continue;
		}
		FDismembermentRequest Request;
		Request.BoneName = BoneName;
		Request.bOverrideImpact = true;
		Request.bOverrideRemovalMode = true;
		Request.bOverrideTargetMesh = true;
		Request.bOverrideWorldScale = true;
		Request.OverrideConfig = ApplyDismembermentConfigs.FindChecked(BoneName);
		Request.OverrideConfig.bHideWholeTargetMesh |= bHideWholeTargetMesh;
		Request.OverrideConfig.RemovalMode = Request.OverrideConfig.GetRemovalMode();
		Request.OverrideConfig.bHideWholeTargetMesh = false;
		// The old scale switch was never read; preserve the old inherited-scale behavior.
		Request.OverrideConfig.bUseWorldScale = true;
		DismembermentRequests.Add(Request);
	}
	ApplyDismembermentConfigs.Reset();
	bHideWholeTargetMesh = false;
	UE_LOG(LogDismemberment, Display, TEXT("Notify=%s: migrated %d legacy entries to explicit requests. Save the animation asset to persist migration."),
		*GetPathName(), LegacyCount);
}

TArray<FString> UAnimNotify_ApplyDismemberment::GetBoneNameOptions() const
{
	TArray<FString> Options;
#if WITH_EDITORONLY_DATA
	if (IsValid(AuthoringConfigData))
		for (const auto& Part : AuthoringConfigData->DismembermentConfigs) Options.Add(Part.Key.ToString());
#endif
	Options.Sort();
	return Options;
}

TArray<FString> UAnimNotify_ApplyDismemberment::GetPresetNameOptions() const
{
	TArray<FString> Options;
#if WITH_EDITORONLY_DATA
	if (IsValid(AuthoringConfigData))
		for (const auto& Part : AuthoringConfigData->DismembermentConfigs)
			for (const auto& Preset : Part.Value.ImpactPresets) Options.AddUnique(Preset.Key.ToString());
#endif
	Options.Sort();
	Options.Insert(FName(NAME_None).ToString(), 0);
	return Options;
}

void UAnimNotify_ApplyDismemberment::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp) || !IsValid(MeshComp->GetOwner())) return;
	UDismembermentComponent* Component = MeshComp->GetOwner()->FindComponentByClass<UDismembermentComponent>();
	if (!IsValid(Component))
	{
		// Animation previews have no gameplay component; avoid spamming their Output Log.
		const UWorld* World = MeshComp->GetWorld();
		if (World && (World->WorldType == EWorldType::EditorPreview || World->WorldType == EWorldType::Editor)) return;
		UE_LOG(LogDismemberment, Warning, TEXT("Animation=%s Owner=%s: no DismembermentComponent."), *GetNameSafe(Animation), *GetNameSafe(MeshComp->GetOwner()));
		return;
	}
	// Also supports old maps set directly from C++ after construction.
	MigrateLegacyConfigurations();
	if (DismembermentRequests.IsEmpty())
	{
		UE_LOG(LogDismemberment, Warning, TEXT("Animation=%s Owner=%s: no dismemberment requests configured."), *GetNameSafe(Animation), *GetNameSafe(MeshComp->GetOwner()));
		return;
	}
	const int32 SuccessCount = Component->ApplyDismembermentRequests(DismembermentRequests);
	if (SuccessCount != DismembermentRequests.Num())
		UE_LOG(LogDismemberment, Warning, TEXT("Animation=%s Owner=%s: %d/%d dismemberment requests succeeded."),
			*GetNameSafe(Animation), *GetNameSafe(MeshComp->GetOwner()), SuccessCount, DismembermentRequests.Num());
}

#if WITH_EDITOR
EDataValidationResult UAnimNotify_ApplyDismemberment::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bValid = SuperResult != EDataValidationResult::Invalid;
	auto AddError = [&Context, &bValid](const FString& Error) { Context.AddError(FText::FromString(Error)); bValid = false; };
	if (DismembermentRequests.IsEmpty() && ApplyDismembermentConfigs.IsEmpty())
		AddError(TEXT("Add at least one dismemberment request."));
	if (!IsValid(AuthoringConfigData))
	{
		// Old notifications have no authoring asset. Runtime still uses the owner's asset.
		Context.AddWarning(FText::FromString(TEXT("Authoring Config Data is not assigned; part/preset dropdown and asset cross-checks are unavailable.")));
	}
	TSet<FName> SeenParts;
	for (const FDismembermentRequest& Request : DismembermentRequests)
	{
		if (Request.BoneName.IsNone()) AddError(TEXT("Part name cannot be None."));
		if (SeenParts.Contains(Request.BoneName)) AddError(FString::Printf(TEXT("Part '%s' is repeated in the same notification."), *Request.BoneName.ToString()));
		SeenParts.Add(Request.BoneName);
		FString Error;
		if (IsValid(AuthoringConfigData))
		{
			FDismembermentConfig Config;
			if (!AuthoringConfigData->ResolveRequest(Request, Config, Error)) AddError(Error);
		}
		else
		{
			// Without a host/asset, validate only the parameters this request overrides.
			FDismembermentConfig Config;
			if (Request.bOverrideImpact)
			{
				Config.ImpactDirection = Request.OverrideConfig.ImpactDirection;
				Config.ImpactForce = Request.OverrideConfig.ImpactForce;
				Config.AngularImpulseDegrees = Request.OverrideConfig.AngularImpulseDegrees;
				Config.LimbSpace = Request.OverrideConfig.LimbSpace;
				Config.AngularImpulseSpace = Request.OverrideConfig.AngularImpulseSpace;
			}
			if (Request.bOverrideRemovalMode) Config.RemovalMode = Request.OverrideConfig.GetRemovalMode();
			if (Request.bOverrideTargetMesh) Config.TargetSkeletalMeshComponentName = Request.OverrideConfig.TargetSkeletalMeshComponentName;
			// Target may come from a runtime asset if it is not explicitly overridden.
			if (!Request.bOverrideTargetMesh) Config.RemovalMode = ELimbRemovalMode::HideBone;
			if (!UDismembermentConfigData::ValidateConfig(Config, Error)) AddError(Error);
		}
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

FString UAnimNotify_ApplyDismemberment::GetNotifyName_Implementation() const
{
	return TEXT("Apply Dismemberment");
}
#endif

