#include "TerraceTerrainBuilder.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapePatchEditLayer.h"
#include "LandscapeTexturePatch.h"
#include "ScopedTransaction.h"
#include "TerraceGroupActor.h"
#include "TerracePlotActor.h"
#include "Components/SplineComponent.h"
#include "TerraceStructureActor.h"
#include "TerraceTerrainOutputActor.h"
#include "TerraceTerrainRaster.h"
#include "TextureCompiler.h"

#define LOCTEXT_NAMESPACE "TerraceTerrainBuilder"

namespace
{
    bool CanEdit(const ATerraceGroupActor* Group, FText& OutError)
    {
        if (!IsValid(Group) || !Group->GetWorld() || Group->GetWorld()->WorldType != EWorldType::Editor
            || !GEditor || GEditor->PlayWorld)
        {
            OutError = LOCTEXT("Editor", "请在停止 PIE 后的编辑器世界中操作。");
            return false;
        }
        if (Group->GeneratedTerrainOutput.IsPending())
        {
            OutError = LOCTEXT("UnloadedOutput", "已有地形输出未加载，请先加载，避免重复生成。");
            return false;
        }
        if (Group->GeneratedStructureOutput.IsPending())
        {
            OutError = LOCTEXT("UnloadedStructure", "已有结构输出未加载，请先加载后再修改地形。");
            return false;
        }
        return true;
    }

    void RemoveOwnedStructures(ATerraceGroupActor* Group)
    {
        ATerraceStructureActor* Structures = Group->GeneratedStructureOutput.Get();
        if (IsValid(Structures) && Structures->SourceGroup.Get() == Group && Structures->GetOwner() == Group)
        {
            Group->GetWorld()->EditorDestroyActor(Structures, true);
        }
        Group->GeneratedStructureOutput.Reset();
    }

    ATerraceTerrainOutputActor* GetOwnedOutput(ATerraceGroupActor* Group)
    {
        auto* Output = Cast<ATerraceTerrainOutputActor>(Group->GeneratedTerrainOutput.Get());
        return IsValid(Output) && Output->SourceGroup.Get() == Group && Output->GetOwner() == Group ? Output : nullptr;
    }

    bool CheckCoverage(ALandscape* Landscape, const TArray<FTerraceTerrainRaster>& Rasters, FText& OutError)
    {
        ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
        FIntRect Extent;
        if (!Info || Info->ComponentSizeQuads <= 0 || !Info->GetLandscapeExtent(Extent))
        {
            OutError = LOCTEXT("NoComponents", "目标 Landscape 没有已加载的有效组件。");
            return false;
        }
        const FTransform Transform = Landscape->GetActorTransform();
        for (int32 I = 0; I < Rasters.Num(); ++I)
        {
            const auto& Raster = Rasters[I];
            const FVector Min = Transform.InverseTransformPosition(FVector(Raster.Bounds.Min, Raster.Elevation));
            const FVector Max = Transform.InverseTransformPosition(FVector(Raster.Bounds.Max, Raster.Elevation));
            // Landscape height range before Z scale: uint16 mapped to [-256, 256-1/128].
            if (Min.ContainsNaN() || Max.ContainsNaN() || Min.Z < -256.0 || Min.Z > 255.9921875
                || Min.X < Extent.Min.X || Min.Y < Extent.Min.Y || Max.X > Extent.Max.X || Max.Y > Extent.Max.Y)
            {
                OutError = FText::Format(LOCTEXT("Outside", "田块 {0} 的遮罩（含过渡和一圈采样留边）超出地形范围，或标高超过目标 Landscape 可表示范围。"), I);
                return false;
            }
            const int32 Size = Info->ComponentSizeQuads;
            const int32 MinX = FMath::FloorToInt(Min.X / Size);
            const int32 MinY = FMath::FloorToInt(Min.Y / Size);
            const int32 MaxX = FMath::CeilToInt(Max.X / Size) - 1;
            const int32 MaxY = FMath::CeilToInt(Max.Y / Size) - 1;
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    ULandscapeComponent* Component = Info->XYtoComponentMap.FindRef(FIntPoint(X, Y));
                    if (!IsValid(Component) || !Component->IsRegistered())
                    {
                        OutError = LOCTEXT("MissingComponent", "遮罩覆盖了缺失或未加载的地形组件，不能提交整平。");
                        return false;
                    }
                }
            }
        }
        return true;
    }
}

