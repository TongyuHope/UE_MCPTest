// PCG Check Attribute Node Implementation

#include "PCGCheckAttributeSettings.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGParamData.h"

#define LOCTEXT_NAMESPACE "PCGCheckAttribute"

UPCGCheckAttributeSettings::UPCGCheckAttributeSettings()
{
}

FPCGDataTypeIdentifier UPCGCheckAttributeSettings::GetCurrentPinTypesID(const UPCGPin* InPin) const
{
	if (!InPin)
	{
		return EPCGDataType::Point;
	}

	const FName PinName = InPin->GetFName();
	if (PinName == PCGPinConstants::DefaultInputLabel || PinName == TEXT("In"))
	{
		return EPCGDataType::Point;
	}
	else if (PinName == PCGPinConstants::DefaultOutputLabel || PinName == TEXT("AttributeSet"))
	{
		return EPCGDataType::Param;
	}
	
	return Super::GetCurrentPinTypesID(InPin);
}

#if WITH_EDITOR
FText UPCGCheckAttributeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Check Attribute");
}

FText UPCGCheckAttributeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"检查输入点数据中 Index=0 的点是否存在指定名称的 AttributeSet。"
		"\n如果存在，则在输出端创建名为 OutName 的 AttributeSet，并将布尔值设为 true。"
		"\n如果不存在，则在输出端创建名为 OutName 的 AttributeSet，并将布尔值设为 false。"
		"\n\n注意：无论 AttributeSet 是什么类型，只需要检查该名称的 AttributeSet 是否存在。");
}
#endif

FPCGElementPtr UPCGCheckAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGCheckAttributeElement>();
}

TArray<FPCGPinProperties> UPCGCheckAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	// In pin - accepts point data
	FPCGPinProperties& InPin = Properties.Emplace_GetRef(TEXT("In"), EPCGDataType::Point);
	InPin.SetRequiredPin();

	return Properties;
}

TArray<FPCGPinProperties> UPCGCheckAttributeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	// AttributeSet output pin - outputs AttributeSet data (Param type)
	FPCGPinProperties& OutPin = Properties.Emplace_GetRef(TEXT("AttributeSet"), EPCGDataType::Param);

	return Properties;
}

bool FPCGCheckAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCheckAttributeElement::Execute);

	check(Context);

	const UPCGCheckAttributeSettings* Settings = Context->GetInputSettings<UPCGCheckAttributeSettings>();
	if (!Settings)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoSettings", "Failed to get input settings"));
		return true;
	}

	const FName OutputName = Settings->OutputName;

	if (OutputName.IsNone())
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidOutputName", "OutputName 不能为空"));
		return true;
	}

	// Get input data from "In" pin
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

		const int32 NumPoints = PointDataIn->GetNumPoints();
		if (NumPoints == 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("NoPoints", "输入点数据没有点"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		// Get AttributeSet name from CheckName selector
		const FPCGAttributePropertyInputSelector& CheckNameSelector = Settings->CheckName;
		FName CheckName = CheckNameSelector.GetName();

		if (CheckName.IsNone())
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("InvalidCheckName", "CheckName 不能为空"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		// Check if AttributeSet with CheckName exists at Index=0
		// An AttributeSet exists if there's at least one attribute that starts with "CheckName."
		bool bAttributeSetExists = false;
		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		MetadataIn->GetAttributes(AttributeNames, AttributeTypes);

		FString CheckNamePrefix = CheckName.ToString() + TEXT(".");
		for (const FName& AttrName : AttributeNames)
		{
			FString AttrNameStr = AttrName.ToString();
			if (AttrNameStr.StartsWith(CheckNamePrefix))
			{
				bAttributeSetExists = true;
				break;
			}
			if (AttrName == CheckName)
			{
				bAttributeSetExists = true;
				break;
			}
		}

		// Create output AttributeSet (UPCGParamData)
		UPCGParamData* OutputParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
		if (!OutputParamData)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToCreateOutput", "无法创建输出 AttributeSet"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		UPCGMetadata* MetadataOut = OutputParamData->MutableMetadata();
		if (!MetadataOut)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToGetMetadata", "无法获取输出元数据"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		// Create entry in the AttributeSet
		const PCGMetadataEntryKey EntryKey = MetadataOut->AddEntry();

		// Create the boolean attribute named OutputName
		FPCGMetadataAttribute<bool>* ResultAttr = MetadataOut->FindOrCreateAttribute<bool>(
			FPCGAttributeIdentifier(OutputName, PCGMetadataDomainID::Default),
			/*DefaultValue=*/ false,
			/*bAllowsInterpolation=*/ false,
			/*bOverrideParent=*/ true);

		if (!ResultAttr)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("FailedToCreateAttribute", "无法创建输出属性"));
			Context->OutputData.TaggedData.Add(Input);
			continue;
		}

		// Set the boolean value based on whether the AttributeSet exists
		ResultAttr->SetValue(EntryKey, bAttributeSetExists);

		// 成功路径只写输出日志，不要用 GraphAndLog，否则会出现在节点上并显示为 Warning（与是否真的出错无关）
		if (bAttributeSetExists)
		{
			PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(
				LOCTEXT("AttributeSetFound", "AttributeSet \"{0}\" 存在，已创建 \"{1}\" = true"),
				FText::FromName(CheckName),
				FText::FromName(OutputName)));
		}
		else
		{
			PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(
				LOCTEXT("AttributeSetNotFound", "AttributeSet \"{0}\" 不存在，已创建 \"{1}\" = false"),
				FText::FromName(CheckName),
				FText::FromName(OutputName)));
		}

		// Add output
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = OutputParamData;
		Output.Pin = TEXT("AttributeSet");
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
