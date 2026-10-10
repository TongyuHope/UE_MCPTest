// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ATYWaterfallActor;
class UTYWaterfallPathComponent;

enum class ETYWaterfallGenerationState : uint8
{
	Idle,
	Generating,
	Cancelling,
	Completed,
	Failed
};

/** Coordinates creation and frame-budgeted simulation of waterfall paths. */
struct TYWATERFALLTOOLSRUNTIME_API FTYWaterfallPathBuilder
{
#if WITH_EDITOR
	void Initialize(ATYWaterfallActor* InOwner);
	bool StartGeneration();
	void TickGeneration();
	void CancelGeneration();
	void ClearGeneratedPaths();
	bool IsGenerating() const { return State == ETYWaterfallGenerationState::Generating; }
	float GetProgress() const;

private:
	bool CreatePaths();
	void FinishGeneration(ETYWaterfallGenerationState ResultState);

	TWeakObjectPtr<ATYWaterfallActor> Owner;
	TArray<TWeakObjectPtr<UTYWaterfallPathComponent>> PendingPaths;
	int32 CurrentPathIndex = 0;
	int32 TotalPathCount = 0;
	ETYWaterfallGenerationState State = ETYWaterfallGenerationState::Idle;
#endif
};
