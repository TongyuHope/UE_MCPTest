// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TYWaterfallSample.generated.h"

/** One distance-resampled record shared by mesh, material and future Niagara generation. */
USTRUCT(BlueprintType)
struct TYWATERFALLTOOLSRUNTIME_API FTYWaterfallSample
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	FVector Tangent = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	FVector Normal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float Distance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float NormalizedDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float Speed = 0.0f;

	/** Accumulated travel time in seconds, used as the material flow coordinate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float Flow = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float Impact = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float Turbulence = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfall|Sample")
	float RandomValue = 0.0f;
};
