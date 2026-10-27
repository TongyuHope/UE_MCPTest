#include "ProjectionDebug.h"

#include "SGraphNode.h"

/** Old assets may still contain these types until their next save. They render nothing. */
class SRetiredProjectionDebugNode final : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SRetiredProjectionDebugNode) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
    {
        GraphNode = InNode;
        SetVisibility(EVisibility::Collapsed);
    }

    virtual bool CanBeSelected(const FVector2f& MousePositionInNode) const override { return false; }
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }
    virtual FVector2f GetDesiredSizeForMarquee2f() const override { return FVector2f::ZeroVector; }
};

namespace
{
    bool ClearLegacyNodeVisuals(UEdGraphNode& Node)
    {
        const bool bChanged = !Node.NodeComment.IsEmpty() || Node.NodeWidth != 0 || Node.NodeHeight != 0;
        Node.NodeComment.Empty();
        Node.NodeWidth = 0;
        Node.NodeHeight = 0;
        return bChanged;
    }
}

UProjectionDebug::UProjectionDebug(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    EnforceProjectionDebugProperties();
}

const FString& UProjectionDebug::GetProjectionDebugText()
{
    static const FString EmptyText;
    return EmptyText;
}

bool UProjectionDebug::EnforceProjectionDebugProperties()
{
    return ClearLegacyNodeVisuals(*this);
}

FText UProjectionDebug::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return FText::GetEmpty();
}

TSharedPtr<SGraphNode> UProjectionDebug::CreateVisualWidget()
{
    return SNew(SRetiredProjectionDebugNode, this);
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
    const bool bChanged = ClearLegacyNodeVisuals(*this);
    CommentColor = FLinearColor::Transparent;
    bCommentBubbleVisible_InDetailsPanel = false;
    bCommentBubbleVisible = false;
    bColorCommentBubble = false;
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
    return FText::GetEmpty();
}

TSharedPtr<SGraphNode> UBlueprintProjectionDebug::CreateVisualWidget()
{
    return SNew(SRetiredProjectionDebugNode, this);
}

void UBlueprintProjectionDebug::PostLoad()
{
    Super::PostLoad();
    EnforceProjectionDebugProperties();
}
