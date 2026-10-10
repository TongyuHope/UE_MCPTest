// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/TYWaterfallPathComponent.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
	float StableSampleRandom(int32 Seed, float Distance)
	{
		const int32 DistanceKey = FMath::RoundToInt(Distance * 10.0f);
		FRandomStream RandomStream(HashCombineFast(GetTypeHash(Seed), GetTypeHash(DistanceKey)));
		return RandomStream.GetFraction();
	}
}

UTYWaterfallPathComponent::UTYWaterfallPathComponent()
{
	// The actor or a generation builder owns the simulation cadence. The component
	// itself must not tick, otherwise every path would create its own editor tick.
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
}

#if WITH_EDITOR
ATYWaterfallActor* UTYWaterfallPathComponent::GetWaterfallOwner() const
{
	return GetOwner<ATYWaterfallActor>();
}

bool UTYWaterfallPathComponent::GeneratePreviewPath()
{
	if (!InitializeSimulation(0.5f))
	{
		return false;
	}

	// This compatibility entry point intentionally runs synchronously. The
	// multi-path builder uses AdvanceSimulation directly with a frame budget.
	while (!bSimulationComplete)
	{
		AdvanceSimulation(MaxSteps);
	}
	return bSimulationComplete;
}

bool UTYWaterfallPathComponent::InitializeSimulation(
	float SplineTime,
	float DirectionJitterDegrees,
	bool bReverseFlowDirection)
{
	ATYWaterfallActor* Waterfall = GetWaterfallOwner();
	if (!IsValid(Waterfall) || !IsValid(Waterfall->GetTopSpline()))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	Modify();
	ClearPreviewPath();
	NormalizedTopSplinePosition = FMath::Clamp(SplineTime, 0.0f, 1.0f);

	USplineComponent* TopSpline = Waterfall->GetTopSpline();
	TopSplineDistance = TopSpline->GetSplineLength() * NormalizedTopSplinePosition;
	const FVector StartPosition = TopSpline->GetLocationAtTime(
		SplineTime, ESplineCoordinateSpace::World, true);
	const FVector UpDirection = TopSpline->GetUpVectorAtTime(
		SplineTime, ESplineCoordinateSpace::World, true).GetSafeNormal();
	// The top spline describes the waterfall's width. Water must leave the
	// cliff perpendicular to that width, producing a T-shaped top/path layout.
	// The right vector supplies that perpendicular direction while preserving
	// the spline's authored up vector and rotation.
	FVector StartDirection = TopSpline->GetRightVectorAtTime(
		SplineTime, ESplineCoordinateSpace::World, true)
		.GetSafeNormal()
		.RotateAngleAxis(DirectionJitterDegrees, UpDirection);
	if (bReverseFlowDirection)
	{
		StartDirection *= -1.0f;
	}

	if (StartDirection.IsNearlyZero())
	{
		return false;
	}

	CurrentPoint.Position = StartPosition;
	CurrentPoint.Velocity = StartDirection * FMath::Max(InitialSpeed, 0.0f);
	CurrentPoint.State = ETYWaterfallPointState::Start;
	SimulatedPoints.Add(CurrentPoint);
	StepsCompleted = 0;
	WorldTerminationHeight = Waterfall->GetActorLocation().Z + TerminationHeight;
	bSimulationInitialized = true;
	bSimulationComplete = false;
	WriteSimulationToSpline();
	return true;
}

void UTYWaterfallPathComponent::ConfigureSimulation(
	float InInitialSpeed,
	FVector InGravity,
	float InDrag,
	bool bInEnableWorldCollision,
	float InFixedDeltaTime,
	int32 InMaxSteps,
	float InTerminationHeight)
{
	InitialSpeed = FMath::Max(InInitialSpeed, 0.0f);
	Gravity = InGravity;
	Drag = FMath::Clamp(InDrag, 0.0f, 1.0f);
	bEnableWorldCollision = bInEnableWorldCollision;
	FixedDeltaTime = FMath::Max(InFixedDeltaTime, 0.001f);
	MaxSteps = FMath::Max(InMaxSteps, 1);
	TerminationHeight = InTerminationHeight;
}

