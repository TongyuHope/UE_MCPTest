#include "DetachedLib/AnimNotify_ApplyDismemberment.h"

#include "Components/SkeletalMeshComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


UAnimNotify_ApplyDismemberment::UAnimNotify_ApplyDismemberment()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(197, 23, 46);
#endif
}

void UAnimNotify_ApplyDismemberment::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp)) return;
	const AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	UDismembermentComponent* DestructionComponent = Owner->FindComponentByClass<UDismembermentComponent>();
	if (!IsValid(DestructionComponent))
	{
		return;
	}

	for (const auto& ApplyDismembermentConfig : ApplyDismembermentConfigs)
	{
		FDismembermentConfig Config = ApplyDismembermentConfig.Value;
		Config.bHideWholeTargetMesh |= bHideWholeTargetMesh;
		DestructionComponent->ApplyDismemberment(ApplyDismembermentConfig.Key, Config);
	}
}

#if WITH_EDITOR
EDataValidationResult UAnimNotify_ApplyDismemberment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (ApplyDismembermentConfigs.Num() == 0)
	{
		Context.AddError(NSLOCTEXT("AnimNotify_ApplyDismemberment", "DismembermentConfigsEmpty",
			"DismembermentConfigs is empty. Please add at least one Dismemberment config."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

FString UAnimNotify_ApplyDismemberment::GetNotifyName_Implementation() const
{
	return TEXT("Apply Dismemberment");
}
#endif

