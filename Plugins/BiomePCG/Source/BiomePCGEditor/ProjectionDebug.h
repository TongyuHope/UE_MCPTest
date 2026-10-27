#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "ProjectionDebug.generated.h"

/**
 * Retired overlay type kept only so existing assets and historical redirects still load.
 * It no longer renders anything; the editor module removes saved instances on open/save.
 */
UCLASS()
class BIOMEPCGEDITOR_API UProjectionDebug : public UEdGraphNode
{
    GENERATED_BODY()

public:
    UProjectionDebug(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual bool CanUserDeleteNode() const override { return true; }
    virtual bool CanDuplicateNode() const override { return false; }
    virtual bool GetCanRenameNode() const override { return false; }
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
    virtual void PostLoad() override;

    bool EnforceProjectionDebugProperties();
    static const FString& GetProjectionDebugText();
};

/** Retired Blueprint comment-node compatibility type; no text or overlay is drawn. */
UCLASS()
class BIOMEPCGEDITOR_API UBlueprintProjectionDebug : public UEdGraphNode_Comment
{
    GENERATED_BODY()

public:
    UBlueprintProjectionDebug(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual bool CanUserDeleteNode() const override { return true; }
    virtual bool CanDuplicateNode() const override { return false; }
    virtual bool GetCanRenameNode() const override { return false; }
    virtual void OnRenameNode(const FString& NewName) override;
    virtual void ResizeNode(const FVector2f& NewSize) override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
    virtual void PostLoad() override;

    bool EnforceProjectionDebugProperties();
};
