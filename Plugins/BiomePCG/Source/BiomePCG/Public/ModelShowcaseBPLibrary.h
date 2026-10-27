// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModelShowcaseBPLibrary.generated.h"

/** 行轴方向：模型在行内沿此轴排列 */
UENUM(BlueprintType)
enum class EShowcaseRowAxis : uint8
{
	Y  UMETA(DisplayName = "Y轴（行沿Y）"),
	X  UMETA(DisplayName = "X轴（行沿X）")
};

/** 排列顺序轴：每行模型按此轴的 |BoundMin|+|BoundMax| 由小到大排序；自动适配则按每模型最长轴的尺寸排序并自动旋转对齐 */
UENUM(BlueprintType)
enum class EShowcaseSortAxis : uint8
{
	Auto  UMETA(DisplayName = "自动适配"),
	Z     UMETA(DisplayName = "Z轴"),
	X     UMETA(DisplayName = "X轴"),
	Y     UMETA(DisplayName = "Y轴")
};

/**
 * Model Showcase - 根据 Content 文件夹结构将模型按类型排布到场景中
 * 支持多文件夹选择、递归遍历、智能间距、Text 大小与颜色
 */
UCLASS()
class BIOMEPCG_API UModelShowcaseBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 创建模型展示
	 * @param ContentFolderPaths  Content 文件夹路径数组，留空则使用 Content Browser 当前选中的多个文件夹
	 * @param PlanActor  排布原点 Actor
	 * @param RowAxis  行轴：Y=行内模型沿Y排列，X=沿X排列
	 * @param SortAxis  排列顺序轴：每行模型按此轴的 |BoundMin|+|BoundMax| 由小到大排序
	 * @return 成功创建的 Actor 数量
	 */
	UFUNCTION(BlueprintCallable, Category = "BiomePCG|Model Showcase", meta = (CallInEditor = "true"))
	static int32 CreateModelShowcase(
		const TArray<FString>& ContentFolderPaths,
		AActor* PlanActor = nullptr,
		EShowcaseRowAxis RowAxis = EShowcaseRowAxis::Y,
		EShowcaseSortAxis SortAxis = EShowcaseSortAxis::Auto
	);

	/** 简化版本：单路径，使用默认参数 */
	UFUNCTION(BlueprintCallable, Category = "BiomePCG|Model Showcase", meta = (CallInEditor = "true"))
	static int32 CreateModelShowcaseSimple(
		const FString& ContentFolderPath = TEXT(""),
		AActor* PlanActor = nullptr
	);
};
