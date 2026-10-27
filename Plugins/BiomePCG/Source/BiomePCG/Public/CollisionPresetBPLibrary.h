// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CollisionPresetBPLibrary.generated.h"

/**
 * Collision Preset Manager - 批量修改 StaticMesh 资产的 Collision Presets 并保存
 * 支持选择文件夹（递归）或直接选择 StaticMesh 资产
 */
UCLASS()
class BIOMEPCG_API UCollisionPresetBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 批量修改 StaticMesh 资产的 Collision Presets 并保存
	 * @param CollisionPresetName  目标 Collision Preset 名称（如 "BlockAll", "NoCollision" 等）
	 * @param FolderPaths  要处理的文件夹路径数组（留空则使用 Content Browser 当前选中的文件夹，会递归扫描子文件夹）
	 * @param StaticMeshAssets  要处理的 StaticMesh 资产数组（直接选择资产时使用）
	 * @return 成功修改并保存的资产数量
	 */
	UFUNCTION(BlueprintCallable, Category = "BiomePCG|Collision Preset", meta = (CallInEditor = "true"))
	static int32 BatchModifyCollisionPresets(
		const FString& CollisionPresetName,
		const TArray<FString>& FolderPaths,
		const TArray<UStaticMesh*>& StaticMeshAssets
	);

	/**
	 * 获取所有可用的 Collision Preset 名称列表
	 * @return Collision Preset 名称数组
	 */
	UFUNCTION(BlueprintCallable, Category = "BiomePCG|Collision Preset")
	static TArray<FString> GetAvailableCollisionPresets();

	/**
	 * 简化版本：使用默认的 Collision Preset
	 * @param FolderPaths  要处理的文件夹路径数组（留空则使用 Content Browser 当前选中的文件夹）
	 * @return 成功修改并保存的资产数量
	 */
	UFUNCTION(BlueprintCallable, Category = "BiomePCG|Collision Preset", meta = (CallInEditor = "true"))
	static int32 BatchModifyCollisionPresetsSimple(
		const FString& CollisionPresetName,
		const TArray<FString>& FolderPaths
	);
};
