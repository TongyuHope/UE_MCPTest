// PCG Check String Node
// 检测输入属性字符串中是否包含指定的子字符串，并输出布尔值结果

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "PCGCheckString.generated.h"

class FPCGCheckStringElement;

/**
 * PCG Check String Node Settings
 * 功能：检测输入 AttributeSet 中的字符串值是否包含 Search String 子字符串
 * - In（AttributeSet）：输入 Param 类型数据，节点从其中读取字符串属性
 * - Search String：要检测的子字符串
 * - Out：输出布尔值（包含为 true，不包含为 false）到指定的 AttributeSet
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class BIOMEPCGRUNTIME_API UPCGCheckStringSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGCheckStringSettings();

	//~ Begin UPCGSettings interface
	virtual bool UseSeed() const override { return false; }
	virtual FPCGDataTypeIdentifier GetCurrentPinTypesID(const UPCGPin* InPin) const override;
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Check String")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.6f, 0.2f, 0.8f); }
#endif

	virtual FPCGElementPtr CreateElement() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~ End UPCGSettings interface

	/** 要检测的子字符串 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Check String", meta = (PCG_Overridable))
	FString SearchString = TEXT("");

	/** 输出 AttributeSet 的名称（布尔结果将写入此 AttributeSet） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Check String", meta = (PCG_Overridable))
	FName Out = TEXT("Out");
};

/** PCG Element：实现字符串包含检测逻辑 */
class BIOMEPCGRUNTIME_API FPCGCheckStringElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
