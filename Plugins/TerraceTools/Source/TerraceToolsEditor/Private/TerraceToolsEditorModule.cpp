#include "Modules/ModuleManager.h"

#include "ComponentVisualizer.h"
#include "Components/SplineComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "IDetailCustomization.h"
#include "PrimitiveDrawInterface.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "TerraceGroupActor.h"
#include "TerracePlotActor.h"
#include "TerracePreviewComponent.h"
#include "TerraceTerrainBuilder.h"
#include "TerraceStructureBuilder.h"
#include "UnrealEdGlobals.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TerraceToolsEditor"

DEFINE_LOG_CATEGORY_STATIC(LogTerraceTools, Log, All);

namespace
{
    bool RefreshPCGInputs(ATerraceGroupActor* Group, ATerracePlotActor* Plot, FText& OutReport)
    {
        UWorld* World = Group ? Group->GetWorld() : (Plot ? Plot->GetWorld() : nullptr);
        if (!World || World->WorldType != EWorldType::Editor || !GEditor || GEditor->PlayWorld)
        {
            OutReport = LOCTEXT("PCGEditorOnly", "请在停止 PIE 后的编辑器世界中同步 PCG 输入。");
            return false;
        }
        TArray<ATerracePlotActor*> Members;
        if (Group)
        {
            for (const auto& Member : Group->Plots)
            {
                ATerracePlotActor* Item = Member.Get();
                if (!IsValid(Item) || Item->GetWorld() != World)
                {
                    OutReport = LOCTEXT("PCGMissingMember", "组内存在未加载、失效或跨世界的 Plot，请先修复成员列表。");
                    return false;
                }
                Members.AddUnique(Item);
            }
        }
        else if (IsValid(Plot)) { Members.Add(Plot); }
        if (Members.IsEmpty())
        {
            OutReport = LOCTEXT("PCGEmpty", "没有可同步的 Plot。");
            return false;
        }
        const FScopedTransaction Transaction(LOCTEXT("PCGSyncTransaction", "Refresh Terrace PCG Inputs"));
        int32 Valid = 0, Disabled = 0, Failed = 0;
        FText FirstError;
        for (ATerracePlotActor* Member : Members)
        {
            Member->Modify();
            if (Member->PCGBundSpline) { Member->PCGBundSpline->Modify(); }
            FText Report;
            const bool bSuccess = Member->RefreshPCGOutput(Report);
            Member->MarkPackageDirty();
            if (bSuccess) { ++Valid; }
            else if (!Member->bEnablePCGOutput) { ++Disabled; }
            else
            {
                ++Failed;
                if (FirstError.IsEmpty())
                {
                    FirstError = FText::Format(LOCTEXT("PCGMemberError", "{0}：{1}"),
                        FText::FromString(Member->GetActorLabel()), Report);
                }
            }
        }
        GEditor->RedrawLevelEditingViewports();
        OutReport = FText::Format(LOCTEXT("PCGSyncReport", "PCG 输入：有效 {0} 块，关闭 {1} 块，无效 {2} 块。无效/关闭输出已清空。地形完成后手动 Generate PCG。{3}"),
            Valid, Disabled, Failed, FirstError);
        return Failed == 0;
    }

    void ShowReport(const TSharedRef<STextBlock>& Status, const FText& Report, bool bSuccess)
    {
        Status->SetText(Report);
        if (bSuccess)
        {
            UE_LOG(LogTerraceTools, Display, TEXT("%s"), *Report.ToString());
        }
        else
        {
            UE_LOG(LogTerraceTools, Warning, TEXT("%s"), *Report.ToString());
        }
    }

    class FTerraceActorDetails : public IDetailCustomization
    {
    public:
        static TSharedRef<IDetailCustomization> MakeInstance()
        {
            return MakeShared<FTerraceActorDetails>();
        }

        virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
        {
            DetailBuilder.EditCategory(TEXT("梯田工具 / Terrace Tools"),
                LOCTEXT("Parameters", "梯田工具 / Terrace Tools · 参数"), ECategoryPriority::Important).SetSortOrder(1);
            TArray<TWeakObjectPtr<UObject>> Objects;
            DetailBuilder.GetObjectsBeingCustomized(Objects);
            if (Objects.Num() != 1)
            {
                return;
            }
            TWeakObjectPtr<ATerraceGroupActor> Group = Cast<ATerraceGroupActor>(Objects[0].Get());
            TWeakObjectPtr<ATerracePlotActor> Plot = Cast<ATerracePlotActor>(Objects[0].Get());
            IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
                TEXT("TerraceActions"), LOCTEXT("Actions", "梯田工具 / Terrace Tools · 操作"), ECategoryPriority::Important);
            Category.SetSortOrder(0);
            const TSharedRef<STextBlock> Status = SNew(STextBlock)
                .Text(LOCTEXT("PCGWorkflow", "当前流程：更新地形 → 等待碰撞完成 → 同步 PCG 输入 → 在自己的 PCG 图中 Generate。组件标签：TerracePCGBundSpline。原过程网格位于“网格实验”分类。"))
                .AutoWrapText(true);

            Category.AddCustomRow(LOCTEXT("ValidateFilter", "校验 Validate"))
            .WholeRowContent()
            [
                SNew(SButton)
                .Text(LOCTEXT("Validate", "校验边界 / Validate Boundaries"))
                .OnClicked_Lambda([Group, Plot, Status]()
                {
                    FText Report;
                    bool bValid = false;
                    if (Group.IsValid())
                    {
                        TArray<FTerraceBoundarySnapshot> Snapshots;
                        bValid = Group->BuildBoundarySnapshots(Snapshots, Report);
                    }
                    else if (Plot.IsValid())
                    {
                        FTerraceBoundarySnapshot Snapshot;
                        bValid = Plot->BuildBoundarySnapshot(Snapshot, Report);
                        if (bValid)
                        {
                            Report = FText::Format(LOCTEXT("PlotValid", "边界有效：{0} 个采样点，世界标高 {1} cm。"),
                                Snapshot.Boundary.Num(), FText::AsNumber(Snapshot.WorldElevation));
                        }
                    }
                    else
                    {
                        Report = LOCTEXT("Deleted", "Actor 已失效，请重新选择。");
                    }
                    ShowReport(Status, Report, bValid);
                    return FReply::Handled();
                })
            ];

            Category.AddCustomRow(LOCTEXT("PCGSyncFilter", "同步 PCG 输入 Refresh PCG Inputs"))
            .WholeRowContent()
            [
                SNew(SButton)
                .Text(LOCTEXT("PCGSync", "同步 PCG 输入 / Refresh PCG Inputs"))
                .ToolTipText(LOCTEXT("PCGSyncHelp", "同步 Plot 标签、田面标高闭合样条和 PCGData 参数。选择组时同步组内成员。不改地形，不执行 PCG 图。"))
                .OnClicked_Lambda([Group, Plot, Status]()
                {
                    FText Report;
                    const bool bSuccess = RefreshPCGInputs(Group.Get(), Plot.Get(), Report);
                    ShowReport(Status, Report, bSuccess);
                    return FReply::Handled();
                })
            ];

