// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionPresetBPLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/CollisionProfile.h"
#include "Selection.h"
#include "Misc/PackageName.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "FileHelpers.h"
#include "UObject/SavePackage.h"

#if WITH_EDITOR

/** Content Browser 返回 /All/Game/...，AssetRegistry 需要 /Game/... */
static FString NormalizeCollisionContentPath(const FString& Path)
{
	FString Result = Path.Replace(TEXT("\\"), TEXT("/"));
	if (Result.StartsWith(TEXT("/All/")))
	{
		Result = Result.Mid(5);
	}
	else if (Result.StartsWith(TEXT("All/")))
	{
		Result = Result.Mid(4);
	}
	if (!Result.StartsWith(TEXT("/")))
	{
		Result = TEXT("/") + Result;
	}
	if (Result.EndsWith(TEXT("/")))
	{
		Result = Result.LeftChop(1);
	}
	return Result;
}

/** 递归收集指定路径下所有的 StaticMesh 资产 */
static void GatherStaticMeshesRecursive(
	const FString& FolderPath,
	TArray<UStaticMesh*>& OutMeshes)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetDataList, true);

	for (const FAssetData& Data : AssetDataList)
	{
		if (Data.AssetClassPath.GetAssetName() != FName("StaticMesh"))
			continue;

		UStaticMesh* Mesh = Cast<UStaticMesh>(Data.GetAsset());
		if (Mesh)
		{
			OutMeshes.Add(Mesh);
		}
	}
}

/** 修改单个 StaticMesh 的 Collision Preset 并保存 */
static bool ModifyAndSaveStaticMeshCollisionPreset(UStaticMesh* StaticMesh, const FName& CollisionProfileName)
{
	if (!StaticMesh)
		return false;

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup)
	{
		BodySetup = NewObject<UBodySetup>(StaticMesh, NAME_None, RF_Transactional);
		StaticMesh->SetBodySetup(BodySetup);
	}

	BodySetup->DefaultInstance.SetCollisionProfileName(CollisionProfileName);

	StaticMesh->MarkPackageDirty();

	bool bSaved = false;
	UPackage* Package = StaticMesh->GetOutermost();
	if (Package)
	{
		FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		bSaved = UPackage::SavePackage(Package, StaticMesh, *PackageFileName, SaveArgs);
	}

	return bSaved;
}

#endif

int32 UCollisionPresetBPLibrary::BatchModifyCollisionPresetsSimple(
	const FString& CollisionPresetName,
	const TArray<FString>& FolderPaths)
{
	return BatchModifyCollisionPresets(CollisionPresetName, FolderPaths, TArray<UStaticMesh*>());
}

int32 UCollisionPresetBPLibrary::BatchModifyCollisionPresets(
	const FString& CollisionPresetName,
	const TArray<FString>& FolderPaths,
	const TArray<UStaticMesh*>& StaticMeshAssets)
{
#if WITH_EDITOR
	int32 SuccessCount = 0;

	TArray<UStaticMesh*> MeshesToProcess;

	for (UStaticMesh* Mesh : StaticMeshAssets)
	{
		if (Mesh && !MeshesToProcess.Contains(Mesh))
		{
			MeshesToProcess.Add(Mesh);
		}
	}

	TArray<FString> FoldersToScan = FolderPaths;
	if (FoldersToScan.Num() == 0)
	{
		FoldersToScan = UEditorUtilityLibrary::GetSelectedFolderPaths();
	}

	for (const FString& RawFolderPath : FoldersToScan)
	{
		FString NormalizedPath = NormalizeCollisionContentPath(RawFolderPath);
		GatherStaticMeshesRecursive(NormalizedPath, MeshesToProcess);
	}

	if (MeshesToProcess.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollisionPreset: 未找到任何 StaticMesh 资产进行处理"));
		return 0;
	}

	FString NotificationText = FString::Printf(TEXT("正在修改 %d 个 StaticMesh 的 Collision Preset 为: %s"), MeshesToProcess.Num(), *CollisionPresetName);
	FNotificationInfo Info(FText::FromString(NotificationText));
	Info.bUseSuccessFailIcons = false;
	Info.bUseThrobber = true;
	Info.ExpireDuration = 0.0f;

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
	}

	GEditor->BeginTransaction(FText::FromString(TEXT("Batch Modify Collision Presets")));

	FName ProfileName(*CollisionPresetName);

	for (UStaticMesh* Mesh : MeshesToProcess)
	{
		if (ModifyAndSaveStaticMeshCollisionPreset(Mesh, ProfileName))
		{
			SuccessCount++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CollisionPreset: 失败修改资产: %s"), *Mesh->GetName());
		}
	}

	GEditor->EndTransaction();

	if (NotificationItem.IsValid())
	{
		FString ResultText = FString::Printf(TEXT("成功修改并保存 %d / %d 个 StaticMesh"), SuccessCount, MeshesToProcess.Num());
		NotificationItem->SetText(FText::FromString(ResultText));
		NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		NotificationItem->ExpireAndFadeout();
	}

	GEditor->RedrawLevelEditingViewports();

	UE_LOG(LogTemp, Display, TEXT("CollisionPreset: 完成！成功修改并保存 %d / %d 个 StaticMesh"), SuccessCount, MeshesToProcess.Num());

	return SuccessCount;
#else
	return 0;
#endif
}

TArray<FString> UCollisionPresetBPLibrary::GetAvailableCollisionPresets()
{
	TArray<FString> PresetNames;

#if WITH_EDITOR
	// 使用 UCollisionProfile 的公共 API 获取所有预设名称
	TArray<TSharedPtr<FName>> ProfileNameList;
	UCollisionProfile::GetProfileNames(ProfileNameList);

	for (const TSharedPtr<FName>& ProfileName : ProfileNameList)
	{
		if (ProfileName.IsValid())
		{
			PresetNames.Add(ProfileName->ToString());
		}
	}

	if (PresetNames.Num() == 0)
	{
		PresetNames.Add(TEXT("NoCollision"));
		PresetNames.Add(TEXT("BlockAll"));
		PresetNames.Add(TEXT("OverlapAll"));
		PresetNames.Add(TEXT("BlockAllDynamic"));
		PresetNames.Add(TEXT("OverlapAllDynamic"));
		PresetNames.Add(TEXT("IgnoreOnlyPawns"));
		PresetNames.Add(TEXT("OverlapOnlyPawns"));
		PresetNames.Add(TEXT("BlockAllButWeapons"));
		PresetNames.Add(TEXT("OverlapAllButWeapons"));
		PresetNames.Add(TEXT("BlockAllWeapons"));
		PresetNames.Add(TEXT("Vehicle"));
		PresetNames.Add(TEXT("Spectator"));
		PresetNames.Add(TEXT("CharacterMesh"));
		PresetNames.Add(TEXT("PhysicsActor"));
		PresetNames.Add(TEXT("Destructible"));
		PresetNames.Add(TEXT("InvisibleWall"));
		PresetNames.Add(TEXT("InvisibleWallDynamic"));
		PresetNames.Add(TEXT("CustomTraceQuery"));
		PresetNames.Add(TEXT("Visibility"));
		PresetNames.Add(TEXT("Camera"));
	}
#endif

	return PresetNames;
}
