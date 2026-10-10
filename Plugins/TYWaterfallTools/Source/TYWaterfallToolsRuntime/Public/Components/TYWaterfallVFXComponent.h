// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TYWaterfallFXPointData.h"
#include "NiagaraComponent.h"
#include "TYWaterfallVFXComponent.generated.h"

/** Niagara component that owns one waterfall FX point set and its user parameters. */
UCLASS(ClassGroup = (Waterfall), meta = (DisplayName = "TY Waterfall VFX"))
class TYWATERFALLTOOLSRUNTIME_API UTYWaterfallVFXComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:
	UTYWaterfallVFXComponent();

	virtual void BeginPlay() override;

	/** Replaces the serialized point set and uploads it to Niagara once. */
	void SetPointData(const TArray<FTYWaterfallFXPointData>& InPointData, float BoundsPadding);
	void ClearPointData();

	UFUNCTION(BlueprintCallable, Category = "Waterfall|FX")
	void RefreshNiagaraParameters();

	UFUNCTION(BlueprintPure, Category = "Waterfall|FX")
	TArray<FTYWaterfallFXPointData> GetPointData() const { return PointData; }

protected:
	/** Local-space copy stored so moving the actor never invalidates generated FX data. */
	UPROPERTY(VisibleAnywhere, Category = "Waterfall|FX")
	TArray<FTYWaterfallFXPointData> PointData;

	UPROPERTY(VisibleAnywhere, Category = "Waterfall|FX")
	float StoredBoundsPadding = 200.0f;
};
