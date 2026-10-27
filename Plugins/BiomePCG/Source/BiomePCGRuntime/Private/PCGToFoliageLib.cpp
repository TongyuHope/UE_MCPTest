#include "PCGToFoliageLib.h"

#include "Logging/LogMacros.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/EngineTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "RenderingThread.h"

// Foliage 相关
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "FoliageStatistics.h"

// 资产 / 包
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogPCGToFoliage, Log, All);

namespace
{
#if WITH_EDITOR

    /**
     * 比较两个材质数组是否一致（长度与逐元素指针相同）
     */
    bool MaterialsEqual(const TArray<UMaterialInterface*>& A, const TArray<TObjectPtr<UMaterialInterface>>& B)
    {
        if (A.Num() != B.Num()) return false;
        for (int32 i = 0; i < A.Num(); ++i)
        {
            if (A[i] != B[i]) return false;
        }
        return true;
    }

    /**
     * 比较两个材质数组是否一致（逐元素指针比较）
     */
    bool MaterialsEqual(const TArray<UMaterialInterface*>& A, const TArray<UMaterialInterface*>& B)
    {
        if (A.Num() != B.Num()) return false;
        for (int32 i = 0; i < A.Num(); ++i)
        {
            if (A[i] != B[i]) return false;
        }
        return true;
    }

    /** 根据材质数组生成唯一后缀（用于命名） */
    FString MaterialsToSuffix(const TArray<UMaterialInterface*>& Materials)
    {
        FString Combined;
        for (UMaterialInterface* Mat : Materials)
        {
            if (Mat) Combined += Mat->GetPathName();
            Combined += TEXT("|");
        }
        return FString::Printf(TEXT("_M%08X"), GetTypeHash(Combined));
    }

    /**
     * 从 InstancedStaticMeshComponent 获取 Instance End Cull Distance 属性。
     * ISM 的 InstanceEndCullDistance 是 int32 类型（与 FoliageType 的 CullDistance.Max 一致）。
     */
    int32 GetInstanceEndCullDistance(const UInstancedStaticMeshComponent* ISMC)
    {
        if (!ISMC)
        {
            return 0;
        }
        return ISMC->InstanceEndCullDistance;
    }

