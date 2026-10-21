#include "TerraceStructureBuilder.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeEditLayer.h"
#include "LandscapePatchEditLayer.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ScopedTransaction.h"
#include "TerraceGeometry.h"
#include "TerraceGroupActor.h"
#include "TerracePlotActor.h"
#include "TerraceStructureActor.h"
#include "TerraceStructureGeometry.h"
#include "TerraceTerrainOutputActor.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "TerraceStructureBuilder"

namespace
{
    bool CanEdit(ATerraceGroupActor* Group, FText& OutError)
    {
        if (!IsValid(Group) || !Group->GetWorld() || Group->GetWorld()->WorldType != EWorldType::Editor
            || !GEditor || GEditor->PlayWorld)
        {
            OutError = LOCTEXT("Editor", "请在停止 PIE 后的编辑器世界中操作。");
            return false;
        }
        if (Group->GeneratedStructureOutput.IsPending())
        {
            OutError = LOCTEXT("Pending", "结构输出未加载，请先加载后再更新或清理。");
            return false;
        }
        return true;
    }

    ATerraceStructureActor* GetOwnedOutput(ATerraceGroupActor* Group)
    {
        ATerraceStructureActor* Output = Group->GeneratedStructureOutput.Get();
        return IsValid(Output) && Output->SourceGroup.Get() == Group && Output->GetOwner() == Group ? Output : nullptr;
    }
}

