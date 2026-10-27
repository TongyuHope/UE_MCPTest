#include "BiomePCGBPLibrary.h"
#include "BiomePCG.h"

#include "Engine/Selection.h"
#include "Editor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Engine/Level.h" 

// 辅助函数：在具体的 IFA 中查找组件
UInstancedStaticMeshComponent* FindFoliageComponentInActor(AInstancedFoliageActor* IFA, UStaticMesh* Mesh)
{
    if (!IFA || !Mesh) return nullptr;
    TArray<UInstancedStaticMeshComponent*> Components;
    IFA->GetComponents(Components);
    for (UInstancedStaticMeshComponent* Comp : Components)
    {
        if (Comp->GetStaticMesh() == Mesh) return Comp;
    }
    return nullptr;
}

void UBiomePCGBPLibrary::ConvertSelectionToFoliage(UObject* WorldContextObject, bool bRemoveOriginal)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;

    USelection* SelectedActors = GEditor->GetSelectedActors();
    TArray<AActor*> Actors;
    SelectedActors->GetSelectedObjects<AActor>(Actors);

    GEditor->BeginTransaction(FText::FromString("Convert ISM to Foliage"));
    bool bAnyChange = false;

    for (AActor* Actor : Actors)
    {
        TArray<UInstancedStaticMeshComponent*> ISMComponents;
        Actor->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

        for (UInstancedStaticMeshComponent* ISMComp : ISMComponents)
        {
            if (!ISMComp || ISMComp->GetInstanceCount() == 0) continue;
            UStaticMesh* Mesh = ISMComp->GetStaticMesh();
            if (!Mesh) continue;

            // 遍历所有实例
            for (int32 i = 0; i < ISMComp->GetInstanceCount(); i++)
            {
                FTransform InstanceTransform;
                ISMComp->GetInstanceTransform(i, InstanceTransform, true);
                FVector Location = InstanceTransform.GetLocation();

                // --- 1. 射线检测 (找地面) ---
                FHitResult Hit;
                FVector TraceStart = Location + FVector(0, 0, 10000.0f);
                FVector TraceEnd = Location - FVector(0, 0, 10000.0f);

                // 只有打中物体，我们才能知道它属于哪个 Level
                if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic))
                {
                    // 吸附高度
                    if (Hit.GetActor() && (Hit.GetActor()->IsA(ALandscape::StaticClass()) || Hit.GetActor()->IsA(ALandscapeProxy::StaticClass())))
                    {
                        Location.Z = Hit.Location.Z;
                    }

                    // --- 2. 获取目标 Level (修复崩溃的核心步骤) ---
                    ULevel* TargetLevel = nullptr;
                    if (Hit.GetComponent())
                    {
                        TargetLevel = Hit.GetComponent()->GetComponentLevel();
                    }

                    // 兜底：如果射线打在空地上，就用 Actor 所在的 Level
                    if (!TargetLevel)
                    {
                        TargetLevel = Actor->GetLevel();
                    }

                    // --- 3. 获取该 Level 专属的 IFA ---
                    // [重点] 使用 3 参数的 Get，传入 TargetLevel
                    // 这样引擎就会去当前的分区找植被管理器，而不是去拿不存在的全局管理器（从而避免崩溃）
                    AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(World, true, TargetLevel);

                    if (IFA)
                    {
                        // --- 4. 注册 Mesh ---
                        UFoliageType* OutType = nullptr;
                        // [重点] 使用 AddMesh (成员函数)，完全避开 AddInstances 的链接错误
                        IFA->AddMesh(Mesh, &OutType);

                        // --- 5. 手动添加实例 ---
                        UInstancedStaticMeshComponent* TargetComp = FindFoliageComponentInActor(IFA, Mesh);

                        // 双重保险：找不到就刷新一下再找
                        if (!TargetComp)
                        {
                            IFA->Modify();
                            TargetComp = FindFoliageComponentInActor(IFA, Mesh);
                        }

                        if (TargetComp)
                        {
                            TargetComp->Modify();
                            InstanceTransform.SetLocation(Location);

                            // [重点] 直接操作组件添加实例，这是最底层的写法，绝对稳健
                            TargetComp->AddInstance(InstanceTransform, true);
                            bAnyChange = true;
                        }
                    }
                }
            }
        }

        if (bRemoveOriginal)
        {
            Actor->Modify();
            Actor->Destroy();
        }
    }

    if (bAnyChange)
    {
        World->MarkPackageDirty();
    }

    GEditor->EndTransaction();
    GEditor->RedrawLevelEditingViewports();
}