    /**
     * 根据 StaticMesh 和材质查找/创建 UFoliageType_InstancedStaticMesh。
     * 判定规则：Mesh 相同且 OverrideMaterials 数组相同 → 引用已有；否则新建。
     * 命名：静态网格名_Foliage（无材质变体）或 静态网格名_Foliage_M<Hash>（有材质变体）
     *
     * @param ISMC  输入的 InstancedStaticMeshComponent，用于获取材质和剔除距离
     * @param InstanceEndCullDistance  从 ISMC 获取的 Instance End Cull Distance（int32）
     * @param bUpdateExisting  是否更新已存在 FoliageType 的 CullDistance（当其值为0时）
     * @param bCreateFolder  是否在 StaticMesh 同级目录下创建子文件夹保存 FoliageType
     * @param FolderName  当 bCreateFolder=true 时，创建的子文件夹名称
     */
    UFoliageType_InstancedStaticMesh* FindOrCreateFoliageTypeForMesh(
        UInstancedStaticMeshComponent* ISMC,
        int32 InstanceEndCullDistance = 0,
        bool bUpdateExisting = true,
        bool bCreateFolder = false,
        const FString& FolderName = TEXT("FoliageType"))
    {
        if (!ISMC)
        {
            return nullptr;
        }

        UStaticMesh* Mesh = ISMC->GetStaticMesh();
        if (!Mesh)
        {
            return nullptr;
        }

        TArray<UMaterialInterface*> ComponentMaterials = ISMC->GetMaterials();

        const FString MeshPackagePath = Mesh->GetOutermost()->GetName();
        FString FolderPath;
        FString MeshAssetName;
        if (!MeshPackagePath.Split(TEXT("/"), &FolderPath, &MeshAssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
        {
            return nullptr;
        }

        // 如果需要创建文件夹，则在 StaticMesh 同级目录下创建子文件夹
        if (bCreateFolder && !FolderName.IsEmpty())
        {
            FolderPath = FolderPath + TEXT("/") + FolderName;
        }

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

        FARFilter Filter;
        Filter.PackagePaths.Add(*FolderPath);
        Filter.ClassPaths.Add(UFoliageType_InstancedStaticMesh::StaticClass()->GetClassPathName());
        Filter.bRecursivePaths = false;

        TArray<FAssetData> Assets;
        AssetRegistryModule.Get().GetAssets(Filter, Assets);

        UFoliageType_InstancedStaticMesh* Result = nullptr;

        // 查找 Mesh 相同且材质数组相同的 FoliageType
        for (const FAssetData& Asset : Assets)
        {
            UFoliageType_InstancedStaticMesh* Candidate = Cast<UFoliageType_InstancedStaticMesh>(Asset.GetAsset());
            if (!Candidate || Candidate->GetStaticMesh() != Mesh)
            {
                continue;
            }
            if (MaterialsEqual(ComponentMaterials, Candidate->OverrideMaterials))
            {
                Result = Candidate;

                // 如果已存在的 FoliageType 没有设置 CullDistance，但当前有有效的 InstanceEndCullDistance，则更新它
                if (bUpdateExisting && InstanceEndCullDistance > 0 && Result->CullDistance.Max <= 0)
                {
                    Result->CullDistance.Min = 0;
                    Result->CullDistance.Max = InstanceEndCullDistance;
                    Result->MarkPackageDirty();
                    UE_LOG(LogPCGToFoliage, Log,
                        TEXT("Updated existing foliage type '%s' with CullDistance Max = %d from ISM."),
                        *Result->GetName(), InstanceEndCullDistance);
                }
                else if (Result->CullDistance.Max > 0)
                {
                    UE_LOG(LogPCGToFoliage, Log,
                        TEXT("Using existing foliage type '%s' with CullDistance Max = %d (ISM requested: %d)."),
                        *Result->GetName(), Result->CullDistance.Max, InstanceEndCullDistance);
                }
                break;
            }
        }

        if (!Result)
        {
            // 获取 StaticMesh 的默认材质数组
            TArray<UMaterialInterface*> StaticMeshMaterials;
            if (Mesh)
            {
                for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i)
                {
                    StaticMeshMaterials.Add(Mesh->GetMaterial(i));
                }
            }

            // 比较 ISM 材质和 StaticMesh 默认材质，只有不同时才添加后缀
            bool bMaterialsDiffer = !MaterialsEqual(ComponentMaterials, StaticMeshMaterials);

            // 文件名格式：StaticMeshName_Foliage（材质相同）或 StaticMeshName_Foliage_M<Hash>（材质不同）
            FString BaseName = FString::Printf(TEXT("%s_Foliage"), *MeshAssetName);
            if (bMaterialsDiffer)
            {
                BaseName += MaterialsToSuffix(ComponentMaterials);
                UE_LOG(LogPCGToFoliage, Log, TEXT("ISM materials differ from StaticMesh default, adding suffix. Mesh: %s, ISM materials: %d, StaticMesh materials: %d"),
                    *MeshAssetName, ComponentMaterials.Num(), StaticMeshMaterials.Num());
            }

            FString FoliageAssetName = BaseName;
            int32 Suffix = 0;
            while (FindObject<UFoliageType_InstancedStaticMesh>(nullptr, *(FolderPath + TEXT("/") + FoliageAssetName + TEXT(".") + FoliageAssetName)))
            {
                FoliageAssetName = FString::Printf(TEXT("%s_%d"), *BaseName, ++Suffix);
            }

            FString FoliagePackagePath = FolderPath + TEXT("/") + FoliageAssetName;
            UPackage* Package = CreatePackage(*FoliagePackagePath);
            Result = NewObject<UFoliageType_InstancedStaticMesh>(
                Package,
                *FoliageAssetName,
                RF_Public | RF_Standalone
            );

            Result->SetStaticMesh(Mesh);
            Result->OverrideMaterials.Reset();
            for (UMaterialInterface* Mat : ComponentMaterials)
            {
                Result->OverrideMaterials.Add(Mat);
            }

            // 从 StaticMesh 获取 Collision Profile 并应用到 FoliageType
            if (Mesh && Mesh->GetBodySetup())
            {
                FName StaticMeshCollisionProfile = Mesh->GetBodySetup()->DefaultInstance.GetCollisionProfileName();
                if (!StaticMeshCollisionProfile.IsNone())
                {
                    Result->BodyInstance.SetCollisionProfileName(StaticMeshCollisionProfile);
                    UE_LOG(LogPCGToFoliage, Log, TEXT("Applied collision profile '%s' from StaticMesh '%s' to foliage type."),
                        *StaticMeshCollisionProfile.ToString(), *Mesh->GetName());
                }
            }

            // 应用从 ISMC 获取的 Instance End Cull Distance 到 FoliageType
            if (InstanceEndCullDistance > 0)
            {
                Result->CullDistance.Min = 0;
                Result->CullDistance.Max = InstanceEndCullDistance;
                UE_LOG(LogPCGToFoliage, Log, TEXT("Applied Instance End Cull Distance Max = %d from ISM to foliage type '%s'."),
                    InstanceEndCullDistance, *Result->GetName());
            }
            else
            {
                UE_LOG(LogPCGToFoliage, Log, TEXT("No Instance End Cull Distance found from ISM for foliage type '%s', using default."),
                    *Result->GetName());
            }

            Package->MarkPackageDirty();
            AssetRegistryModule.AssetCreated(Result);

            UE_LOG(LogPCGToFoliage, Log, TEXT("Created foliage type asset: %s (mesh + %d materials)")
                TEXT("%s"),
                *FoliagePackagePath, ComponentMaterials.Num(),
                InstanceEndCullDistance > 0 ? *FString::Printf(TEXT(", Instance End Cull Distance Max = %d"), InstanceEndCullDistance) : TEXT(""));
        }

        return Result;
    }

    /**
     * 使用与 Foliage 手绘模式相同的 FoliageTrace 检测地表，确保命中结果与引擎一致。
     * 返回的 Hit.Component 作为 BaseComponent，使植被能随地形高度变化而更新。
     */
    bool FoliageTraceAtLocation(const UWorld* World, const FVector& WorldLocation, const UFoliageType* FoliageType, FHitResult& OutHit)
    {
        if (!World || !FoliageType) return false;
        const float TraceDist = 10000.0f;
        const FVector Start = WorldLocation + FVector(0, 0, TraceDist);
        const FVector End = WorldLocation - FVector(0, 0, TraceDist);
        const FDesiredFoliageInstance DesiredInst(Start, End, FoliageType, 0.f);
        static const FName TraceTag(TEXT("PCGToFoliageTrace"));
        return AInstancedFoliageActor::FoliageTrace(World, OutHit, DesiredInst, TraceTag, /*bReturnFaceIndex*/ true, FFoliageTraceFilterFunc(), /*bAverageNormal*/ false) && OutHit.GetComponent();
    }

/**
 * 递归收集 Actor 及其附属子 Actor 下的所有 InstancedStaticMeshComponent
 *（PCG Graph 一般会把结果挂在某个 Actor 层级下）
 */
void GatherISMCRecursive(AActor* Actor, TArray<UInstancedStaticMeshComponent*>& OutComponents)
{
    if (!Actor)
    {
        return;
    }

    // 当前 Actor 自身的 ISMC
    Actor->GetComponents<UInstancedStaticMeshComponent>(OutComponents);

    // 附属子 Actor 里的 ISMC
    TArray<AActor*> AttachedActors;
    Actor->GetAttachedActors(AttachedActors);
    for (AActor* Child : AttachedActors)
    {
        GatherISMCRecursive(Child, OutComponents);
    }
}

/**
 * 将单个 Actor 下的所有 InstancedStaticMeshComponent 转换为 Foliage 资产
 * 内部实现函数
 *
 * @param WorldContextObject       用于获取 World
 * @param RootActor               要处理的 Actor
 * @param CurrentLevel            目标 Level
 * @param bDestroySourceInstances  是否销毁原 ISM 实例
 * @param bCreateFolder           是否创建子文件夹
 * @param FolderName              子文件夹名称
 * @param OutConvertedCount       输出的转换实例数量
 */
void ConvertSingleActorToFoliage(
    UObject* WorldContextObject,
    AActor* RootActor,
    ULevel* CurrentLevel,
    bool bDestroySourceInstances,
    bool bCreateFolder,
    const FString& FolderName,
    int32& OutConvertedCount)
{
    OutConvertedCount = 0;

    if (!WorldContextObject || !RootActor)
    {
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return;
    }

    // 收集当前 Actor 下的所有 ISMC（包括子 Actor）
    TArray<UInstancedStaticMeshComponent*> ISMComponents;
    GatherISMCRecursive(RootActor, ISMComponents);

    if (ISMComponents.Num() == 0)
    {
        UE_LOG(LogPCGToFoliage, Verbose, TEXT("No ISMC found under actor %s, skipping."), *RootActor->GetName());
        return;
    }

    UE_LOG(LogPCGToFoliage, Log, TEXT("Processing actor '%s' with %d ISMC components."), *RootActor->GetName(), ISMComponents.Num());

    for (UInstancedStaticMeshComponent* ISMC : ISMComponents)
    {
        if (!ISMC)
        {
            continue;
        }

        UStaticMesh* Mesh = ISMC->GetStaticMesh();
        if (!Mesh)
        {
            continue;
        }

        // 获取 ISM 的 Instance End Cull Distance 并赋予到 FoliageType 的 Cull Distance Max
        int32 InstanceEndCullDistance = GetInstanceEndCullDistance(ISMC);

        UFoliageType_InstancedStaticMesh* FoliageType = FindOrCreateFoliageTypeForMesh(ISMC, InstanceEndCullDistance, true, bCreateFolder, FolderName);
        if (!FoliageType)
        {
            UE_LOG(LogPCGToFoliage, Warning, TEXT("Failed to find or create foliage type for mesh %s."), *Mesh->GetName());
            continue;
        }

        const int32 InstanceCount = ISMC->GetInstanceCount();
        if (InstanceCount == 0)
        {
            continue;
        }

        // 把 ISMC 的实例 Transform（世界空间）全部拉出来
        TArray<FTransform> InstanceTransforms;
        InstanceTransforms.Reserve(InstanceCount);

        for (int32 Index = 0; Index < InstanceCount; ++Index)
        {
            FTransform InstanceTransform;
            if (ISMC->GetInstanceTransform(Index, InstanceTransform, true))
            {
                InstanceTransforms.Add(InstanceTransform);
            }
        }

        if (InstanceTransforms.Num() == 0)
        {
            continue;
        }

        // 与 Foliage 手绘模式一致：FoliageTrace + BaseComponent，并按实例位置分发到对应分区的 IFA
        TArray<FFoliageInstance> PlacedInstances;
        PlacedInstances.Reserve(InstanceTransforms.Num());

        for (const FTransform& InstanceTransform : InstanceTransforms)
        {
            const FVector WorldLoc = InstanceTransform.GetLocation();
            FHitResult Hit;

            if (FoliageTraceAtLocation(World, WorldLoc, FoliageType, Hit))
            {
                FFoliageInstance Inst;
                Inst.SetInstanceWorldTransform(InstanceTransform);
                Inst.BaseComponent = Hit.GetComponent();
                PlacedInstances.Add(MoveTemp(Inst));
            }
            else
            {
                FFoliageInstance Inst;
                Inst.SetInstanceWorldTransform(InstanceTransform);
                PlacedInstances.Add(MoveTemp(Inst));
            }
        }

        if (PlacedInstances.Num() > 0)
        {
            // 按实例 Location 分发到各自分区的 IFA
            TMap<AInstancedFoliageActor*, TArray<const FFoliageInstance*>> PerIFAPlacedInstances;
            for (const FFoliageInstance& PlacedInstance : PlacedInstances)
            {
                ULevel* const LevelHint = PlacedInstance.BaseComponent
                    ? PlacedInstance.BaseComponent->GetComponentLevel()
                    : CurrentLevel;
                if (AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(World, true, LevelHint, PlacedInstance.Location))
                {
                    PerIFAPlacedInstances.FindOrAdd(IFA).Add(&PlacedInstance);
                }
            }

            for (const auto& Pair : PerIFAPlacedInstances)
            {
                AInstancedFoliageActor* IFA = Pair.Key;
                const TArray<const FFoliageInstance*>& InstancePtrs = Pair.Value;
                FFoliageInfo* TypeInfo = nullptr;
                if (UFoliageType* RegisteredType = IFA->AddFoliageType(FoliageType, &TypeInfo))
                {
                    TypeInfo->AddInstances(RegisteredType, InstancePtrs);
                    TypeInfo->Refresh(true, true);
                    OutConvertedCount += InstancePtrs.Num();
                }
            }
        }

        UE_LOG(LogPCGToFoliage, Log, TEXT("Converted %d instances from component %s to foliage."), InstanceTransforms.Num(), *ISMC->GetName());

        // 可选：清空原有 PCG ISMC 实例
        if (bDestroySourceInstances)
        {
            ISMC->ClearInstances();
        }
    }
}

#endif // WITH_EDITOR
} // namespace

void UPCGToFoliageLib::ConvertPCGInstancedMeshToFoliage(
    UObject* WorldContextObject,
    AActor* RootActor,
    bool bDestroySourceInstances,
    bool bCreateFolder,
    FString FolderName)
{
#if !WITH_EDITOR
    UE_LOG(LogPCGToFoliage, Warning, TEXT("ConvertPCGInstancedMeshToFoliage only works in editor builds."));
    return;
#else
    if (!WorldContextObject || !RootActor)
    {
        UE_LOG(LogPCGToFoliage, Warning, TEXT("WorldContextObject or RootActor is null."));
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogPCGToFoliage, Warning, TEXT("World is null."));
        return;
    }