bool TerraceStructureBuilder::Update(ATerraceGroupActor* Group, FText& OutReport)
{
    if (!CanEdit(Group, OutReport) || !Group->StructureSettings.Validate(OutReport))
    {
        return false;
    }
    UWorld* World = Group->GetWorld();
    ALandscape* Landscape = Group->TargetLandscape.Get();
    if (World->IsPartitionedWorld() || !IsValid(Landscape) || Landscape->GetWorld() != World)
    {
        OutReport = LOCTEXT("Target", "M2 小样要求普通非 WP 关卡和当前世界中已加载的 Target Landscape。");
        return false;
    }
    TArray<FTerraceBoundarySnapshot> Snapshots;
    if (!Group->BuildBoundarySnapshots(Snapshots, OutReport))
    {
        return false;
    }
    ATerraceTerrainOutputActor* Terrain = Cast<ATerraceTerrainOutputActor>(Group->GeneratedTerrainOutput.Get());
    if (!IsValid(Terrain) || Terrain->SourceGroup.Get() != Group || Terrain->GetOwner() != Group
        || Terrain->AppliedLandscape.Get() != Landscape
        || !Terrain->MatchesInputs(Snapshots, Group->TerrainRasterSpacing, Landscape->GetActorTransform()))
    {
        OutReport = LOCTEXT("OutdatedSupport", "请先使用当前版本“更新地形”，应用当前边界、标高、过渡宽度和田埂支撑余量，然后再生成结构。0.3.0 旧输出也需要更新一次。");
        return false;
    }
    const ULandscapePatchEditLayer* Layer = Cast<ULandscapePatchEditLayer>(Landscape->GetEditLayerConst(Terrain->LayerGuid));
    if (!Layer || !Layer->IsVisible() || Layer->IsLocked()
        || !FMath::IsNearlyEqual(Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap), 1.0f))
    {
        OutReport = LOCTEXT("Layer", "工具 Patch 层需要可见、解锁且高度 Alpha 为 1。");
        return false;
    }
    if (!Landscape->IsUpToDate())
    {
        OutReport = LOCTEXT("Busy", "Landscape 尚有地形更新或回读工作，请等待更新完成后再次生成结构。");
        return false;
    }
    if (Landscape->CollisionMipLevel != 0 || Landscape->SimpleCollisionMipLevel != 0)
    {
        OutReport = LOCTEXT("CollisionMip", "M2 小样要求 Landscape 的 Collision Mip Level 和 Simple Collision Mip Level 均为 0，避免护坡与角色碰撞面不一致。");
        return false;
    }
    TArray<FTerraceStructureLayout> Layouts;
    int64 TotalTriangles = 0;
    for (const auto& Snapshot : Snapshots)
    {
        FTerraceStructureLayout Layout;
        FTerraceBoundarySnapshot StructureSnapshot = Snapshot;
        // The generated plateau now includes a flat band outside the authored boundary.
        // Extend the bank to the same terrain transition foot, keeping the bund in place.
        StructureSnapshot.Settings.TransitionWidth += Terrain->AppliedSupportPadding;
        if (!TerraceStructureGeometry::BuildLayout(StructureSnapshot, Group->StructureSettings, Layout, OutReport))
        {
            OutReport = FText::Format(LOCTEXT("LayoutFailureWithPadding", "田块 {0}：护坡有效宽度 {1} cm（含自动支撑余量 {2} cm）。{3}"),
                Layouts.Num() + 1, FText::AsNumber(StructureSnapshot.Settings.TransitionWidth),
                FText::AsNumber(Terrain->AppliedSupportPadding), OutReport);
            return false;
        }
        TotalTriangles += static_cast<int64>(4) * Layout.Rings.Num() * Layout.Rings[0].Num();
        if (TotalTriangles > 131072)
        {
            OutReport = LOCTEXT("Budget", "整组结构超过 131,072 三角形，请减少田块或降低结构采样密度。");
            return false;
        }
        Layouts.Add(MoveTemp(Layout));
    }
    double PairChecks = 0.0;
    for (int32 I = 0; I < Layouts.Num(); ++I)
    {
        for (int32 J = I + 1; J < Layouts.Num(); ++J)
        {
            PairChecks += static_cast<double>(Layouts[I].Rings.Last().Num()) * Layouts[J].Rings.Last().Num();
        }
    }
    if (PairChecks > 4000000.0)
    {
        OutReport = LOCTEXT("PairBudget", "结构相邻关系超出检查预算，请减少田块或降低采样密度。");
        return false;
    }
    for (int32 I = 0; I < Layouts.Num(); ++I)
    {
        for (int32 J = I + 1; J < Layouts.Num(); ++J)
        {
            if (TerraceGeometry::PolygonDistance(Layouts[I].Rings.Last(), Layouts[J].Rings.Last()) <= TerraceGeometry::BoundaryTolerance)
            {
                OutReport = LOCTEXT("Overlap", "田埂或护坡外轮廓相互接触/重叠，请拉开田块或减小过渡宽度。");
                return false;
            }
        }
    }

    TArray<FTerraceStructureMesh> MeshData;
    TArray<UMaterialInterface*> BundMaterials, BankMaterials;
    TArray<TStrongObjectPtr<UMaterialInterface>> MaterialKeepAlive;
    for (int32 I = 0; I < Layouts.Num(); ++I)
    {
        const auto& Layout = Layouts[I];
        TArray<TArray<double>> Ground;
        for (const auto& Ring : Layout.Rings)
        {
            TArray<double> Heights;
            for (const auto& XY : Ring)
            {
                const TOptional<float> Height = Landscape->GetHeightAtLocation(FVector(XY, Layout.Origin.Z), EHeightfieldSource::Complex);
                if (!Height.IsSet() || !FMath::IsFinite(Height.GetValue()))
                {
                    OutReport = LOCTEXT("Height", "护坡覆盖缺失、带洞或尚未就绪的 Landscape 碰撞区域，请检查边界余量和目标地形。");
                    return false;
                }
                Heights.Add(static_cast<double>(Height.GetValue()));
            }
            Ground.Add(MoveTemp(Heights));
        }
        FTerraceStructureMesh Mesh;
        if (!TerraceStructureGeometry::BuildMesh(Layout, Group->StructureSettings, Ground, Mesh, OutReport))
        {
            OutReport = FText::Format(LOCTEXT("PlotGroundFailure", "田块 {0}（{1}）：{2}"),
                I + 1, FText::FromString(Group->Plots[I].Get()->GetActorLabel()), OutReport);
            return false;
        }
        MeshData.Add(MoveTemp(Mesh));
        UMaterialInterface* Bund = Snapshots[I].Settings.BundMaterial.LoadSynchronous();
        if (Bund)
        {
            MaterialKeepAlive.Emplace(Bund);
        }
        UMaterialInterface* Bank = Snapshots[I].Settings.BankMaterial.LoadSynchronous();
        if (Bank)
        {
            MaterialKeepAlive.Emplace(Bank);
        }
        if ((!Snapshots[I].Settings.BundMaterial.IsNull() && !Bund) || (!Snapshots[I].Settings.BankMaterial.IsNull() && !Bank))
        {
            OutReport = LOCTEXT("Material", "指定的田埂或护坡材质无法加载，旧结构未替换。");
            return false;
        }
        BundMaterials.Add(Bund);
        BankMaterials.Add(Bank ? Bank : Bund);
    }

    const FScopedTransaction Transaction(LOCTEXT("Update", "Update Terrace Structures"));
    Group->Modify();
    Group->GetLevel()->Modify();
    FActorSpawnParameters Params;
    Params.OverrideLevel = Group->GetLevel();
    Params.ObjectFlags |= RF_Transactional;
    ATerraceStructureActor* Output = World->SpawnActor<ATerraceStructureActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Output)
    {
        OutReport = LOCTEXT("Spawn", "无法创建结构输出，旧结构未替换。");
        return false;
    }
    Output->SetActorLabel(TEXT("TerraceStructures"));
    Output->SetOwner(Group);
    Output->SourceGroup = Group;
    for (int32 I = 0; I < MeshData.Num(); ++I)
    {
        UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Output,
            FName(*FString::Printf(TEXT("PlotMesh_%d"), I)), RF_Transactional);
        Component->PrimaryComponentTick.bCanEverTick = false;
        Component->SetMobility(EComponentMobility::Static);
        Component->bUseComplexAsSimpleCollision = true;
        Component->bUseAsyncCooking = false;
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Output->AddInstanceComponent(Component);
        Component->SetupAttachment(Output->GetRootComponent());
        Component->SetRelativeLocation(MeshData[I].Origin);
        Component->RegisterComponent();
        Output->Meshes.Add(Component);
        Output->PlotIds.Add(Snapshots[I].PlotId);
        for (int32 Section = 0; Section < 3; ++Section)
        {
            const auto& Data = MeshData[I].Sections[Section];
            if (!Data.Triangles.IsEmpty())
            {
                Component->CreateMeshSection(Section, Data.Vertices, Data.Triangles, Data.Normals, Data.UVs,
                    TArray<FColor>(), Data.Tangents, Group->StructureSettings.bCreateStructureCollision);
            }
            Component->SetMaterial(Section, Section == 0 ? BundMaterials[I] : BankMaterials[I]);
        }
    }
    ATerraceStructureActor* OldOutput = GetOwnedOutput(Group);
    Group->GeneratedStructureOutput = Output;
    if (OldOutput)
    {
        World->EditorDestroyActor(OldOutput, true);
    }
    for (UProceduralMeshComponent* Component : Output->Meshes)
    {
        Component->SetCollisionEnabled(Group->StructureSettings.bCreateStructureCollision
            ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
    Output->MarkPackageDirty();
    Group->MarkPackageDirty();
    GEditor->RedrawLevelEditingViewports();
    OutReport = FText::Format(LOCTEXT("Built", "已生成 {0} 块田的田埂/土质护坡，共 {1} 个三角形。请检查转角、地形接缝和角色行走；未生成台阶或垂直石墙。"),
        MeshData.Num(), FText::AsNumber(TotalTriangles));
    return true;
}

bool TerraceStructureBuilder::Clear(ATerraceGroupActor* Group, FText& OutReport)
{
    if (!CanEdit(Group, OutReport))
    {
        return false;
    }
    const FScopedTransaction Transaction(LOCTEXT("Clear", "Clear Terrace Structures"));
    Group->Modify();
    if (ATerraceStructureActor* Output = GetOwnedOutput(Group))
    {
        Group->GetWorld()->EditorDestroyActor(Output, true);
    }
    Group->GeneratedStructureOutput.Reset();
    Group->MarkPackageDirty();
    GEditor->RedrawLevelEditingViewports();
    OutReport = LOCTEXT("Cleared", "已清理本组结构输出；Landscape Patch 保留。");
    return true;
}

#undef LOCTEXT_NAMESPACE