bool TerraceTerrainBuilder::Update(ATerraceGroupActor* Group, FText& OutReport)
{
    if (!CanEdit(Group, OutReport))
    {
        return false;
    }
    UWorld* World = Group->GetWorld();
    if (World->IsPartitionedWorld())
    {
        OutReport = LOCTEXT("WP", "M1 首轮小样限定非 World Partition 关卡，请先在独立普通关卡验证。");
        return false;
    }
    ALandscape* Landscape = Group->TargetLandscape.Get();
    if (!IsValid(Landscape) || Landscape->GetWorld() != World)
    {
        OutReport = LOCTEXT("Landscape", "请指定当前世界中已加载的 Target Landscape（主 Landscape Actor）。");
        return false;
    }
    const FVector Scale = Landscape->GetActorScale3D();
    if (Landscape->GetActorTransform().ContainsNaN() || !Landscape->GetActorRotation().IsNearlyZero(0.01)
        || Scale.X <= UE_SMALL_NUMBER || Scale.Y <= UE_SMALL_NUMBER || Scale.Z <= UE_SMALL_NUMBER
        || Landscape->GetEditLayersConst().IsEmpty())
    {
        OutReport = LOCTEXT("LandscapeTransform", "首版要求 Landscape 无旋转、正向缩放，并具有有效编辑层。");
        return false;
    }
    ATerraceTerrainOutputActor* OldOutput = GetOwnedOutput(Group);
    if (OldOutput && OldOutput->HasEnabledContribution() && OldOutput->AppliedLandscape.Get() != Landscape)
    {
        OutReport = LOCTEXT("SwitchTarget", "更换 Landscape 前，请先移除旧地形贡献。");
        return false;
    }

    TArray<FTerraceBoundarySnapshot> Snapshots;
    TArray<FTerraceTerrainRaster> Rasters;
    const double SupportPadding = TerraceTerrainRaster::CalculateSupportPadding(
        Group->TerrainRasterSpacing, Landscape->GetActorTransform());
    if (!Group->BuildBoundarySnapshots(Snapshots, OutReport)
        || !TerraceTerrainRaster::Build(Snapshots, Group->TerrainRasterSpacing, Rasters, OutReport, SupportPadding)
        || !CheckCoverage(Landscape, Rasters, OutReport))
    {
        return false;
    }
    ULandscapePatchEditLayer* Layer = OldOutput && OldOutput->AppliedLandscape.Get() == Landscape
        ? Cast<ULandscapePatchEditLayer>(Landscape->GetEditLayer(OldOutput->LayerGuid)) : nullptr;
    if (Layer && (Layer->IsLocked() || !Layer->IsVisible()
        || !FMath::IsNearlyEqual(Layer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap), 1.0f)))
    {
        OutReport = LOCTEXT("LayerState", "工具 Patch 层被锁定、隐藏或高度 Alpha 不为 1。请先在 Landscape 编辑层中修复。");
        return false;
    }

    const FScopedTransaction Transaction(LOCTEXT("UpdateTransaction", "Update Terrace Landscape"));
    Group->Modify();
    Group->GetLevel()->Modify();
    Landscape->Modify();
    if (!Layer)
    {
        const FString BaseName = TEXT("TerraceTools_") + Group->GetName();
        FName LayerName(*BaseName);
        int32 Suffix = 1;
        while (Landscape->GetEditLayer(LayerName))
        {
            LayerName = FName(*FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++));
        }
        const int32 Index = Landscape->CreateLayer(LayerName, ULandscapePatchEditLayer::StaticClass());
        Layer = Index != INDEX_NONE ? Cast<ULandscapePatchEditLayer>(Landscape->GetEditLayer(Index)) : nullptr;
        if (!Layer)
        {
            OutReport = LOCTEXT("CreateLayer", "无法创建工具 Patch 层，请检查编辑层数量上限。旧结果未替换。");
            return false;
        }
    }

    FActorSpawnParameters Params;
    Params.OverrideLevel = Group->GetLevel();
    Params.ObjectFlags |= RF_Transactional;
    ATerraceTerrainOutputActor* Output = World->SpawnActor<ATerraceTerrainOutputActor>(
        FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Output)
    {
        OutReport = LOCTEXT("Spawn", "无法创建地形输出 Actor。旧结果未替换；新建空层可手动删除。");
        return false;
    }
    Output->SetActorLabel(TEXT("TerraceTerrainOutput"));
    Output->SetOwner(Group);
    Output->SourceGroup = Group;
    Output->AppliedLandscape = Landscape;
    Output->LayerGuid = Layer->GetGuid();
    TArray<UTexture*> TexturesToFinish;
    for (int32 I = 0; I < Rasters.Num(); ++I)
    {
        const auto& Raster = Rasters[I];
        UTexture2D* Texture = NewObject<UTexture2D>(Output,
            FName(*FString::Printf(TEXT("Mask_%d"), I)), RF_Transactional);
        Texture->SRGB = false;
        Texture->CompressionSettings = TC_VectorDisplacementmap;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->NeverStream = true;
        Texture->AddressX = TA_Clamp;
        Texture->AddressY = TA_Clamp;
        Texture->Filter = TF_Bilinear;
        Texture->Source.Init(Raster.Resolution.X, Raster.Resolution.Y, 1, 1, TSF_BGRA8,
            reinterpret_cast<const uint8*>(Raster.Pixels.GetData()));
        Output->MaskTextures.Add(Texture);
        Texture->PostEditChange();
        TexturesToFinish.Add(Texture);

        ULandscapeTexturePatch* Patch = NewObject<ULandscapeTexturePatch>(Output,
            FName(*FString::Printf(TEXT("PlotPatch_%d"), I)), RF_Transactional);
        Patch->SetIsEnabled(false);
        Output->AddInstanceComponent(Patch);
        Patch->SetupAttachment(Output->GetRootComponent());
        Patch->SetRelativeLocation(FVector(Raster.Bounds.GetCenter(), Raster.Elevation));
        Patch->SetUnscaledCoverage(Raster.Bounds.GetSize());
        Patch->SetResolution(FVector2D(Raster.Resolution.X, Raster.Resolution.Y));
        Patch->SetHeightSourceMode(ELandscapeTexturePatchSourceMode::TextureAsset);
        Patch->SetHeightTextureAsset(Texture);
        Patch->SetHeightEncodingMode(ELandscapeTextureHeightPatchEncoding::WorldUnits);
        Patch->SetHeightEncodingSettings(FLandscapeTexturePatchEncodingSettings());
        Patch->SetZeroHeightMeaning(ELandscapeTextureHeightPatchZeroHeightMeaning::PatchZ);
        Patch->SetHeightTextureChannel(ELandscapeTexturePatchTextureChannel::Red);
        Patch->SetHeightAlphaSourceMode(ELandscapeTexturePatchAlphaSourceMode::SourceTextureChannel);
        Patch->SetHeightAlphaTextureChannel(ELandscapeTexturePatchTextureChannel::Alpha);
        Patch->SetFalloffMode(ELandscapeTexturePatchFalloffMode::RoundedRectangle);
        Patch->SetFalloff(0.0f);
        Patch->SetBlendMode(ELandscapeTexturePatchBlendMode::AlphaBlend);
        Output->Patches.Add(Patch);
        if (!Patch->AssignToLandscape(Landscape, Layer->GetName()))
        {
            World->EditorDestroyActor(Output, true);
            OutReport = LOCTEXT("Bind", "Patch 绑定失败，旧结果未替换；新建空层可手动删除。");
            return false;
        }
        Patch->RegisterComponent();
    }
    FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
    for (UTexture* Texture : TexturesToFinish)
    {
        if (!Texture->GetResource())
        {
            World->EditorDestroyActor(Output, true);
            OutReport = LOCTEXT("Texture", "遮罩纹理资源尚未就绪，旧结果未替换。");
            return false;
        }
    }
    Output->CaptureInputs(Snapshots, Group->TerrainRasterSpacing, Landscape->GetActorTransform());
    for (const auto& Member : Group->Plots)
    {
        ATerracePlotActor* Plot = Member.Get(); // All members passed BuildBoundarySnapshots above.
        Plot->Modify();
        if (Plot->PCGBundSpline) { Plot->PCGBundSpline->Modify(); }
        FText PCGReport;
        Plot->RefreshPCGOutput(PCGReport);
        Plot->MarkPackageDirty();
    }
    // Terrain changes invalidate geometry sampled from the previous terrain state.
    RemoveOwnedStructures(Group);
    // All new patches are configured and disabled before replacing the old generation.
    if (OldOutput)
    {
        OldOutput->SetContributionEnabled(false);
    }
    Output->SetContributionEnabled(true);
    Group->GeneratedTerrainOutput = Output;
    if (OldOutput)
    {
        World->EditorDestroyActor(OldOutput, true);
    }
    Group->MarkPackageDirty();
    Landscape->MarkPackageDirty();
    Landscape->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All, true);
    GEditor->RedrawLevelEditingViewports();
    OutReport = FText::Format(LOCTEXT("SubmittedForPCG", "已提交 {0} 块田的 Patch 更新，边界外增加 {1} cm 平坦支撑余量，清理旧过程网格并同步 PCG 输入。等待地形及碰撞完成后，在自己的 PCG 图中 Generate；不会自动清理或重建外部 PCG 输出。"),
        Rasters.Num(), FText::AsNumber(SupportPadding));
    return true;
}

bool TerraceTerrainBuilder::RemoveContribution(ATerraceGroupActor* Group, FText& OutReport)
{
    if (!CanEdit(Group, OutReport))
    {
        return false;
    }
    ATerraceTerrainOutputActor* Output = GetOwnedOutput(Group);
    const FScopedTransaction Transaction(LOCTEXT("RemoveTransaction", "Remove Terrace Landscape Contribution"));
    Group->Modify();
    RemoveOwnedStructures(Group);
    if (Output)
    {
        Output->SetContributionEnabled(false);
    }
    else
    {
        // A copied group must never disable its original group's output.
        Group->GeneratedTerrainOutput.Reset();
    }
    Group->MarkPackageDirty();
    GEditor->RedrawLevelEditingViewports();
    OutReport = LOCTEXT("Removed", "已清理本组结构，并请求移除 Patch 贡献。等待 Landscape 更新后查看下层地形；保留工具空层供下次更新复用。");
    return true;
}

#undef LOCTEXT_NAMESPACE