            if (Group.IsValid())
            {
                IDetailCategoryBuilder& MeshExperiment = DetailBuilder.EditCategory(TEXT("TerraceMeshExperiment"),
                    LOCTEXT("MeshExperiment", "梯田工具 / Terrace Tools · 网格实验"), ECategoryPriority::Default);
                MeshExperiment.SetSortOrder(2);
                Category.AddCustomRow(LOCTEXT("TerrainFilter", "更新地形 Update Terrain"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("TerrainUpdate", "更新地形 / Update Terrain"))
                    .OnClicked_Lambda([Group, Status]()
                    {
                        FText Report;
                        const bool bSuccess = TerraceTerrainBuilder::Update(Group.Get(), Report);
                        ShowReport(Status, Report, bSuccess);
                        return FReply::Handled();
                    })
                ];
                MeshExperiment.AddCustomRow(LOCTEXT("StructureFilter", "更新田埂护坡 Update Structures"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StructureExperiment", "生成旧版过程网格 / Update Structures"))
                    .ToolTipText(LOCTEXT("StructureUsage", "先在 Plot 设置田埂宽高和材质，再选中本组更新地形，等待碰撞完成后点击此按钮。它会沿边界生成田埂及外侧土坡，田内继续使用 Landscape。"))
                    .OnClicked_Lambda([Group, Status]()
                    {
                        FText Report;
                        const bool bSuccess = TerraceStructureBuilder::Update(Group.Get(), Report);
                        ShowReport(Status, Report, bSuccess);
                        return FReply::Handled();
                    })
                ];
                Category.AddCustomRow(LOCTEXT("StructureClearFilter", "清理结构 Clear Structures"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StructureClearLegacy", "清理旧版网格 / Clear Structures"))
                    .OnClicked_Lambda([Group, Status]()
                    {
                        FText Report;
                        const bool bSuccess = TerraceStructureBuilder::Clear(Group.Get(), Report);
                        ShowReport(Status, Report, bSuccess);
                        return FReply::Handled();
                    })
                ];
                Category.AddCustomRow(LOCTEXT("RemoveTerrainFilter", "移除地形贡献 Remove Terrain"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("TerrainRemove", "移除地形贡献 / Remove Terrain Contribution"))
                    .OnClicked_Lambda([Group, Status]()
                    {
                        FText Report;
                        const bool bSuccess = TerraceTerrainBuilder::RemoveContribution(Group.Get(), Report);
                        ShowReport(Status, Report, bSuccess);
                        return FReply::Handled();
                    })
                ];
                Category.AddCustomRow(LOCTEXT("AddFilter", "新增田块 Add Plot"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Add", "新增田块 / Add Plot"))
                    .OnClicked_Lambda([Group, Status]()
                    {
                        if (!Group.IsValid() || !Group->GetWorld() || Group->GetWorld()->WorldType != EWorldType::Editor
                            || !GEditor || GEditor->PlayWorld)
                        {
                            ShowReport(Status, LOCTEXT("EditorOnly", "请在停止 PIE 后的编辑器世界中操作。"), false);
                            return FReply::Handled();
                        }
                        const FScopedTransaction Transaction(LOCTEXT("AddTransaction", "Add Terrace Plot"));
                        Group->Modify();
                        Group->GetLevel()->Modify();
                        FActorSpawnParameters Params;
                        Params.OverrideLevel = Group->GetLevel();
                        Params.ObjectFlags |= RF_Transactional;
                        const FVector Location = Group->GetActorLocation() + FVector(0, Group->Plots.Num() * 1500.0, 0);
                        ATerracePlotActor* NewPlot = Group->GetWorld()->SpawnActor<ATerracePlotActor>(Location, FRotator::ZeroRotator, Params);
                        if (NewPlot)
                        {
                            NewPlot->SetActorLabel(TEXT("TerracePlot"));
                            Group->Plots.Add(TSoftObjectPtr<ATerracePlotActor>(NewPlot));
                            Group->MarkPackageDirty();
                            GEditor->RedrawLevelEditingViewports();
                            ShowReport(Status, LOCTEXT("Added", "已新增田块并加入组。可在 Outliner 中选择它并编辑 BoundarySpline。"), true);
                        }
                        else
                        {
                            ShowReport(Status, LOCTEXT("SpawnFailure", "无法创建田块。"), false);
                        }
                        return FReply::Handled();
                    })
                ];
            }

            if (Plot.IsValid())
            {
                Category.AddCustomRow(LOCTEXT("FlattenFilter", "控制点归平 Flatten"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Flatten", "控制点归平 / Flatten Control Points"))
                    .OnClicked_Lambda([Plot, Status]()
                    {
                        if (!Plot.IsValid() || !IsValid(Plot->BoundarySpline) || !Plot->GetWorld()
                            || Plot->GetWorld()->WorldType != EWorldType::Editor || !GEditor || GEditor->PlayWorld)
                        {
                            return FReply::Handled();
                        }
                        const FScopedTransaction Transaction(LOCTEXT("FlattenTransaction", "Flatten Terrace Control Points"));
                        Plot->Modify();
                        USplineComponent* Spline = Plot->BoundarySpline;
                        Spline->Modify();
                        for (int32 I = 0; I < Spline->GetNumberOfSplinePoints(); ++I)
                        {
                            FVector P = Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::Local);
                            P.Z = 0.0;
                            Spline->SetLocationAtSplinePoint(I, P, ESplineCoordinateSpace::Local, false);
                        }
                        Spline->bSplineHasBeenEdited = true;
                        Spline->UpdateSpline();
                        Plot->MarkPackageDirty();
                        GEditor->RedrawLevelEditingViewports();
                        ShowReport(Status, LOCTEXT("Flattened", "已将控制点局部 Z 设为 0；田面标高仍为 Actor 世界 Z + Elevation Offset。"), true);
                        return FReply::Handled();
                    })
                ];
            }

            Category.AddCustomRow(LOCTEXT("StatusFilter", "状态 Status"))
            .WholeRowContent()[Status];
        }
    };

    class FTerracePreviewVisualizer : public FComponentVisualizer
    {
    public:
        virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override
        {
            if (!Component || !PDI)
            {
                return;
            }
            if (const ATerracePlotActor* Plot = Cast<ATerracePlotActor>(Component->GetOwner()))
            {
                DrawPlot(Plot, PDI);
            }
            else if (const ATerraceGroupActor* Group = Cast<ATerraceGroupActor>(Component->GetOwner()))
            {
                for (const TSoftObjectPtr<ATerracePlotActor>& Member : Group->Plots)
                {
                    const ATerracePlotActor* MemberPlot = Member.Get();
                    if (IsValid(MemberPlot) && MemberPlot->GetWorld() == Group->GetWorld())
                    {
                        DrawPlot(MemberPlot, PDI);
                    }
                }
            }
        }

    private:
        static void DrawPlot(const ATerracePlotActor* Plot, FPrimitiveDrawInterface* PDI)
        {
            // Sampling is O(n); expensive validity checks only run on explicit validation.
            TArray<FVector2D> Points;
            FText Error;
            if (!Plot->SampleBoundary(Points, Error) || Points.Num() < 3)
            {
                return;
            }
            const double Z = Plot->GetWorldElevation();
            for (int32 I = 0; I < Points.Num(); ++I)
            {
                const FVector A(Points[I], Z);
                const FVector B(Points[(I + 1) % Points.Num()], Z);
                PDI->DrawLine(A, B, FLinearColor(0.0f, 0.8f, 1.0f), SDPG_World, 2.0f);
                if (I % 4 == 0)
                {
                    PDI->DrawLine(A, A + FVector(0, 0, Plot->Settings.BundHeight),
                        FLinearColor(1.0f, 0.65f, 0.1f), SDPG_World, 1.0f);
                }
            }
        }
    };
}

class FTerraceToolsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        SplineChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this,
            &FTerraceToolsEditorModule::OnObjectPropertyChanged);
        FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyEditor.RegisterCustomClassLayout(ATerraceGroupActor::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FTerraceActorDetails::MakeInstance));
        PropertyEditor.RegisterCustomClassLayout(ATerracePlotActor::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FTerraceActorDetails::MakeInstance));
        PropertyEditor.NotifyCustomizationModuleChanged();
        if (GUnrealEd)
        {
            TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FTerracePreviewVisualizer>();
            GUnrealEd->RegisterComponentVisualizer(UTerracePreviewComponent::StaticClass()->GetFName(), Visualizer);
            Visualizer->OnRegister();
        }
    }

    virtual void ShutdownModule() override
    {
        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(SplineChangedHandle);
        if (GUnrealEd)
        {
            GUnrealEd->UnregisterComponentVisualizer(TEXT("TerracePreviewComponent"));
        }
        if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
        {
            PropertyEditor->UnregisterCustomClassLayout(TEXT("TerraceGroupActor"));
            PropertyEditor->UnregisterCustomClassLayout(TEXT("TerracePlotActor"));
            PropertyEditor->NotifyCustomizationModuleChanged();
        }
    }
private:
    void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
    {
        if ((Event.ChangeType & EPropertyChangeType::Interactive) != 0 || (GEditor && GEditor->PlayWorld)) { return; }
        USplineComponent* Spline = Cast<USplineComponent>(Object);
        ATerracePlotActor* Plot = Spline ? Cast<ATerracePlotActor>(Spline->GetOwner()) : nullptr;
        if (IsValid(Plot) && !Plot->IsTemplate() && Spline == Plot->BoundarySpline
            && Plot->GetWorld() && Plot->GetWorld()->WorldType == EWorldType::Editor)
        {
            FText Report;
            Plot->RefreshPCGOutput(Report);
        }
    }

    FDelegateHandle SplineChangedHandle;
};

IMPLEMENT_MODULE(FTerraceToolsEditorModule, TerraceToolsEditor)

#undef LOCTEXT_NAMESPACE
