#include "ConvertSMALib.h"

#include "Logging/LogMacros.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogConvertSMA, Log, All);

namespace
{
#if WITH_EDITOR

    /**
     * 递归收集 Actor 及其子 Actor 下的所有 InstancedStaticMeshComponent
     */
    void GatherSMAComponentsRecursive(AActor* Actor, TArray<UInstancedStaticMeshComponent*>& OutComponents)
    {
        if (!Actor)
        {
            return;
        }

        Actor->GetComponents<UInstancedStaticMeshComponent>(OutComponents);

        TArray<AActor*> AttachedActors;
        Actor->GetAttachedActors(AttachedActors);
        for (AActor* Child : AttachedActors)
        {
            GatherSMAComponentsRecursive(Child, OutComponents);
        }
    }

    /**
     * 获取或创建 StaticMesh Actor Factory
     */
    UActorFactoryStaticMesh* GetStaticMeshFactory()
    {
        static UActorFactoryStaticMesh* Factory = nullptr;
        if (!Factory)
        {
            Factory = NewObject<UActorFactoryStaticMesh>();
            Factory->AddToRoot();
        }
        return Factory;
    }

    /**
     * 从 StaticMesh 资产复制 Collision Presets 到 StaticMeshComponent
     */
    void ApplyCollisionFromStaticMesh(UStaticMeshComponent* SMComponent, UStaticMesh* SourceMesh)
    {
        if (!SMComponent || !SourceMesh)
        {
            return;
        }

        // 获取 StaticMesh 的 BodySetup
        UBodySetup* BodySetup = SourceMesh->GetBodySetup();
        if (!BodySetup)
        {
            return;
        }

        // 从 BodySetup 获取 Collision Profile 名称并设置
        FName ProfileName = BodySetup->DefaultInstance.GetCollisionProfileName();
        if (!ProfileName.IsNone())
        {
            SMComponent->SetCollisionProfileName(ProfileName);
        }
    }

#endif // WITH_EDITOR
} // namespace