    ULevel* Level = RootActor->GetLevel();
    if (!Level)
    {
        Level = World->GetCurrentLevel();
    }

    int32 ConvertedCount = 0;
    ConvertSingleActorToFoliage(
        WorldContextObject,
        RootActor,
        Level,
        bDestroySourceInstances,
        bCreateFolder,
        FolderName,
        ConvertedCount);

    UE_LOG(LogPCGToFoliage, Display, TEXT("ConvertPCGInstancedMeshToFoliage completed. Total converted: %d instances."), ConvertedCount);
#endif
}

void UPCGToFoliageLib::ConvertMultipleActorsToFoliage(
    UObject* WorldContextObject,
    const TArray<AActor*>& RootActors,
    bool bDestroySourceInstances,
    bool bCreateFolder,
    FString FolderName)
{
#if !WITH_EDITOR
    UE_LOG(LogPCGToFoliage, Warning, TEXT("ConvertMultipleActorsToFoliage only works in editor builds."));
    return;
#else
    if (!WorldContextObject)
    {
        UE_LOG(LogPCGToFoliage, Warning, TEXT("WorldContextObject is null."));
        return;
    }

    if (RootActors.Num() == 0)
    {
        UE_LOG(LogPCGToFoliage, Warning, TEXT("No actors provided."));
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogPCGToFoliage, Warning, TEXT("World is null."));
        return;
    }

    // 使用当前 Level 作为默认 Level
    ULevel* CurrentLevel = World->GetCurrentLevel();

    int32 TotalConverted = 0;
    int32 ActorsProcessed = 0;
    int32 ActorsWithISMC = 0;

    for (AActor* RootActor : RootActors)
    {
        if (!RootActor)
        {
            UE_LOG(LogPCGToFoliage, Verbose, TEXT("Skipping null actor."));
            continue;
        }

        ActorsProcessed++;

        // 检查这个 Actor 是否有 ISMC
        TArray<UInstancedStaticMeshComponent*> ISMComponents;
        GatherISMCRecursive(RootActor, ISMComponents);

        if (ISMComponents.Num() == 0)
        {
            UE_LOG(LogPCGToFoliage, Verbose, TEXT("Actor '%s' has no ISMC, skipping."), *RootActor->GetName());
            continue;
        }

        ActorsWithISMC++;

        int32 ConvertedCount = 0;
        ConvertSingleActorToFoliage(
            WorldContextObject,
            RootActor,
            CurrentLevel,
            bDestroySourceInstances,
            bCreateFolder,
            FolderName,
            ConvertedCount);

        TotalConverted += ConvertedCount;
    }

    UE_LOG(LogPCGToFoliage, Display, TEXT("ConvertMultipleActorsToFoliage completed. Actors processed: %d/%d (with ISMC: %d), Total instances converted: %d."),
        ActorsProcessed, RootActors.Num(), ActorsWithISMC, TotalConverted);
#endif
}
