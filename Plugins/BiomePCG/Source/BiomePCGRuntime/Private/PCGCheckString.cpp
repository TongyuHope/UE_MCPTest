// PCG Check String Node Implementation

#include "PCGCheckString.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGParamData.h"

#define LOCTEXT_NAMESPACE "PCGCheckString"

UPCGCheckStringSettings::UPCGCheckStringSettings()
{
}

FPCGDataTypeIdentifier UPCGCheckStringSettings::GetCurrentPinTypesID(const UPCGPin* InPin) const
{
	if (!InPin)
	{
		return EPCGDataType::Param;
	}

	const FName PinName = InPin->GetFName();
	if (PinName == PCGPinConstants::DefaultInputLabel || PinName == TEXT("In"))
	{
		return EPCGDataType::Param;
	}
	else if (PinName == PCGPinConstants::DefaultOutputLabel || PinName == TEXT("Out"))
	{
		return EPCGDataType::Param;
	}

	return Super::GetCurrentPinTypesID(InPin);
}

#if WITH_EDITOR
FText UPCGCheckStringSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Check String");
}

FText UPCGCheckStringSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"检测输入 AttributeSet 中的字符串值是否包含 Search String 所填的字符串。\n"
		"如果包含则返回 true，否则返回 false。\n\n"
		"例如：\n"
		"  输入 AttributeSet 中字符串 = \"abcde123\"\n"
		"  Search String = \"bcd\"  -> 返回 true\n"
		"  Search String = \"13\"   -> 返回 false\n\n"
		"结果将写入 Out 所填的 AttributeSet 中（Param 类型输出）。");
}
#endif

FPCGElementPtr UPCGCheckStringSettings::CreateElement() const
{
	return MakeShared<FPCGCheckStringElement>();
}

TArray<FPCGPinProperties> UPCGCheckStringSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	FPCGPinProperties& InPin = Properties.Emplace_GetRef(TEXT("In"), EPCGDataType::Param);
	InPin.SetRequiredPin();

	return Properties;
}

TArray<FPCGPinProperties> UPCGCheckStringSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	FPCGPinProperties& OutPin = Properties.Emplace_GetRef(TEXT("Out"), EPCGDataType::Param);

	return Properties;
}

bool FPCGCheckStringElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCheckStringElement::Execute);

	check(Context);

	const UPCGCheckStringSettings* Settings = Context->GetInputSettings<UPCGCheckStringSettings>();
	if (!Settings)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoSettings", "Failed to get input settings"));
		return true;
	}

	const FString SearchString = Settings->SearchString;
	const FName OutName = Settings->Out;

	if (OutName.IsNone())
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidOutName", "Out cannot be empty"));
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(TEXT("In"));
	if (Inputs.IsEmpty())
	{
		Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	}

	if (Inputs.IsEmpty())
	{
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGParamData* ParamInput = Cast<UPCGParamData>(Input.Data);

		if (!ParamInput)
		{
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const UPCGMetadata* MetadataIn = ParamInput->ConstMetadata();
		if (!MetadataIn)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("NoMetadata", "Input AttributeSet has no metadata"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		MetadataIn->GetAttributes(AttributeNames, AttributeTypes);

		FString AttributeSetValue;
		bool bFoundString = false;

		for (int32 i = 0; i < AttributeNames.Num(); ++i)
		{
			if (AttributeTypes[i] == EPCGMetadataTypes::String)
			{
				const FPCGMetadataAttribute<FString>* StringAttr = MetadataIn->GetConstTypedAttribute<FString>(
					FPCGAttributeIdentifier(AttributeNames[i], PCGMetadataDomainID::Default));
				if (StringAttr)
				{
					constexpr PCGMetadataEntryKey StringEntryKey = 0;
					AttributeSetValue = StringAttr->GetValueFromItemKey(StringEntryKey);
					bFoundString = true;
					break;
				}
			}
		}

		if (!bFoundString)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("NoStringAttr", "Input AttributeSet does not contain a string attribute"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const bool bContains = !SearchString.IsEmpty() && AttributeSetValue.Contains(SearchString);

		UPCGParamData* OutputParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
		if (!OutputParamData)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToCreateOutput", "Failed to create output AttributeSet"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		UPCGMetadata* MetadataOut = OutputParamData->MutableMetadata();
		if (!MetadataOut)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToGetMetadata", "Failed to get output metadata"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		const PCGMetadataEntryKey EntryKey = MetadataOut->AddEntry();

		FPCGMetadataAttribute<bool>* ResultAttr = MetadataOut->FindOrCreateAttribute<bool>(
			FPCGAttributeIdentifier(OutName, PCGMetadataDomainID::Default),
			false,
			false,
			true);

		if (!ResultAttr)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToCreateAttribute", "Failed to create output boolean attribute"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		ResultAttr->SetValue(EntryKey, bContains);

		PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(
			LOCTEXT("CheckResult",
				"Check String: Input=\"{0}\" contains \"{1}\" = {2}"),
			FText::FromString(AttributeSetValue),
			FText::FromString(SearchString),
			bContains ? LOCTEXT("True", "true") : LOCTEXT("False", "false")));

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = OutputParamData;
		Output.Pin = TEXT("Out");
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
