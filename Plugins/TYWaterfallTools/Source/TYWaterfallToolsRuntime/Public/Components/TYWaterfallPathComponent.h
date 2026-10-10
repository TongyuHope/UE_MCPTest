// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Data/TYWaterfallSample.h"
#include "TYWaterfallPathComponent.generated.h"

class ATYWaterfallActor;

/** State recorded for each point produced by the editor-time path simulation. */
UENUM()
enum class ETYWaterfallPointState : uint8
{
	Start,
	Airborne,
	Sliding,
	Stopped,
	Terminated
};

/** A debug-friendly snapshot of one simulated fluid point. */
USTRUCT()
struct TYWATERFALLTOOLSRUNTIME_API FTYWaterfallSimPoint
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	FVector HitNormal = FVector::UpVector;

	UPROPERTY()
	ETYWaterfallPointState State = ETYWaterfallPointState::Start;
};

/** Spline that stores one simulated waterfall path. */
UCLASS(ClassGroup = (Waterfall), meta = (DisplayName = "TY Waterfall Path"))
class TYWATERFALLTOOLSRUNTIME_API UTYWaterfallPathComponent : public USplineComponent
{
	GENERATED_BODY()

public:
	UTYWaterfallPathComponent();

#if WITH_EDITOR
	/** Runs one deterministic path simulation and writes the result into this spline. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Simulation")
	bool GeneratePreviewPath();

	/** Prepares this path at a normalized position along the owner's top spline. */
	bool InitializeSimulation(float SplineTime, float DirectionJitterDegrees = 0.0f,
		bool bReverseFlowDirection = false);
	void ConfigureSimulation(float InInitialSpeed, FVector InGravity, float InDrag,
		bool bInEnableWorldCollision, float InFixedDeltaTime, int32 InMaxSteps,
		float InTerminationHeight);

	/** Advances at most StepBudget fixed steps and returns the number consumed. */
	int32 AdvanceSimulation(int32 StepBudget);

	/** Removes all simulated points and spline points created by GeneratePreviewPath. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Simulation")
	void ClearPreviewPath();

	UFUNCTION(BlueprintPure, Category = "Waterfall|Simulation")
	int32 GetNumSimulatedPoints() const { return SimulatedPoints.Num(); }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Simulation")
	bool HasCompletedSimulation() const { return bSimulationComplete; }

	/** Rebuilds the stable distance-based sample cache used by mesh and FX systems. */
	bool BuildResampledSamples(float SampleSpacing);
	const TArray<FTYWaterfallSample>& GetResampledSamples() const { return ResampledSamples; }
	void SetSampleSeed(int32 InSeed) { SampleSeed = InSeed; }
	float GetUVSeed() const { return FRandomStream(SampleSeed).GetFraction(); }
	float GetNormalizedTopSplinePosition() const { return NormalizedTopSplinePosition; }
	float GetTopSplineDistance() const { return TopSplineDistance; }
	void SetPathDebugColor(const FLinearColor& InColor);
#endif

protected:
	/** Initial speed along the top spline's forward direction, in cm/s. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	float InitialSpeed = 800.0f;

	/** World-space acceleration applied every fixed simulation step, in cm/s^2. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	/** Fraction of velocity removed per second. Values are clamped to [0, 1]. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	float Drag = 0.1f;

	/** Whether this path interacts with world geometry; Kill Plane is independent. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	bool bEnableWorldCollision = true;

	/** Fixed time step makes repeated runs with the same inputs deterministic. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	float FixedDeltaTime = 0.016f;

	/** Safety limit that prevents a malformed scene from simulating forever. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	int32 MaxSteps = 600;

	/** Simulation ends when the point reaches this world-Z offset from its owner. */
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	float TerminationHeight = -1000.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	TArray<FTYWaterfallSimPoint> SimulatedPoints;

	/** Derived data rebuilt when the requested sample spacing changes. */
	UPROPERTY(Transient)
	TArray<FTYWaterfallSample> ResampledSamples;

	/** Persisted so regenerating a mesh after reopening the level preserves UV3. */
	UPROPERTY()
	int32 SampleSeed = 0;

	UPROPERTY()
	float NormalizedTopSplinePosition = 0.0f;

	UPROPERTY()
	float TopSplineDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	bool bSimulationComplete = false;

	UPROPERTY(Transient)
	FTYWaterfallSimPoint CurrentPoint;

	UPROPERTY(Transient)
	int32 StepsCompleted = 0;

	UPROPERTY(Transient)
	float WorldTerminationHeight = 0.0f;

	UPROPERTY(Transient)
	bool bSimulationInitialized = false;
#endif

private:
#if WITH_EDITOR
	ATYWaterfallActor* GetWaterfallOwner() const;
	void WriteSimulationToSpline();
#endif
};
