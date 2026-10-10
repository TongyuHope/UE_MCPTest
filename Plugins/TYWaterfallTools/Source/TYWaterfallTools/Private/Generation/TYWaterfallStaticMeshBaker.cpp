// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/TYWaterfallStaticMeshBaker.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "ContentBrowserModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "UDynamicMesh.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "TYWaterfallStaticMeshBaker"

namespace
{
void ShowBakeNotification(const FText& Message, SNotificationItem::ECompletionState State)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 4.0f;
	Info.bUseLargeFont = false;
	if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(State);
	}
}

void ApplyWaterfallMaterials(UStaticMesh& StaticMesh, const UTYWaterfallSettingsComponent& Settings)
{
	static const FName SlotNames[] = {
		TEXT("Singular"), TEXT("PerPath"), TEXT("Cross"), TEXT("Splash")
	};
	UMaterialInterface* Materials[] = {
		Settings.GetSingularMaterial(), Settings.GetPerPathMaterial(),
		Settings.GetCrossMaterial(), Settings.GetSplashMaterial()
	};

	TArray<FStaticMaterial>& StaticMaterials = StaticMesh.GetStaticMaterials();
	while (StaticMaterials.Num() < UE_ARRAY_COUNT(SlotNames))
	{
		StaticMaterials.Add(FStaticMaterial());
	}
	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(SlotNames); ++SlotIndex)
	{
		StaticMaterials[SlotIndex].MaterialInterface = Materials[SlotIndex];
		StaticMaterials[SlotIndex].MaterialSlotName = SlotNames[SlotIndex];
		StaticMaterials[SlotIndex].ImportedMaterialSlotName = SlotNames[SlotIndex];
	}

	StaticMesh.MarkPackageDirty();
	StaticMesh.PostEditChange();
}
}

bool FTYWaterfallStaticMeshBaker::Bake(ATYWaterfallActor& Waterfall)
{
	UTYWaterfallMeshComponent* MeshComponent = Waterfall.GetDynamicMeshComponent();
	UTYWaterfallSettingsComponent* Settings = Waterfall.GetWaterfallSettings();
	UDynamicMesh* DynamicMesh = IsValid(MeshComponent) ? MeshComponent->GetDynamicMesh() : nullptr;
	if (!IsValid(DynamicMesh) || DynamicMesh->GetTriangleCount() == 0 || !IsValid(Settings))
	{
		ShowBakeNotification(
			LOCTEXT("MissingMesh", "Generate a non-empty waterfall mesh before baking."),
			SNotificationItem::CS_Fail);
		return false;
	}

	FSaveAssetDialogConfig DialogConfig;
	DialogConfig.DialogTitleOverride = LOCTEXT("SaveDialogTitle", "Save Waterfall Static Mesh");
	DialogConfig.DefaultPath = TEXT("/Game/WaterfallMeshes");
	DialogConfig.DefaultAssetName = FString::Printf(TEXT("SM_%s"),
		*ObjectTools::SanitizeObjectName(Waterfall.GetActorNameOrLabel()));
	DialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	DialogConfig.AssetClassNames.Add(UStaticMesh::StaticClass()->GetClassPathName());

	FContentBrowserModule& ContentBrowser =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	const FString ObjectPath = ContentBrowser.Get().CreateModalSaveAssetDialog(DialogConfig);
	if (ObjectPath.IsEmpty())
	{
		return false;
	}

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
	{
		const FText Prompt = FText::Format(
			LOCTEXT("ConfirmOverwrite", "The asset {0} already exists. Replace it? This cannot be undone."),
			FText::FromString(ObjectPath));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes)
		{
			return false;
		}
		if (!UEditorAssetLibrary::DeleteAsset(ObjectPath))
		{
			ShowBakeNotification(LOCTEXT("DeleteFailed", "Could not replace the existing Static Mesh asset."),
				SNotificationItem::CS_Fail);
			return false;
		}
	}

	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	Options.bEnableCollision = false;
	Options.bEnableRecomputeNormals = false;
	Options.bEnableRecomputeTangents = true;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	const FString PackagePath = FPackageName::ObjectPathToPackageName(ObjectPath);
	UStaticMesh* SavedMesh =
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
			DynamicMesh, PackagePath, Options, Outcome);
	if (!IsValid(SavedMesh) || Outcome != EGeometryScriptOutcomePins::Success)
	{
		ShowBakeNotification(LOCTEXT("BakeFailed", "Static Mesh baking failed."),
			SNotificationItem::CS_Fail);
		return false;
	}

	ApplyWaterfallMaterials(*SavedMesh, *Settings);
	if (!UEditorAssetLibrary::SaveLoadedAsset(SavedMesh, false))
	{
		ShowBakeNotification(LOCTEXT("SaveFailed", "The Static Mesh was created but could not be saved."),
			SNotificationItem::CS_Fail);
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("AssignBakedMesh", "Assign Baked Waterfall Mesh"));
	Waterfall.Modify();
	Waterfall.SetBakedStaticMesh(SavedMesh);
	Settings->SetShowBakedMesh(true);
	Waterfall.MarkPackageDirty();
	ShowBakeNotification(LOCTEXT("BakeSucceeded", "Waterfall Static Mesh baked successfully."),
		SNotificationItem::CS_Success);
	return true;
}

#undef LOCTEXT_NAMESPACE
