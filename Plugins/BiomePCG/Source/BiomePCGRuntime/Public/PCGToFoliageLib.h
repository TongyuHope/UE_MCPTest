#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PCGToFoliageLib.generated.h"

class UInstancedStaticMeshComponent;
class UFoliageType_InstancedStaticMesh;

/**
 * PCG Instanced Mesh 转换为 Foliage 工具库
 * 将指定 Actor（及其子级）下的所有 InstancedStaticMeshComponent
 * 转换为 Foliage 资产并放置到 FoliageActor 中
 * 可在普通 Actor 蓝图中引用；实际烘焙仅在编辑器构建中执行。
 */
UCLASS()
class BIOMEPCGRUNTIME_API UPCGToFoliageLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /**
     * 将 RootActor 下的所有 InstancedStaticMeshComponent 转换为 Foliage 资产
     *
     * @param WorldContextObject       填 self 或任意 Actor（用于获取 World）
     * @param RootActor               挂载 PCG 结果的 Actor，或 PCG Volume 所在 Actor
     * @param bDestroySourceInstances  是否销毁原 InstancedStaticMesh 实例
     * @param bCreateFolder           是否在 StaticMesh 同级目录下创建子文件夹保存 FoliageType
     * @param FolderName              当 bCreateFolder=true 时，创建的子文件夹名称（默认 "FoliageType"）
     */
    UFUNCTION(BlueprintCallable, Category = "PCG|Foliage", meta = (WorldContext = "WorldContextObject"))
    static void ConvertPCGInstancedMeshToFoliage(
        UObject* WorldContextObject,
        AActor* RootActor,
        bool bDestroySourceInstances = false,
        bool bCreateFolder = false,
        FString FolderName = TEXT("FoliageType"));

    /**
     * 将多个 Actor 下的所有 InstancedStaticMeshComponent 转换为 Foliage 资产
     * 支持蓝图调用，也供编辑器烘焙面板使用。
     *
     * @param WorldContextObject       填 self 或任意 Actor（用于获取 World）
     * @param RootActors             多个挂载 PCG 结果的 Actor 数组
     * @param bDestroySourceInstances  是否销毁原 InstancedStaticMesh 实例
     * @param bCreateFolder           是否在 StaticMesh 同级目录下创建子文件夹保存 FoliageType
     * @param FolderName              当 bCreateFolder=true 时，创建的子文件夹名称（默认 "FoliageType"）
     */
    UFUNCTION(BlueprintCallable, Category = "PCG|Foliage", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bCreateFolder,FolderName"))
    static void ConvertMultipleActorsToFoliage(
        UObject* WorldContextObject,
        const TArray<AActor*>& RootActors,
        bool bDestroySourceInstances = false,
        bool bCreateFolder = false,
        FString FolderName = TEXT("FoliageType"));
};
