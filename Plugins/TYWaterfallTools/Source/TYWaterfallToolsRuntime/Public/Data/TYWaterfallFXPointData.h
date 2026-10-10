// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TYWaterfallFXPointData.generated.h"

/** One world-space point supplied to a waterfall Niagara system. */
USTRUCT(BlueprintType)
struct TYWATERFALLTOOLSRUNTIME_API FTYWaterfallFXPointData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|FX")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|FX")
	FVector ForwardDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|FX")
	FVector UpDirection = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|FX")
	FVector RightDirection = FVector::RightVector;
};
