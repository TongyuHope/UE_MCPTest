#include "SplinePainter/BiomeSplinePainterToolkit.h"

#include "BiomePaintedSplineComponent.h"
#include "BiomeSplinePainterActor.h"
#include "SplinePainter/BiomeSplinePainterEdMode.h"
#include "SplinePainter/BiomeSplinePainterSettings.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BiomeSplinePainterToolkit"

FBiomeSplinePainterToolkit::FBiomeSplinePainterToolkit(FBiomeSplinePainterEdMode* InMode)
    : Painter(InMode)
{
    FDetailsViewArgs Args;
    Args.bAllowSearch = false;
    Args.bHideSelectionTip = true;
    Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    TSharedRef<IDetailsView> Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Args);
    Details->SetObject(Painter->GetSettings());

    auto Button = [](const FText& Text, TFunction<void()> Action)
    {
        return SNew(SButton).Text(Text).OnClicked_Lambda([Action]()
        {
            Action();
            return FReply::Handled();
        });
    };

    Content = SNew(SScrollBox)
        + SScrollBox::Slot().Padding(8)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
            [
                SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Instructions",
                    "单击地表放置 Actor（此次不画线），然后选择绘制方式。\n长按拖动：按住画线，松开提交。\n逐点点击：单击确认各点，Enter 或“完成当前线”提交，再画下一条；Backspace 回退一点。\nEsc 取消当前线；空闲时 Esc 退出。Alt/右键保留视口导航。\n完成后可用原生样条控制点继续编辑，请保存关卡和新建蓝图。"))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
            [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return Painter->GetStatus(); }) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Button(LOCTEXT("UseSelection", "使用选中的绘制 Actor"), [this]() { Painter->UseSelection(); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
            [ Button(LOCTEXT("NewActor", "放置新的 Actor"), [this]() { Painter->NewTarget(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Details ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
            [
                SNew(SButton)
                .Text(LOCTEXT("CompleteLine", "完成当前线 / 绘制下一条（Enter）"))
                .IsEnabled_Lambda([this]() { return Painter->CanCompleteCurrentSpline(); })
                .OnClicked_Lambda([this]()
                {
                    Painter->CompleteCurrentSpline();
                    return FReply::Handled();
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
            [
                SNew(SButton)
                .Text(LOCTEXT("RemoveLastPoint", "回退最后一点（Backspace）"))
                .IsEnabled_Lambda([this]() { return Painter->HasPendingPointSpline(); })
                .OnClicked_Lambda([this]()
                {
                    Painter->RemoveLastClickedPoint();
                    return FReply::Handled();
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
            [
                SNew(SButton)
                .Text(LOCTEXT("CancelLine", "取消当前逐点线"))
                .IsEnabled_Lambda([this]() { return Painter->HasPendingPointSpline(); })
                .OnClicked_Lambda([this]()
                {
                    Painter->CancelCurrentSpline();
                    return FReply::Handled();
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
            [ Button(LOCTEXT("ApplyGraph", "将图和自动刷新设置应用到目标"), [this]() { Painter->ApplyGraph(); }) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Button(LOCTEXT("Refresh", "刷新目标 PCG"), [this]() { Painter->RefreshPCG(); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
            [ SNew(STextBlock).Text(LOCTEXT("Splines", "已保存样条（点击选择并高亮）")) ]
            + SVerticalBox::Slot().AutoHeight()[ SAssignNew(SplineList, SVerticalBox) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
            [ Button(LOCTEXT("Delete", "删除选中样条"), [this]() { Painter->DeleteSelectedSpline(); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
            [ Button(LOCTEXT("Finish", "完成绘制"), [this]() { Painter->Finish(); }) ]
        ];
    UpdateSplineList();
}

FText FBiomeSplinePainterToolkit::GetBaseToolkitName() const
{
    return LOCTEXT("Title", "Biome 样条植被绘制");
}

FEdMode* FBiomeSplinePainterToolkit::GetEditorMode() const
{
    return Painter;
}

void FBiomeSplinePainterToolkit::UpdateSplineList()
{
    const ABiomeSplinePainterActor* Target = Painter->GetSettings()->TargetActor.Get();
    TArray<TWeakObjectPtr<UBiomePaintedSplineComponent>> Current;
    if (IsValid(Target))
    {
        for (UBiomePaintedSplineComponent* Spline : Target->GetPaintedSplines())
        {
            Current.Add(Spline);
        }
    }
    if (Current == CachedSplines)
    {
        return;
    }
    CachedSplines = Current;
    SplineList->ClearChildren();
    for (TWeakObjectPtr<UBiomePaintedSplineComponent> WeakSpline : Current)
    {
        SplineList->AddSlot().AutoHeight().Padding(0, 2)
        [
            SNew(SButton)
            .Text_Lambda([WeakSpline]()
            {
                const UBiomePaintedSplineComponent* Spline = WeakSpline.Get();
                return IsValid(Spline) ? FText::FromString(FString::Printf(TEXT("%s  |  %.0f cm  |  %d 点"),
                    *Spline->StrokeLabel, Spline->GetSplineLength(), Spline->GetNumberOfSplinePoints())) : FText::GetEmpty();
            })
            .OnClicked_Lambda([this, WeakSpline]()
            {
                Painter->SelectSpline(WeakSpline.Get());
                return FReply::Handled();
            })
        ];
    }
}

#undef LOCTEXT_NAMESPACE