int32 UTYWaterfallPathComponent::AdvanceSimulation(int32 StepBudget)
{
	if (!bSimulationInitialized || bSimulationComplete || StepBudget <= 0)
	{
		return 0;
	}

	ATYWaterfallActor* Waterfall = GetWaterfallOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Waterfall) || !IsValid(World))
	{
		bSimulationComplete = true;
		return 0;
	}

	const float Step = FMath::Max(FixedDeltaTime, KINDA_SMALL_NUMBER);
	const int32 StepLimit = FMath::Max(MaxSteps, 1);
	const float DragFactor = 1.0f - FMath::Clamp(Drag * Step, 0.0f, 1.0f);
	const bool bHasKillPlane = IsValid(Waterfall->GetKillPlaneComponent());
	int32 StepsUsed = 0;

	// Each iteration advances exactly one fixed step. A line trace covers the
	// whole segment, so fast-moving points cannot skip a thin collision surface.
	while (StepsUsed < StepBudget && StepsCompleted < StepLimit)
	{
		++StepsUsed;
		++StepsCompleted;
		const FVector PreviousPosition = CurrentPoint.Position;
		CurrentPoint.Velocity += Gravity * Step;
		CurrentPoint.Velocity *= DragFactor;
		CurrentPoint.State = ETYWaterfallPointState::Airborne;

		const FVector CandidatePosition = PreviousPosition + CurrentPoint.Velocity * Step;

		// Match the reference tool: Kill Plane is an infinite mathematical plane,
		// not a mesh collision surface. Test the complete movement segment so a
		// fast point cannot tunnel through it between simulation steps.
		if (const UStaticMeshComponent* KillPlane = Waterfall->GetKillPlaneComponent())
		{
			FVector PlaneNormal = KillPlane->GetUpVector().GetSafeNormal();
			if (PlaneNormal.IsNearlyZero())
			{
				PlaneNormal = FVector::UpVector;
			}
			const FVector PlaneOrigin = KillPlane->GetComponentLocation();
			const float PreviousSide = FVector::DotProduct(
				PreviousPosition - PlaneOrigin, PlaneNormal);
			const float CandidateSide = FVector::DotProduct(
				CandidatePosition - PlaneOrigin, PlaneNormal);
			const float Denominator = PreviousSide - CandidateSide;
			if (PreviousSide * CandidateSide <= 0.0f
				&& !FMath::IsNearlyZero(Denominator))
			{
				const float HitAlpha = FMath::Clamp(PreviousSide / Denominator, 0.0f, 1.0f);
				CurrentPoint.Position = FMath::Lerp(PreviousPosition, CandidatePosition, HitAlpha);
				CurrentPoint.HitNormal = PlaneNormal;
				CurrentPoint.Velocity = FVector::ZeroVector;
				CurrentPoint.State = ETYWaterfallPointState::Terminated;
				SimulatedPoints.Add(CurrentPoint);
				bSimulationComplete = true;
				break;
			}
		}

		FHitResult Hit;
		bool bHit = false;
		if (bEnableWorldCollision)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TYWaterfallPath), true, Waterfall);
			bHit = World->LineTraceSingleByChannel(
				Hit, PreviousPosition, CandidatePosition, ECC_Visibility, QueryParams);
		}

		if (bHit)
		{
			CurrentPoint.Position = Hit.ImpactPoint + Hit.ImpactNormal * 2.0f;
			CurrentPoint.HitNormal = Hit.ImpactNormal;

			// Remove the velocity component pointing into the surface. The
			// remaining component becomes the sliding direction.
			CurrentPoint.Velocity = FVector::VectorPlaneProject(
				CurrentPoint.Velocity, Hit.ImpactNormal);
			CurrentPoint.State = ETYWaterfallPointState::Sliding;

			if (CurrentPoint.Velocity.SizeSquared() < FMath::Square(1.0f))
			{
				CurrentPoint.Velocity = FVector::ZeroVector;
				CurrentPoint.State = ETYWaterfallPointState::Stopped;
			}
		}
		else
		{
			CurrentPoint.Position = CandidatePosition;
			CurrentPoint.HitNormal = FVector::UpVector;
		}

		SimulatedPoints.Add(CurrentPoint);

		// Without world collision, only the Kill Plane should stop normal motion.
		// MaxSteps remains a safety cap for paths that never reach the plane.
		const bool bStoppedByWorldCollision = bEnableWorldCollision
			&& (CurrentPoint.State == ETYWaterfallPointState::Stopped
				|| CurrentPoint.Velocity.IsNearlyZero());
		if (bStoppedByWorldCollision
			|| (!bHasKillPlane && CurrentPoint.Position.Z <= WorldTerminationHeight)
			|| StepsCompleted >= StepLimit)
		{
			SimulatedPoints.Last().State = ETYWaterfallPointState::Terminated;
			bSimulationComplete = true;
			break;
		}
	}

	WriteSimulationToSpline();
	MarkRenderStateDirty();
	if (bSimulationComplete)
	{
		MarkPackageDirty();
	}
	return StepsUsed;
}

void UTYWaterfallPathComponent::SetPathDebugColor(const FLinearColor& InColor)
{
	SetUnselectedSplineSegmentColor(InColor);
	SetSelectedSplineSegmentColor(InColor.Desaturate(0.2f));
}