void UConvertSMALib::ConvertISMToStaticMeshActors(
    UObject* WorldContextObject,
    AActor* RootActor,
    FString GroupName,
    bool bDestroySourceInstances)
{
#if !WITH_EDITOR
    UE_LOG(LogConvertSMA, Warning, TEXT("ConvertISMToStaticMeshActors only works in editor builds."));
    return;
#else
    if (!WorldContextObject || !RootActor)
    {
        UE_LOG(LogConvertSMA, Warning, TEXT("WorldContextObject or RootActor is null."));
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogConvertSMA, Warning, TEXT("World is null."));
        return;
    }

    ULevel* Level = RootActor->GetLevel();
    if (!Level)
    {
        Level = World->GetCurrentLevel();
    }

    // 收集当前层级下所有 ISMC（包括子 Actor）
    TArray<UInstancedStaticMeshComponent*> ISMComponents;
    GatherSMAComponentsRecursive(RootActor, ISMComponents);

    if (ISMComponents.Num() == 0)
    {
        UE_LOG(LogConvertSMA, Warning, TEXT("No InstancedStaticMeshComponent found under actor %s."),
            *RootActor->GetName());
        return;
    }

    // 确保组名不为空
    if (GroupName.IsEmpty())
    {
        GroupName = TEXT("STA_Folder");
    }

    // 获取 Factory
    UActorFactoryStaticMesh* Factory = GetStaticMeshFactory();
    if (!Factory)
    {
        UE_LOG(LogConvertSMA, Error, TEXT("Failed to get UActorFactoryStaticMesh"));
        return;
    }

    int32 TotalConverted = 0;
    int32 SMAIndex = 0;

    for (UInstancedStaticMeshComponent* ISMC : ISMComponents)
    {
        if (!ISMC)
        {
            continue;
        }

        UStaticMesh* Mesh = ISMC->GetStaticMesh();
        if (!Mesh)
        {
            UE_LOG(LogConvertSMA, Warning, TEXT("ISM %s has no StaticMesh assigned, skipping."),
                *ISMC->GetName());
            continue;
        }

        const int32 InstanceCount = ISMC->GetInstanceCount();
        if (InstanceCount == 0)
        {
            continue;
        }

        UE_LOG(LogConvertSMA, Log, TEXT("Processing ISM '%s' with %d instances, mesh: '%s'"),
            *ISMC->GetName(), InstanceCount, *Mesh->GetName());

        // 收集所有实例的 Transform
        TArray<FTransform> InstanceTransforms;
        InstanceTransforms.Reserve(InstanceCount);

        for (int32 Index = 0; Index < InstanceCount; ++Index)
        {
            FTransform InstanceTransform;
            if (ISMC->GetInstanceTransform(Index, InstanceTransform, /*bWorldSpace*/ true))
            {
                InstanceTransforms.Add(InstanceTransform);
            }
        }

        if (InstanceTransforms.Num() == 0)
        {
            continue;
        }

        // 获取 ISM 的材质
        TArray<UMaterialInterface*> ISMMaterials = ISMC->GetMaterials();

        // 获取 ISM 的 CollisionProfile
        FName CollisionProfileName = ISMC->BodyInstance.GetCollisionProfileName();

        // 缓存 ISMC 的其他属性
        const bool bCastShadow = ISMC->CastShadow;
        const EComponentMobility::Type Mobility = ISMC->Mobility;

        // 创建 AssetData
        FAssetData MeshAssetData(Mesh);

        // 为每个实例创建一个 StaticMeshActor
        for (const FTransform& InstanceTransform : InstanceTransforms)
        {
            // 使用 Factory 创建 Actor
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = RootActor;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnParams.OverrideLevel = Level;

            AActor* SpawnedActor = Factory->CreateActor(MeshAssetData.GetAsset(), Level, InstanceTransform, SpawnParams);
            if (!SpawnedActor)
            {
                UE_LOG(LogConvertSMA, Warning, TEXT("Failed to spawn StaticMeshActor for instance %d of ISM %s"),
                    SMAIndex, *ISMC->GetName());
                continue;
            }

            UE_LOG(LogConvertSMA, Log, TEXT("Created Actor: %s, Class: %s"),
                *SpawnedActor->GetName(), *SpawnedActor->GetClass()->GetName());

            // 获取 StaticMeshComponent 并配置
            TArray<UActorComponent*> Components;
            SpawnedActor->GetComponents(UStaticMeshComponent::StaticClass(), Components);

            for (UActorComponent* Comp : Components)
            {
                UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(Comp);
                if (SMComponent && SMComponent->GetStaticMesh())
                {
                    // 应用材质
                    for (int32 MatIndex = 0; MatIndex < ISMMaterials.Num(); ++MatIndex)
                    {
                        if (ISMMaterials[MatIndex])
                        {
                            SMComponent->SetMaterial(MatIndex, ISMMaterials[MatIndex]);
                        }
                    }

                    // 应用阴影配置
                    SMComponent->SetCastShadow(bCastShadow);

                    // 应用 Mobility
                    SMComponent->SetMobility(Mobility);

                    // 根据 StaticMesh 资产的 Collision Presets 设置碰撞
                    ApplyCollisionFromStaticMesh(SMComponent, Mesh);

                    break; // 只处理第一个有效的 SMC
                }
            }

            // 设置 Actor 标签
            SpawnedActor->SetActorLabel(FString::Printf(TEXT("%s_%s_%d"), *Mesh->GetName(), *GroupName, SMAIndex));

            // 设置 Folder
            SpawnedActor->SetFolderPath(FName(*GroupName));

            TotalConverted++;
            SMAIndex++;
        }

        UE_LOG(LogConvertSMA, Log, TEXT("Converted %d instances from component %s to StaticMeshActors."),
            InstanceTransforms.Num(), *ISMC->GetName());

        // 可选：销毁原 ISM 实例
        if (bDestroySourceInstances)
        {
            ISMC->ClearInstances();
        }
    }

    UE_LOG(LogConvertSMA, Log, TEXT("ConvertSMA completed: %d StaticMeshActors created in group '%s'"),
        TotalConverted, *GroupName);
#endif // WITH_EDITOR
}
