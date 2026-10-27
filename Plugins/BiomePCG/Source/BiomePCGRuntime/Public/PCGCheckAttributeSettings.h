// PCG Check Attribute Node
// 检查输入点是否存在指定的 AttributeSet，并输出布尔值结果

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGCheckAttributeSettings.generated.h"

class FPCGCheckAttributeElement;

/**
 * PCG 检查属性节点设置
 * 功能：检查输入点数据中是否存在指定名称的 AttributeSet，
 *       如果存在则输出布尔值 true，否则输出 false
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class BIOMEPCGRUNTIME_API UPCGCheckAttributeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGCheckAttributeSettings();

	//~ Begin UPCGSettings interface
	virtual bool UseSeed() const override { return false; }
	virtual FPCGDataTypeIdentifier GetCurrentPinTypesID(const UPCGPin* InPin) const override;
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CheckAttribute")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.8f, 0.4f, 0.0f); }
#endif

	virtual FPCGElementPtr CreateElement() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~ End UPCGSettings interface

	/** 要检查的 AttributeSet 名称（从输入数据中选择 AttributeSet） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Check Attribute", meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector CheckName;

	/** 输出 AttributeSet 的名称 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Check Attribute", meta = (PCG_Overridable))
	FName OutputName = TEXT("Exist");
};

/** PCG 元素：实现检查 AttributeSet 是否存在的逻辑 */
class BIOMEPCGRUNTIME_API FPCGCheckAttributeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
