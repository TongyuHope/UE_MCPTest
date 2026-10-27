// Copyright Epic Games, Inc. All Rights Reserved.
// PCGDiffencePoints Node - 基于 PCGDifferenceElement，输出改为 Point 类型

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Data/PCGDifferenceData.h"

#include "PCGDiffencePoints.generated.h"

namespace PCGDiffenceConstants
{
	const FName SourceLabel = TEXT("Source");
	const FName DifferencesLabel = TEXT("Differences");
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class BIOMEPCGRUNTIME_API UPCGDiffenceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DiffencePoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDiffenceSettings", "NodeTitle", "DiffencePoints"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual void ApplyStructuralDeprecation(UPCGNode* InOutNode) override;
#endif
	virtual FString GetAdditionalTitleInformation() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The density function to use when recalculating the density after the operation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGDifferenceDensityFunction DensityFunction = EPCGDifferenceDensityFunction::Minimum;

	/** Describes how the difference operation will treat the output data:
	 * Continuous - Non-destructive data output will be maintained.
	 * Discrete - Output data will be discrete points, or explicitly converted to points.
	 * Inferred - Output data will choose from Continuous or Discrete, based on the source and operation.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGDifferenceMode Mode = EPCGDifferenceMode::Inferred;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDiffMetadata = true;

	/** If enabled, the output will not automatically filter out points with zero density. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bKeepZeroDensityPoints = false;
};

class FPCGDiffenceElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};

