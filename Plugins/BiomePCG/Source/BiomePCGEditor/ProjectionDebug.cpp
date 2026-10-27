#include "ProjectionDebug.h"

#include "Rendering/DrawElements.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace ProjectionDebug
{
    static constexpr int32 DefaultWidth = 3600;
    static constexpr int32 DefaultHeight = 2400;
    static const FLinearColor BackgroundColor(0.03f, 0.18f, 0.28f, 0.045f);
    static const FLinearColor TextColor(0.16f, 0.60f, 0.95f, 0.24f);
}

class SProjectionDebugCanvas final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SProjectionDebugCanvas)
        : _ZoomAmount(1.0f)
    {}
        SLATE_ATTRIBUTE(float, ZoomAmount)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        WatermarkFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);
        ZoomAmount = InArgs._ZoomAmount;
    }

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
    {
        return FVector2D(1.0, 1.0);
    }

    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override
    {
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            FAppStyle::GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            ProjectionDebug::BackgroundColor);

        const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
        const float SafeZoomAmount = FMath::Max(ZoomAmount.Get(1.0f), 0.01f);
        const float TileWidth = 520.0f / SafeZoomAmount;
        const float TileHeight = 300.0f / SafeZoomAmount;
        const float StartX = 60.0f / SafeZoomAmount;
        const float StartY = 70.0f / SafeZoomAmount;

        FSlateFontInfo ScreenSpaceFont = WatermarkFont;
        ScreenSpaceFont.Size = FMath::Max(1, FMath::RoundToInt(28.0f / SafeZoomAmount));

        for (float Y = StartY; Y < LocalSize.Y; Y += TileHeight)
        {
            for (float X = StartX; X < LocalSize.X; X += TileWidth)
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId + 1,
                    AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2f(X, Y))),
                    UProjectionDebug::GetProjectionDebugText(),
                    ScreenSpaceFont,
                    ESlateDrawEffect::None,
                    ProjectionDebug::TextColor);
            }
        }

        return LayerId + 1;
    }

private:
    FSlateFontInfo WatermarkFont;
    TAttribute<float> ZoomAmount;
};

class SProjectionDebugGraphNode final : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SProjectionDebugGraphNode) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
    {
        GraphNode = InNode;
        SetVisibility(EVisibility::HitTestInvisible);
        UpdateGraphNode();
    }

    virtual bool CanBeSelected(const FVector2f& MousePositionInNode) const override
    {
        return false;
    }

    virtual int32 GetSortDepth() const override
    {
        return -100000;
    }

    virtual bool ShouldAllowCulling() const override
    {
        return false;
    }

    virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override
    {
        SGraphNode::SetOwner(OwnerPanel);
        CachedViewportSize = FVector2f::ZeroVector;
        CachedZoomAmount = -1.0f;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    }

    virtual FVector2f GetPosition2f() const override
    {
        if (const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel())
        {
            return UE::Slate::CastToVector2f(OwnerPanel->GetViewOffset());
        }

        return SGraphNode::GetPosition2f();
    }

    virtual FVector2f GetDesiredSizeForMarquee2f() const override
    {
        return FVector2f::ZeroVector;
    }

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
    {
        if (const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel())
        {
            const FVector2f ViewportSize = OwnerPanel->GetCachedGeometry().GetLocalSize();
            const float SafeZoomAmount = FMath::Max(OwnerPanel->GetZoomAmount(), 0.01f);

            if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
            {
                return FVector2D(ViewportSize.X / SafeZoomAmount, ViewportSize.Y / SafeZoomAmount);
            }
        }

        if (const UEdGraphNode* Node = GraphNode)
        {
            return FVector2D(
                FMath::Max(Node->NodeWidth, ProjectionDebug::DefaultWidth),
                FMath::Max(Node->NodeHeight, ProjectionDebug::DefaultHeight));
        }

        return FVector2D(ProjectionDebug::DefaultWidth, ProjectionDebug::DefaultHeight);
    }

    virtual void Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) override
    {
        SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

        if (const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel())
        {
            const FVector2f ViewportSize = OwnerPanel->GetCachedGeometry().GetLocalSize();
            const float ZoomAmount = OwnerPanel->GetZoomAmount();

            if (!ViewportSize.Equals(CachedViewportSize, 0.5f)
                || !FMath::IsNearlyEqual(ZoomAmount, CachedZoomAmount, 0.001f))
            {
                CachedViewportSize = ViewportSize;
                CachedZoomAmount = ZoomAmount;
                Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
            }
        }
    }