bool UTYWaterfallPathComponent::BuildResampledSamples(float SampleSpacing)
{
	ResampledSamples.Reset();
	if (SimulatedPoints.Num() < 2)
	{
		return false;
	}

	const float SafeSpacing = FMath::Max(SampleSpacing, 1.0f);
	TArray<float> SourceDistances;
	SourceDistances.SetNumZeroed(SimulatedPoints.Num());
	for (int32 PointIndex = 1; PointIndex < SimulatedPoints.Num(); ++PointIndex)
	{
		SourceDistances[PointIndex] = SourceDistances[PointIndex - 1]
			+ FVector::Distance(SimulatedPoints[PointIndex - 1].Position,
				SimulatedPoints[PointIndex].Position);
	}

	const float TotalDistance = SourceDistances.Last();
	if (TotalDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const int32 SampleCount = FMath::Max(FMath::CeilToInt(TotalDistance / SafeSpacing) + 1, 2);
	FVector WidthDirection = FVector::RightVector;
	if (const ATYWaterfallActor* Waterfall = GetWaterfallOwner())
	{
		if (const USplineComponent* TopSpline = Waterfall->GetTopSpline())
		{
			WidthDirection = TopSpline->GetDirectionAtTime(
				NormalizedTopSplinePosition, ESplineCoordinateSpace::World, true).GetSafeNormal();
		}
	}
	ResampledSamples.Reserve(SampleCount);
	int32 SourceIndex = 0;
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Distance = SampleIndex == SampleCount - 1
			? TotalDistance
			: FMath::Min(SampleIndex * SafeSpacing, TotalDistance);
		while (SourceIndex + 1 < SourceDistances.Num()
			&& SourceDistances[SourceIndex + 1] < Distance)
		{
			++SourceIndex;
		}

		const int32 NextIndex = FMath::Min(SourceIndex + 1, SimulatedPoints.Num() - 1);
		const float Span = SourceDistances[NextIndex] - SourceDistances[SourceIndex];
		const float Alpha = Span > KINDA_SMALL_NUMBER
			? (Distance - SourceDistances[SourceIndex]) / Span
			: 0.0f;
		const FTYWaterfallSimPoint& A = SimulatedPoints[SourceIndex];
		const FTYWaterfallSimPoint& B = SimulatedPoints[NextIndex];
		const FVector Position = FMath::Lerp(A.Position, B.Position, Alpha);
		const FVector Velocity = FMath::Lerp(A.Velocity, B.Velocity, Alpha);
		const FVector Tangent = (NextIndex != SourceIndex
			? (B.Position - A.Position)
			: A.Velocity).GetSafeNormal();
		FVector SideTangent = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
		if (SideTangent.IsNearlyZero())
		{
			SideTangent = FVector::VectorPlaneProject(WidthDirection, Tangent).GetSafeNormal();
		}
		const FVector Normal = FVector::CrossProduct(Tangent, SideTangent).GetSafeNormal();
		const float Speed = Velocity.Size();
		const float Impact = (A.State == ETYWaterfallPointState::Sliding
			|| B.State == ETYWaterfallPointState::Sliding) ? 1.0f : 0.0f;
		const float DirectionChange = SourceIndex > 0
			? FVector::CrossProduct(
				(SimulatedPoints[SourceIndex].Position - SimulatedPoints[SourceIndex - 1].Position).GetSafeNormal(),
				Tangent).Size()
			: 0.0f;

		// Match WaterfallTools: each cached point contributes one sampled path
		// length divided by its speed, including the first point.
		float Flow = SafeSpacing / FMath::Max(Speed, 1.0f);
		if (!ResampledSamples.IsEmpty())
		{
			const FTYWaterfallSample& Previous = ResampledSamples.Last();
			Flow += Previous.Flow;
		}
		FTYWaterfallSample& Sample = ResampledSamples.AddDefaulted_GetRef();
		Sample.Position = Position;
		Sample.Tangent = Tangent.IsNearlyZero() ? FVector::ForwardVector : Tangent;
		Sample.Normal = Normal.IsNearlyZero()
			? FMath::Lerp(A.HitNormal, B.HitNormal, Alpha).GetSafeNormal()
			: Normal;
		Sample.Velocity = Velocity;
		Sample.Distance = Distance;
		Sample.NormalizedDistance = Distance / TotalDistance;
		Sample.Speed = Speed;
		Sample.Flow = Flow;
		Sample.Impact = Impact;
		Sample.Turbulence = FMath::Clamp(FMath::Max(DirectionChange, Impact), 0.0f, 1.0f);
		Sample.RandomValue = StableSampleRandom(SampleSeed, Distance);
	}

	return ResampledSamples.Num() >= 2;
}

void UTYWaterfallPathComponent::ClearPreviewPath()
{
	Modify();
	SimulatedPoints.Reset();
	bSimulationComplete = false;
	bSimulationInitialized = false;
	StepsCompleted = 0;
	CurrentPoint = FTYWaterfallSimPoint();
	ResampledSamples.Reset();
	NormalizedTopSplinePosition = 0.0f;
	TopSplineDistance = 0.0f;
	ClearSplinePoints(false);
	UpdateSpline();
}

void UTYWaterfallPathComponent::WriteSimulationToSpline()
{
	ClearSplinePoints(false);
	for (const FTYWaterfallSimPoint& Point : SimulatedPoints)
	{
		AddSplinePoint(Point.Position, ESplineCoordinateSpace::World, false);
	}
	if (GetNumberOfSplinePoints() > 0)
	{
		SetSplinePointType(0, ESplinePointType::Linear, false);
	}
	for (int32 PointIndex = 1; PointIndex < GetNumberOfSplinePoints(); ++PointIndex)
	{
		SetSplinePointType(PointIndex, ESplinePointType::CurveClamped, false);
	}
	UpdateSpline();
}
#endif
