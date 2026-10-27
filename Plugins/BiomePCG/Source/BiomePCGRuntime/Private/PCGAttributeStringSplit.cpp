// PCG Attribute String Split Node Implementation

#include "PCGAttributeStringSplit.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataCommon.h"

#define LOCTEXT_NAMESPACE "PCGAttributeStringSplit"

// 解析格式如 ("val1","val2","val3") 的字符串，拆分为独立项
// 支持引号内包含逗号、括号等情况
static void ParseQuotedArrayString(const FString& InStr, TArray<FString>& OutItems)
{
	OutItems.Reset();
	FString Trimmed = InStr.TrimStartAndEnd();
	if (Trimmed.IsEmpty()) return;

	// 移除最外层括号
	if (Trimmed.StartsWith(TEXT("(")) && Trimmed.EndsWith(TEXT(")")))
	{
		Trimmed = Trimmed.Mid(1, Trimmed.Len() - 2).TrimStartAndEnd();
	}

	if (Trimmed.IsEmpty()) return;

	// 按逗号拆分，但需考虑引号内的逗号
	TArray<FString> RawParts;
	FString Current;
	bool bInQuotes = false;
	const TCHAR QuoteChar = TEXT('"');

	for (int32 i = 0; i < Trimmed.Len(); ++i)
	{
		TCHAR C = Trimmed[i];
		if (C == QuoteChar)
		{
			bInQuotes = !bInQuotes;
			Current.AppendChar(C);
		}
		else if (C == TEXT(',') && !bInQuotes)
		{
			RawParts.Add(Current.TrimStartAndEnd());
			Current.Empty();
		}
		else
		{
			Current.AppendChar(C);
		}
	}
	if (!Current.IsEmpty())
	{
		RawParts.Add(Current.TrimStartAndEnd());
	}

	// 去除每项首尾引号
	for (const FString& Part : RawParts)
	{
		FString Clean = Part.TrimStartAndEnd();
		if (Clean.Len() >= 2 && Clean.StartsWith(TEXT("\"")) && Clean.EndsWith(TEXT("\"")))
		{
			Clean = Clean.Mid(1, Clean.Len() - 2);
		}
		OutItems.Add(Clean);
	}
}

UPCGAttributeStringSplitSettings::UPCGAttributeStringSplitSettings()
{
}

FPCGDataTypeIdentifier UPCGAttributeStringSplitSettings::GetCurrentPinTypesID(const UPCGPin* InPin) const
{
	const FName PinName = InPin->GetFName();
	if (PinName == PCGPinConstants::DefaultInputLabel || PinName == PCGPinConstants::DefaultOutputLabel)
	{
		return EPCGDataType::Point;
	}
	return Super::GetCurrentPinTypesID(InPin);
}

#if WITH_EDITOR
FText UPCGAttributeStringSplitSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Attribute String Split");
}

FText UPCGAttributeStringSplitSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"将输入属性中的字符串（格式如 (\"val1\",\"val2\")）拆分为多个独立值，"
		"并写入输出属性 M1, M2, M3...");
}
#endif

FPCGElementPtr UPCGAttributeStringSplitSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeStringSplitElement>();
}

TArray<FPCGPinProperties> UPCGAttributeStringSplitSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	FPCGPinProperties& InputPin = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPin.SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> UPCGAttributeStringSplitSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

bool FPCGAttributeStringSplitElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeStringSplitElement::Execute);

	check(Context);

	const UPCGAttributeStringSplitSettings* Settings = Context->GetInputSettings<UPCGAttributeStringSplitSettings>();
	check(Settings);

	const FName InputAttrName = Settings->InputSourceName;
	const FName OutputAttrBase = Settings->OutputSourceName;

	if (InputAttrName.IsNone() || OutputAttrBase.IsNone())
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidAttributeNames", "InputSourceName 或 OutputSourceName 不能为空"));
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

		const FPCGAttributeIdentifier InputAttrId(InputAttrName, PCGMetadataDomainID::Elements);
		const FPCGMetadataAttribute<FString>* InputAttr = MetadataIn->GetConstTypedAttribute<FString>(InputAttrId);
		if (!InputAttr)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				LOCTEXT("InputAttrNotFound", "未找到输入属性 \"{0}\""), FText::FromName(InputAttrName)));
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
			ParseQuotedArrayString(StrValue, Parts);
			MaxSplitCount = FMath::Max(MaxSplitCount, Parts.Num());
		}

		// 创建输出属性 M1, M2, M3... 并获取可变属性指针（使用 Elements 域以匹配点数据）
		TArray<FPCGMetadataAttribute<FString>*> OutAttrs;
		OutAttrs.Reserve(MaxSplitCount);
		for (int32 i = 1; i <= MaxSplitCount; ++i)
		{
			FName AttrName = FName(*FString::Printf(TEXT("%s%d"), *OutputAttrBase.ToString(), i));
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
			ParseQuotedArrayString(StrValue, Parts);

			// 获取输出点的 EntryKey（复制后相同）
			const PCGMetadataEntryKey OutEntryKey = PointDataOut->GetMetadataEntry(PointIdx);

			// 从 1 开始循环到 Parts.Num()，将值写入 M1, M2, M3...
			for (int32 i = 1; i <= Parts.Num(); ++i)
			{
				FPCGMetadataAttribute<FString>* OutAttr = OutAttrs[i - 1];
				if (OutAttr)
				{
					OutAttr->SetValue(OutEntryKey, Parts[i - 1]);
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