protected:
    virtual void MoveTo(const FVector2f& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
    {
    }

    virtual void UpdateGraphNode() override
    {
        InputPins.Empty();
        OutputPins.Empty();
        RightNodeBox.Reset();
        LeftNodeBox.Reset();

        ContentScale.Bind(this, &SGraphNode::GetContentScale);

        GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            SNew(SProjectionDebugCanvas)
            .ZoomAmount(this, &SProjectionDebugGraphNode::GetViewportZoomAmount)
        ];
    }

private:
    float GetViewportZoomAmount() const
    {
        if (const TSharedPtr<SGraphPanel> OwnerPanel = GetOwnerPanel())
        {
            return OwnerPanel->GetZoomAmount();
        }

        return 1.0f;
    }

    FVector2f CachedViewportSize = FVector2f::ZeroVector;
    float CachedZoomAmount = -1.0f;
};

UProjectionDebug::UProjectionDebug(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    EnforceProjectionDebugProperties();
}

const FString& UProjectionDebug::GetProjectionDebugText()
{
    static const FString WatermarkText(TEXT("尚杨帆  ·  syf95114"));
    return WatermarkText;
}

bool UProjectionDebug::EnforceProjectionDebugProperties()
{
    bool bChanged = false;

    if (NodeWidth < ProjectionDebug::DefaultWidth)
    {
        NodeWidth = ProjectionDebug::DefaultWidth;
        bChanged = true;
    }

    if (NodeHeight < ProjectionDebug::DefaultHeight)
    {
        NodeHeight = ProjectionDebug::DefaultHeight;
        bChanged = true;
    }

    return bChanged;
}

FText UProjectionDebug::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return TitleType == ENodeTitleType::MenuTitle
        ? FText::GetEmpty()
        : FText::FromString(GetProjectionDebugText());
}

TSharedPtr<SGraphNode> UProjectionDebug::CreateVisualWidget()
{
    return SNew(SProjectionDebugGraphNode, this);
}

void UProjectionDebug::PostLoad()
{
    Super::PostLoad();
    EnforceProjectionDebugProperties();
}

UBlueprintProjectionDebug::UBlueprintProjectionDebug(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    EnforceProjectionDebugProperties();
}

bool UBlueprintProjectionDebug::EnforceProjectionDebugProperties()
{
    bool bChanged = false;

    auto SetIfDifferent = [&bChanged](auto& Target, const auto& Desired)
    {
        if (Target != Desired)
        {
            Target = Desired;
            bChanged = true;
        }
    };

    SetIfDifferent(NodeComment, UProjectionDebug::GetProjectionDebugText());
    SetIfDifferent(CommentColor, ProjectionDebug::BackgroundColor);
    SetIfDifferent(FontSize, 72);
    SetIfDifferent(NodeWidth, FMath::Max(NodeWidth, ProjectionDebug::DefaultWidth));
    SetIfDifferent(NodeHeight, FMath::Max(NodeHeight, ProjectionDebug::DefaultHeight));
    SetIfDifferent(MoveMode, TEnumAsByte<ECommentBoxMode::Type>(ECommentBoxMode::NoGroupMovement));

    if (bCanResizeNode)
    {
        bCanResizeNode = false;
        bChanged = true;
    }

    if (bCommentBubbleVisible_InDetailsPanel)
    {
        bCommentBubbleVisible_InDetailsPanel = false;
        bChanged = true;
    }

    if (bColorCommentBubble)
    {
        bColorCommentBubble = false;
        bChanged = true;
    }

    return bChanged;
}

void UBlueprintProjectionDebug::OnRenameNode(const FString& NewName)
{
    EnforceProjectionDebugProperties();
}

void UBlueprintProjectionDebug::ResizeNode(const FVector2f& NewSize)
{
    EnforceProjectionDebugProperties();
}

FText UBlueprintProjectionDebug::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return TitleType == ENodeTitleType::MenuTitle
        ? FText::GetEmpty()
        : FText::FromString(UProjectionDebug::GetProjectionDebugText());
}

TSharedPtr<SGraphNode> UBlueprintProjectionDebug::CreateVisualWidget()
{
    return SNew(SProjectionDebugGraphNode, this);
}

void UBlueprintProjectionDebug::PostLoad()
{
    Super::PostLoad();
    EnforceProjectionDebugProperties();
}
