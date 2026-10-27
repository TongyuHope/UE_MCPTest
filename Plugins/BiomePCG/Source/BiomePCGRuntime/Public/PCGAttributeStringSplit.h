// PCG Attribute String Split Node
// 将每个点上的 AttributeSet 字符串按逗号拆分，输出到 M1, M2, M3... 等属性

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "PCGAttributeStringSplit.generated.h"

class FPCGAttributeStringSplitElement;

/**
 * PCG 字符串拆分节点设置
 * 功能：将输入属性（如 In）中的字符串值拆分，格式如 ("val1","val2","val3")，
 *       将拆分结果依次写入输出属性 M1, M2, M3...
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class BIOMEPCGRUNTIME_API UPCGAttributeStringSplitSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGAttributeStringSplitSettings();

	//~ Begin UPCGSettings interface
	virtual bool UseSeed() const override { return false; }
	virtual FPCGDataTypeIdentifier GetCurrentPinTypesID(const UPCGPin* InPin) const override;
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AttributeStringSplit")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.0f, 0.5f, 0.8f); }
#endif

	virtual FPCGElementPtr CreateElement() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~ End UPCGSettings interface

	/** 输入属性名称：从点数据的 AttributeSet 中读取此名称对应的字符串值 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Attribute Split", meta = (PCG_Overridable))
	FName InputSourceName = TEXT("Materials_String");

	/** 输出属性基础名称：拆分结果将写入 M1, M2, M3...（当基础名为 M 时） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Attribute Split", meta = (PCG_Overridable))
	FName OutputSourceName = TEXT("M");
};

/** PCG 元素：实现字符串拆分逻辑 */
class FPCGAttributeStringSplitElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
