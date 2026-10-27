// PCG StringToAttributeTest Node Implementation

#include "PCGStringToAttributeTest.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataCommon.h"

#define LOCTEXT_NAMESPACE "PCGStringToAttributeTest"

UPCGStringToAttributeTestSettings::UPCGStringToAttributeTestSettings()
{
}

FPCGDataTypeIdentifier UPCGStringToAttributeTestSettings::GetCurrentPinTypesID(const UPCGPin* InPin) const
{
	const FName PinName = InPin->GetFName();
	if (PinName == PCGPinConstants::DefaultInputLabel || PinName == PCGPinConstants::DefaultOutputLabel)
	{
		return EPCGDataType::Point;
	}
	return Super::GetCurrentPinTypesID(InPin);
}

#if WITH_EDITOR
FText UPCGStringToAttributeTestSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "StringToAttributeTest");
}

FText UPCGStringToAttributeTestSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"将输入属性中的字符串按指定分隔符拆分，"
		"并将拆分结果写入输出属性 M1, M2, M3...");
}
#endif

FPCGElementPtr UPCGStringToAttributeTestSettings::CreateElement() const
{
	return MakeShared<FPCGStringToAttributeTestElement>();
}

TArray<FPCGPinProperties> UPCGStringToAttributeTestSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	FPCGPinProperties& InputPin = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPin.SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> UPCGStringToAttributeTestSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

bool FPCGStringToAttributeTestElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStringToAttributeTestElement::Execute);

	check(Context);

	const UPCGStringToAttributeTestSettings* Settings = Context->GetInputSettings<UPCGStringToAttributeTestSettings>();
	check(Settings);

	const FString InputAttrName = Settings->InputName;
	const FString OutputAttrBase = Settings->OutputName;
	const FString SplitStr = Settings->SplitString;

	if (InputAttrName.IsEmpty() || OutputAttrBase.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidAttributeNames", "InputName 或 OutputName 不能为空"));
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.Num() == 0)
	{
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialInput = Cast<UPCGSpatialData>(Input.Data);
		if (!SpatialInput)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const UPCGPointData* PointDataIn = SpatialInput->ToPointData(Context);
		if (!PointDataIn)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const UPCGMetadata* MetadataIn = PointDataIn->Metadata;
		if (!MetadataIn)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("NoMetadata", "输入点数据没有元数据"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const FName InputAttrFName = FName(*InputAttrName);
		const FPCGAttributeIdentifier InputAttrId(InputAttrFName, PCGMetadataDomainID::Elements);
		const FPCGMetadataAttribute<FString>* InputAttr = MetadataIn->GetConstTypedAttribute<FString>(InputAttrId);
		if (!InputAttr)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("InputAttrNotFound", "未找到输入属性 \"{0}\""), FText::FromString(InputAttrName)));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		// 复制输入数据作为输出
		UPCGData* DuplicatedData = Input.Data->DuplicateData(Context);
		if (!DuplicatedData)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		UPCGPointData* PointDataOut = Cast<UPCGPointData>(DuplicatedData);
		if (!PointDataOut)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		UPCGMetadata* MetadataOut = PointDataOut->MutableMetadata();
		if (!MetadataOut)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const TArray<FPCGPoint>& Points = PointDataIn->GetPoints();
		const int32 NumPoints = Points.Num();

		// 预先确定最大拆分数量，用于创建属性
		int32 MaxSplitCount = 0;
		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			const PCGMetadataEntryKey EntryKey = PointDataIn->GetMetadataEntry(PointIdx);
			FString StrValue = InputAttr->GetValueFromItemKey(EntryKey);
			TArray<FString> Parts;
			StrValue.ParseIntoArray(Parts, *SplitStr, true);
			MaxSplitCount = FMath::Max(MaxSplitCount, Parts.Num());
		}

		// 创建输出属性 M1, M2, M3... 并获取可变属性指针
		TArray<FPCGMetadataAttribute<FString>*> OutAttrs;
		OutAttrs.Reserve(MaxSplitCount);
		for (int32 i = 1; i <= MaxSplitCount; ++i)
		{
			FString AttrNameStr = FString::Printf(TEXT("%s%d"), *OutputAttrBase, i);
			FName AttrName = FName(*AttrNameStr);
			FPCGMetadataAttribute<FString>* OutAttr = MetadataOut->FindOrCreateAttribute<FString>(
				FPCGAttributeIdentifier(AttrName, PCGMetadataDomainID::Elements),
				FString(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
			OutAttrs.Add(OutAttr);
		}

		// 为每个点执行拆分并写入
		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			const PCGMetadataEntryKey EntryKey = PointDataIn->GetMetadataEntry(PointIdx);
			FString StrValue = InputAttr->GetValueFromItemKey(EntryKey);

			TArray<FString> Parts;
			StrValue.ParseIntoArray(Parts, *SplitStr, true);

			// 获取输出点的 EntryKey（复制后相同）
			const PCGMetadataEntryKey OutEntryKey = PointDataOut->GetMetadataEntry(PointIdx);

			// 从 1 开始循环到 Parts.Num()，将值写入 M1, M2, M3...
			for (int32 i = 1; i <= Parts.Num(); ++i)
			{
				FPCGMetadataAttribute<FString>* OutAttr = OutAttrs[i - 1];
				if (OutAttr)
				{
					// 清理每段首尾的括号和引号
					FString CleanValue = Parts[i - 1].TrimStartAndEnd();
					if (CleanValue.StartsWith(TEXT("(")))
					{
						CleanValue = CleanValue.Mid(1).TrimStartAndEnd();
					}
					if (CleanValue.StartsWith(TEXT("\"")))
					{
						CleanValue = CleanValue.Mid(1);
					}
					if (CleanValue.EndsWith(TEXT(")")))
					{
						CleanValue = CleanValue.LeftChop(1).TrimEnd();
					}
					if (CleanValue.EndsWith(TEXT("\"")))
					{
						CleanValue = CleanValue.LeftChop(1);
					}
					OutAttr->SetValue(OutEntryKey, CleanValue);
				}
			}
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointDataOut;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
