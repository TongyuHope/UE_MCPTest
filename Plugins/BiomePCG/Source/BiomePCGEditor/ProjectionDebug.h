#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "ProjectionDebug.generated.h"

/**
 * Editor-only graph projection overlay. The visual is anchored to the graph viewport,
 * remains screen-sized while zooming, and cannot be selected or deleted through
 * normal graph-editor interaction.
 */
UCLASS()
class BIOMEPCGEDITOR_API UProjectionDebug : public UEdGraphNode
{
    GENERATED_BODY()

public:
    UProjectionDebug(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual bool CanUserDeleteNode() const override { return false; }
    virtual bool CanDuplicateNode() const override { return false; }
    virtual bool GetCanRenameNode() const override { return false; }
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
    virtual void PostLoad() override;

    bool EnforceProjectionDebugProperties();
    static const FString& GetProjectionDebugText();
};

/** Blueprint-compatible projection overlay. Kismet requires decorative graph nodes to
 * derive from a supported comment/documentation class. */
UCLASS()
class BIOMEPCGEDITOR_API UBlueprintProjectionDebug : public UEdGraphNode_Comment
{
    GENERATED_BODY()

public:
    UBlueprintProjectionDebug(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual bool CanUserDeleteNode() const override { return false; }
    virtual bool CanDuplicateNode() const override { return false; }
    virtual bool GetCanRenameNode() const override { return false; }
    virtual void OnRenameNode(const FString& NewName) override;
    virtual void ResizeNode(const FVector2f& NewSize) override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
    virtual void PostLoad() override;

    bool EnforceProjectionDebugProperties();
};
