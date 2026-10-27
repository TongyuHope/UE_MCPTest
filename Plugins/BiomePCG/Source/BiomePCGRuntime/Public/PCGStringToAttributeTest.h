// PCG StringToAttributeTest Node
// 将输入属性中的字符串按指定分隔符拆分，输出到 M1, M2, M3... 等属性

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "PCGStringToAttributeTest.generated.h"

class FPCGStringToAttributeTestElement;

/**
 * PCG StringToAttributeTest 节点设置
 * 功能：读取输入属性中的字符串值，按指定分隔符拆分，
 *       将拆分结果依次写入输出属性 M1, M2, M3...
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class BIOMEPCGRUNTIME_API UPCGStringToAttributeTestSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGStringToAttributeTestSettings();

	//~ Begin UPCGSettings interface
	virtual bool UseSeed() const override { return false; }
	virtual FPCGDataTypeIdentifier GetCurrentPinTypesID(const UPCGPin* InPin) const override;
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("StringToAttributeTest")); }
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "String To Attribute", meta = (PCG_Overridable))
	FString InputName = TEXT("Materials_String");

	/** 输出属性基础名称：拆分结果将写入 M1, M2, M3...（当基础名为 M 时） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "String To Attribute", meta = (PCG_Overridable))
	FString OutputName = TEXT("M");

	/** 分隔符：用于拆分输入字符串的字符 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "String To Attribute", meta = (PCG_Overridable))
	FString SplitString = TEXT(",");
};

/** PCG 元素：实现字符串拆分逻辑 */
class FPCGStringToAttributeTestElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
