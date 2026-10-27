#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConvertSMALib.generated.h"

/**
 * Convert SMA - 将 ISM 转换为 StaticMeshActor 蓝图工具库
 * 将当前蓝图层级下的 InstancedStaticMeshComponent 的所有模型实例
 * 转换为世界大纲中的 StaticMeshActor，确保位置、旋转、缩放一致
 * 可在普通 Actor 蓝图中引用；实际转换仅在编辑器构建中执行。
 */
UCLASS()
class BIOMEPCGRUNTIME_API UConvertSMALib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /**
     * 将 RootActor 下的所有 InstancedStaticMeshComponent 实例转换为独立的 StaticMeshActor
     *
     * @param WorldContextObject       填 self 或任意 Actor（用于获取 World）
     * @param RootActor               挂载 ISM 的 Actor，或 ISM 所在 Actor
     * @param GroupName               创建的 StaticMeshActor 所属的组名称（用于世界大纲组织）
     * @param bDestroySourceInstances 是否销毁原 InstancedStaticMesh 实例
     */
    UFUNCTION(BlueprintCallable, Category = "PCG|ConvertSMA", meta = (WorldContext = "WorldContextObject"))
    static void ConvertISMToStaticMeshActors(
        UObject* WorldContextObject,
        AActor* RootActor,
        FString GroupName = TEXT("STA_Folder"),
        bool bDestroySourceInstances = true);
};
