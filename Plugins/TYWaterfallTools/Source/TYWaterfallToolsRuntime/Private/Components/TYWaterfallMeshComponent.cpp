// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/TYWaterfallMeshComponent.h"

#include "Components/TYWaterfallPathComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

using namespace UE::Geometry;

namespace
{
enum class EWaterfallMaterialSlot : int32
{
	Singular = 0,
	PerPath = 1,
	Cross = 2,
	Splash = 3
};

struct FWaterfallMeshAttributes
{
	// The four UV overlays and primary color overlay are copied into the baked
	// Static Mesh by Geometry Script. Keep their meanings stable for materials.
	FDynamicMeshNormalOverlay* Normals = nullptr;
	FDynamicMeshUVOverlay* UV0 = nullptr;
	FDynamicMeshUVOverlay* UV1 = nullptr;
	FDynamicMeshUVOverlay* UV2 = nullptr;
	FDynamicMeshUVOverlay* UV3 = nullptr;
	FDynamicMeshColorOverlay* Colors = nullptr;
	FDynamicMeshMaterialAttribute* MaterialIDs = nullptr;
};

FWaterfallMeshAttributes InitializeAttributes(FDynamicMesh3& Mesh)
{
	// Every surface must populate all four UV overlays, including triangle IDs.
	Mesh.EnableAttributes();
	Mesh.Attributes()->SetNumUVLayers(4);
	Mesh.Attributes()->EnablePrimaryColors();
	Mesh.Attributes()->EnableMaterialID();
	return {
		Mesh.Attributes()->PrimaryNormals(),
		Mesh.Attributes()->GetUVLayer(0),
		Mesh.Attributes()->GetUVLayer(1),
		Mesh.Attributes()->GetUVLayer(2),
		Mesh.Attributes()->GetUVLayer(3),
		Mesh.Attributes()->PrimaryColors(),
		Mesh.Attributes()->GetMaterialID()
	};
}

void SetTriangleAttributes(
	const FWaterfallMeshAttributes& Attributes,
	int32 TriangleID,
	const FIndex3i& Corners,
	const TArray<int32>& NormalIDs,
	const TArray<int32>& UV0IDs,
	const TArray<int32>& UV1IDs,
	const TArray<int32>& UV2IDs,
	const TArray<int32>& UV3IDs,
	const TArray<int32>& ColorIDs,
	EWaterfallMaterialSlot MaterialSlot)
{
	if (TriangleID < 0)
	{
		return;
	}

	auto Remap = [&Corners](const TArray<int32>& IDs)
	{
		return FIndex3i(IDs[Corners.A], IDs[Corners.B], IDs[Corners.C]);
	};

	Attributes.Normals->SetTriangle(TriangleID, Remap(NormalIDs));
	Attributes.UV0->SetTriangle(TriangleID, Remap(UV0IDs));
	Attributes.UV1->SetTriangle(TriangleID, Remap(UV1IDs));
	Attributes.UV2->SetTriangle(TriangleID, Remap(UV2IDs));
	Attributes.UV3->SetTriangle(TriangleID, Remap(UV3IDs));
	Attributes.Colors->SetTriangle(TriangleID, Remap(ColorIDs));
	Attributes.MaterialIDs->SetValue(TriangleID, static_cast<int32>(MaterialSlot));
}

FVector4f MakeWaterfallVertexColor(FVector Direction, float Turbulence)
{
	// RGB stores a signed direction remapped from [-1, 1] to [0, 1]. Materials
	// decode it with Direction = VertexColor.rgb * 2 - 1. Alpha stores turbulence.
	Direction = Direction.GetSafeNormal();
	const FVector EncodedDirection = (Direction + FVector::OneVector) * 0.5;
	return FVector4f(
		static_cast<float>(EncodedDirection.X),
		static_cast<float>(EncodedDirection.Y),
		static_cast<float>(EncodedDirection.Z),
		Turbulence);
}

FTYWaterfallSample InterpolateSampleAtNormalizedDistance(
	const TArray<FTYWaterfallSample>& Samples,
	float NormalizedDistance)
{
	const float TargetDistance = FMath::Clamp(NormalizedDistance, 0.0f, 1.0f);
	if (Samples.Num() == 1 || TargetDistance <= Samples[0].NormalizedDistance)
	{
		return Samples[0];
	}
	if (TargetDistance >= Samples.Last().NormalizedDistance)
	{
		return Samples.Last();
	}

	int32 UpperIndex = 1;
	while (UpperIndex < Samples.Num()
		&& Samples[UpperIndex].NormalizedDistance < TargetDistance)
	{
		++UpperIndex;
	}

	const int32 LowerIndex = FMath::Max(UpperIndex - 1, 0);
	const FTYWaterfallSample& Lower = Samples[LowerIndex];
	const FTYWaterfallSample& Upper = Samples[FMath::Min(UpperIndex, Samples.Num() - 1)];
	const float DistanceRange = Upper.NormalizedDistance - Lower.NormalizedDistance;
	const float Alpha = DistanceRange > KINDA_SMALL_NUMBER
		? (TargetDistance - Lower.NormalizedDistance) / DistanceRange
		: 0.0f;

	FTYWaterfallSample Result;
	Result.Position = FMath::Lerp(Lower.Position, Upper.Position, Alpha);
	Result.Tangent = FMath::Lerp(Lower.Tangent, Upper.Tangent, Alpha).GetSafeNormal();
	Result.Normal = FMath::Lerp(Lower.Normal, Upper.Normal, Alpha).GetSafeNormal();
	Result.Velocity = FMath::Lerp(Lower.Velocity, Upper.Velocity, Alpha);
	Result.Distance = FMath::Lerp(Lower.Distance, Upper.Distance, Alpha);
	Result.NormalizedDistance = TargetDistance;
	Result.Speed = FMath::Lerp(Lower.Speed, Upper.Speed, Alpha);
	Result.Flow = FMath::Lerp(Lower.Flow, Upper.Flow, Alpha);
	Result.Impact = FMath::Lerp(Lower.Impact, Upper.Impact, Alpha);
	Result.Turbulence = FMath::Lerp(Lower.Turbulence, Upper.Turbulence, Alpha);
	Result.RandomValue = FMath::Lerp(Lower.RandomValue, Upper.RandomValue, Alpha);
	return Result;
}

bool AppendSingular(
	FDynamicMesh3& Mesh,
	const FWaterfallMeshAttributes& Attributes,
	const TArray<TObjectPtr<UTYWaterfallPathComponent>>& Paths,
	const FTransform& WorldToMesh,
	FVector WorldWidthAxis,
	FVector2D BaseUVScale)
{
	struct FSortedPath
	{
		const TArray<FTYWaterfallSample>* Samples = nullptr;
		float UVSeed = 0.0f;
		float TopSplinePosition = 0.0f;
		float TopSplineDistance = 0.0f;
		double WidthPosition = 0.0;
	};

	TArray<FSortedPath> SortedPaths;
	int32 RowCount = 0;
	for (const UTYWaterfallPathComponent* Path : Paths)
	{
		if (!IsValid(Path))
		{
			continue;
		}

		const TArray<FTYWaterfallSample>& Samples = Path->GetResampledSamples();
		if (Samples.Num() < 2 || Samples.Last().Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		SortedPaths.Add({ &Samples, Path->GetUVSeed(),
			Path->GetNormalizedTopSplinePosition(), Path->GetTopSplineDistance(),
			FVector::DotProduct(Samples[0].Position, WorldWidthAxis) });
		RowCount = FMath::Max(RowCount, Samples.Num());
	}

	if (SortedPaths.Num() < 2 || RowCount < 2)
	{
		return false;
	}

	// The reference plugin remaps paths to a shared longitudinal resolution before
	// joining them. Sorting once at the lip preserves path identity on every row,
	// which makes the topology deterministic and avoids row-to-row index swaps.
	SortedPaths.StableSort([](const FSortedPath& A, const FSortedPath& B)
	{
		return A.WidthPosition < B.WidthPosition;
	});

	const int32 PathCount = SortedPaths.Num();
	TArray<FTYWaterfallSample> GridSamples;
	GridSamples.SetNum(PathCount * RowCount);
	for (int32 PathIndex = 0; PathIndex < PathCount; ++PathIndex)
	{
		for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
		{
			const float RowAlpha = static_cast<float>(RowIndex) / (RowCount - 1);
			GridSamples[PathIndex * RowCount + RowIndex] =
				InterpolateSampleAtNormalizedDistance(*SortedPaths[PathIndex].Samples, RowAlpha);
		}
	}

	const int32 VertexCount = PathCount * RowCount;
	TArray<int32> VertexIDs;
	TArray<int32> NormalIDs;
	TArray<int32> UV0IDs;
	TArray<int32> UV1IDs;
	TArray<int32> UV2IDs;
	TArray<int32> UV3IDs;
	TArray<int32> ColorIDs;
	VertexIDs.Reserve(VertexCount);
	NormalIDs.Reserve(VertexCount);
	UV0IDs.Reserve(VertexCount);
	UV1IDs.Reserve(VertexCount);
	UV2IDs.Reserve(VertexCount);
	UV3IDs.Reserve(VertexCount);
	ColorIDs.Reserve(VertexCount);

	for (int32 PathIndex = 0; PathIndex < PathCount; ++PathIndex)
	{
		const float PathAlpha = static_cast<float>(PathIndex) / (PathCount - 1);
		for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
		{
			const int32 GridIndex = PathIndex * RowCount + RowIndex;
			const FTYWaterfallSample& Sample = GridSamples[GridIndex];
			const int32 PreviousPath = FMath::Max(PathIndex - 1, 0);
			const int32 NextPath = FMath::Min(PathIndex + 1, PathCount - 1);
			FVector WorldAcross = (
				GridSamples[NextPath * RowCount + RowIndex].Position
				- GridSamples[PreviousPath * RowCount + RowIndex].Position).GetSafeNormal();
			if (WorldAcross.IsNearlyZero())
			{
				WorldAcross = WorldWidthAxis;
			}

			FVector WorldTangent = Sample.Tangent.GetSafeNormal();
			if (WorldTangent.IsNearlyZero())
			{
				WorldTangent = FVector::DownVector;
			}
			FVector WorldNormal = FVector::CrossProduct(WorldAcross, WorldTangent).GetSafeNormal();
			if (WorldNormal.IsNearlyZero())
			{
				WorldNormal = Sample.Normal.GetSafeNormal();
			}
			if (WorldNormal.IsNearlyZero())
			{
				WorldNormal = FVector::UpVector;
			}

			VertexIDs.Add(Mesh.AppendVertex(FVector3d(
				WorldToMesh.TransformPosition(Sample.Position))));
			NormalIDs.Add(Attributes.Normals->AppendElement(FVector3f(
				WorldToMesh.TransformVectorNoScale(WorldNormal).GetSafeNormal())));
			UV0IDs.Add(Attributes.UV0->AppendElement(FVector2f(
				// Singular: X is the normalized width/path coordinate; Y is distance
				// along the shared waterfall sheet.
				PathAlpha * BaseUVScale.X,
				Sample.NormalizedDistance * BaseUVScale.Y)));
			UV1IDs.Add(Attributes.UV1->AppendElement(FVector2f(
				// Singular: X identifies the source top-spline path; Y is world-unit
				// travel distance, scaled for material tiling.
				SortedPaths[PathIndex].TopSplineDistance * BaseUVScale.X,
				Sample.Distance * BaseUVScale.Y)));
			UV2IDs.Add(Attributes.UV2->AppendElement(FVector2f(
				// Shared protocol: UV2 = (speed in cm/s, accumulated flow time in s).
				Sample.Speed, Sample.Flow * BaseUVScale.Y)));
			UV3IDs.Add(Attributes.UV3->AppendElement(FVector2f(
				// Shared protocol: UV3 = (stable per-path random seed, top-spline t).
				SortedPaths[PathIndex].UVSeed, SortedPaths[PathIndex].TopSplinePosition)));
			ColorIDs.Add(Attributes.Colors->AppendElement(
				MakeWaterfallVertexColor(Sample.Velocity, Sample.Turbulence)));
		}
	}

	// Use the same winding as the validated Per-Path ribbon: increasing path
	// index is left-to-right, while increasing row index follows the water flow.
	for (int32 PathIndex = 0; PathIndex < PathCount - 1; ++PathIndex)
	{
		for (int32 RowIndex = 0; RowIndex < RowCount - 1; ++RowIndex)
		{
			const int32 Left0 = PathIndex * RowCount + RowIndex;
			const int32 Left1 = Left0 + 1;
			const int32 Right0 = (PathIndex + 1) * RowCount + RowIndex;
			const int32 Right1 = Right0 + 1;
			const FIndex3i CornersA(Left0, Left1, Right0);
			const FIndex3i CornersB(Right0, Left1, Right1);
			const int32 TriangleA = Mesh.AppendTriangle(
				VertexIDs[CornersA.A], VertexIDs[CornersA.B], VertexIDs[CornersA.C]);
			const int32 TriangleB = Mesh.AppendTriangle(
				VertexIDs[CornersB.A], VertexIDs[CornersB.B], VertexIDs[CornersB.C]);
			SetTriangleAttributes(Attributes, TriangleA, CornersA,
				NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs,
				EWaterfallMaterialSlot::Singular);
			SetTriangleAttributes(Attributes, TriangleB, CornersB,
				NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs,
				EWaterfallMaterialSlot::Singular);
		}
	}

	return true;
}

bool AppendRibbon(
	FDynamicMesh3& Mesh,
	const FWaterfallMeshAttributes& Attributes,
	const TArray<FTYWaterfallSample>& Samples,
	const FTransform& WorldToMesh,
	FVector WorldWidthAxis,
	float Width,
	float BottomWidthScale,
	int32 Subdivisions,
	FVector2D BaseUVScale,
	float RotationDegrees,
	float UVSeed,
	float TopSplinePosition,
	EWaterfallMaterialSlot MaterialSlot)
{
	if (Samples.Num() < 2 || Samples.Last().Distance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const int32 SegmentCount = Samples.Num() - 1;
	const bool bCrossPlane = MaterialSlot == EWaterfallMaterialSlot::Cross;
	const int32 SafeSubdivisions = FMath::Clamp(Subdivisions, 0, 32);
	const int32 RowVertexCount = SafeSubdivisions + 2;
	TArray<int32> VertexIDs;
	TArray<int32> NormalIDs;
	TArray<int32> UV0IDs;
	TArray<int32> UV1IDs;
	TArray<int32> UV2IDs;
	TArray<int32> UV3IDs;
	TArray<int32> ColorIDs;
	const int32 VertexCount = Samples.Num() * RowVertexCount;
	VertexIDs.Reserve(VertexCount);
	NormalIDs.Reserve(VertexCount);
	UV0IDs.Reserve(VertexCount);
	UV1IDs.Reserve(VertexCount);
	UV2IDs.Reserve(VertexCount);
	UV3IDs.Reserve(VertexCount);
	ColorIDs.Reserve(VertexCount);

	for (const FTYWaterfallSample& Sample : Samples)
	{
		const FVector WorldTangent = Sample.Tangent.GetSafeNormal();
		FVector WorldAcross = FVector::VectorPlaneProject(
			WorldWidthAxis, WorldTangent).GetSafeNormal();
		if (WorldAcross.IsNearlyZero())
		{
			WorldAcross = FVector::CrossProduct(
				WorldTangent, FVector::UpVector).GetSafeNormal();
		}
		if (WorldAcross.IsNearlyZero())
		{
			WorldAcross = FVector::RightVector;
		}

		if (bCrossPlane)
		{
			// WaterfallTools builds Cross from the cached surface normal.  Rotating
			// the width axis by 90 degrees is only an approximation and can make the
			// plane nearly edge-on to its own normal at curved path samples.
			const FVector FallbackAcross = WorldAcross;
			// Reference Cross expands directly along the cached surface normal.
			// Projecting it onto the tangent-orthogonal plane changes the plane's
			// facing and makes the material appear on the wrong viewing side.
			WorldAcross = Sample.Normal.GetSafeNormal();
			if (WorldAcross.IsNearlyZero())
			{
				WorldAcross = FallbackAcross.RotateAngleAxis(RotationDegrees, WorldTangent);
			}
		}
		else
		{
			WorldAcross = WorldAcross.RotateAngleAxis(RotationDegrees, WorldTangent);
		}
		const FVector WorldNormal = bCrossPlane
			? FVector::CrossProduct(WorldTangent, WorldAcross).GetSafeNormal()
			: FVector::CrossProduct(WorldAcross, WorldTangent).GetSafeNormal();
		// Width is the top width. Interpolate its scale along normalized path
		// distance so one setting can produce the common narrow-top/wide-bottom
		// waterfall silhouette without changing path simulation.
		const float WidthScale = FMath::Lerp(
			1.0f, BottomWidthScale, FMath::Clamp(Sample.NormalizedDistance, 0.0f, 1.0f));
		// WaterfallTools treats Per-Path width as the total width, while Cross
		// width is the distance from its centre to either side.
		const float HalfWidth = MaterialSlot == EWaterfallMaterialSlot::Cross
			? Width * WidthScale : Width * WidthScale * 0.5f;
		const FVector HalfWidthOffset = WorldAcross * HalfWidth;
		// Cross uses the same left/right ordering as the reference builder:
		// left is +Normal and right is -Normal. This keeps the ribbon winding
		// consistent with the normal written to the dynamic mesh.
		const FVector WorldLeft = bCrossPlane
			? Sample.Position + HalfWidthOffset
			: Sample.Position - HalfWidthOffset;
		const FVector WorldRight = bCrossPlane
			? Sample.Position - HalfWidthOffset
			: Sample.Position + HalfWidthOffset;
		const FVector3f LocalNormal = FVector3f(
			WorldToMesh.TransformVectorNoScale(WorldNormal).GetSafeNormal());

		const FVector2f UV2(Sample.Speed, Sample.Flow * BaseUVScale.Y);
		const FVector2f UV3(UVSeed, TopSplinePosition);
		const FVector4f Color = MakeWaterfallVertexColor(
			Sample.Velocity, Sample.Turbulence);
		for (int32 ColumnIndex = 0; ColumnIndex < RowVertexCount; ++ColumnIndex)
		{
			const float ColumnAlpha = static_cast<float>(ColumnIndex)
				/ static_cast<float>(RowVertexCount - 1);
			const FVector WorldPosition = FMath::Lerp(WorldLeft, WorldRight, ColumnAlpha);
			const float Across = bCrossPlane
				? FMath::Lerp(HalfWidth, -HalfWidth, ColumnAlpha)
				: FMath::Lerp(-HalfWidth, HalfWidth, ColumnAlpha);
			VertexIDs.Add(Mesh.AppendVertex(FVector3d(
				WorldToMesh.TransformPosition(WorldPosition))));
			NormalIDs.Add(Attributes.Normals->AppendElement(LocalNormal));
			UV0IDs.Add(Attributes.UV0->AppendElement(FVector2f(
				// Ribbon surfaces: X is across the ribbon (0..1); Y is normalized
				// distance from the top to the endpoint.
				ColumnAlpha * BaseUVScale.X,
				Sample.NormalizedDistance * BaseUVScale.Y)));
			UV1IDs.Add(Attributes.UV1->AppendElement(FVector2f(
				// Ribbon surfaces: X is signed local width in Unreal units; Y is
				// accumulated path distance in Unreal units.
				Across * BaseUVScale.X, Sample.Distance * BaseUVScale.Y)));
			// UV2, UV3 and vertex color use the shared material protocol above.
			UV2IDs.Add(Attributes.UV2->AppendElement(UV2));
			UV3IDs.Add(Attributes.UV3->AppendElement(UV3));
			ColorIDs.Add(Attributes.Colors->AppendElement(Color));
		}
	}

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < RowVertexCount - 1; ++ColumnIndex)
		{
			const int32 Left0 = SegmentIndex * RowVertexCount + ColumnIndex;
			const int32 Right0 = Left0 + 1;
			const int32 Left1 = Left0 + RowVertexCount;
			const int32 Right1 = Left1 + 1;
			// WaterfallTools reverses the Cross plane winding relative to the
			// Per-Path ribbon. Keep both triangles in the same facing direction.
			const FIndex3i CornersA = bCrossPlane
				? FIndex3i(Left0, Right0, Left1)
				: FIndex3i(Left0, Left1, Right0);
			const FIndex3i CornersB = bCrossPlane
				? FIndex3i(Right1, Left1, Right0)
				: FIndex3i(Right0, Left1, Right1);
			const int32 TriangleA = Mesh.AppendTriangle(
				VertexIDs[CornersA.A], VertexIDs[CornersA.B], VertexIDs[CornersA.C]);
			const int32 TriangleB = Mesh.AppendTriangle(
				VertexIDs[CornersB.A], VertexIDs[CornersB.B], VertexIDs[CornersB.C]);
			SetTriangleAttributes(Attributes, TriangleA, CornersA,
				NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs, MaterialSlot);
			SetTriangleAttributes(Attributes, TriangleB, CornersB,
				NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs, MaterialSlot);
		}
	}

	return true;
}

bool AppendSplashReference(
	FDynamicMesh3& Mesh,
	const FWaterfallMeshAttributes& Attributes,
	const FTYWaterfallSample& Endpoint,
	const FTransform& WorldToMesh,
	FVector WorldWidthAxis,
	float RibbonWidth,
	float FrontRadius,
	float BackRadius,
	int32 RadialSegments,
	int32 Rings,
	float UVSeed,
	float TopSplinePosition)
{
	// The path sample normal describes the waterfall wall, not the receiving
	// water surface.  Splash must lie on the receiving plane, so use the world
	// up normal here and derive both horizontal axes from it.
	const FVector WorldNormal = FVector::UpVector;
	FVector WorldAcross = FVector::VectorPlaneProject(WorldWidthAxis, WorldNormal).GetSafeNormal();
	if (WorldAcross.IsNearlyZero()) WorldAcross = FVector::CrossProduct(WorldNormal, Endpoint.Tangent).GetSafeNormal();
	if (WorldAcross.IsNearlyZero()) return false;
	const FVector WorldForward = FVector::CrossProduct(WorldAcross, WorldNormal).GetSafeNormal();
	const float HalfWidth = FMath::Max(RibbonWidth * 0.5f, 1.0f);
	const FVector Left = Endpoint.Position - WorldAcross * HalfWidth;
	const FVector Right = Endpoint.Position + WorldAcross * HalfWidth;
	const int32 CapSteps = FMath::Max(RadialSegments, 1);
	const int32 RingCount = FMath::Max(Rings, 1);

	// This mirrors the reference builder's Calculated* arrays.  Positions on a
	// cap stay coincident; only the extrusion direction rotates around the
	// contact normal, which is what gives the splash its rounded ends.
	TArray<FVector> Positions, Directions, Normals, LocalDirections;
	TArray<float> Radii, LocalDistances;
	Positions.Add(Left); Positions.Add(Right);
	Directions.Add(WorldForward); Directions.Add(WorldForward);
	Normals.Add(WorldNormal); Normals.Add(WorldNormal);
	LocalDirections.Add(-WorldForward); LocalDirections.Add(-WorldForward);
	LocalDistances.Add(-HalfWidth); LocalDistances.Add(HalfWidth);
	for (int32 Index = 0; Index < CapSteps; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / FMath::Max(CapSteps - 1, 1);
		const float Angle = 180.0f * static_cast<float>(Index + 1)
			/ (FMath::Max(CapSteps - 1, 1) + 2.0f);
		Positions.Add(Right);
		Directions.Add(WorldForward.RotateAngleAxis(Angle, WorldNormal).GetSafeNormal());
		Normals.Add(WorldNormal);
		LocalDirections.Add((-WorldForward).RotateAngleAxis(Angle, WorldNormal).GetSafeNormal());
		Radii.Add(FMath::Lerp(FrontRadius, BackRadius, Alpha));
		LocalDistances.Add(HalfWidth);
	}
	Positions.Add(Right); Directions.Add(-WorldForward); Normals.Add(WorldNormal);
	LocalDirections.Add(WorldForward); LocalDistances.Add(HalfWidth); Radii.Add(BackRadius);
	Positions.Add(Left); Directions.Add(-WorldForward); Normals.Add(WorldNormal);
	LocalDirections.Add(WorldForward); LocalDistances.Add(-HalfWidth); Radii.Add(BackRadius);
	for (int32 Index = 0; Index <= CapSteps; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / FMath::Max(CapSteps, 1);
		const float Angle = 180.0f * static_cast<float>(Index + 1)
			/ (FMath::Max(CapSteps, 1) + 1.0f);
		Positions.Add(Left);
		Directions.Add((-WorldForward).RotateAngleAxis(Angle, WorldNormal).GetSafeNormal());
		Normals.Add(WorldNormal);
		LocalDirections.Add(WorldForward.RotateAngleAxis(Angle, WorldNormal).GetSafeNormal());
		Radii.Add(FMath::Lerp(BackRadius, FrontRadius, Alpha));
		LocalDistances.Add(-HalfWidth);
	}

	// Rotate the already closed reference outline as one rigid shape.  Rotating
	// only WorldForward would leave the left/right endpoints in their old order,
	// causing the two rounded caps to cross over each other.
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		Positions[Index] = Endpoint.Position
			+ (Positions[Index] - Endpoint.Position).RotateAngleAxis(180.0f, WorldNormal);
		Directions[Index] = Directions[Index].RotateAngleAxis(180.0f, WorldNormal).GetSafeNormal();
		LocalDirections[Index] = LocalDirections[Index].RotateAngleAxis(180.0f, WorldNormal).GetSafeNormal();
	}
	// Add the fixed radii for the two width-line samples after the cap arrays.
	Radii.Insert(FrontRadius, 0); Radii.Insert(FrontRadius, 1);

	float OuterPerimeter = 0.0f;
	TArray<float> OuterDistance; OuterDistance.SetNumZeroed(Positions.Num());
	for (int32 Index = 1; Index < Positions.Num(); ++Index)
	{
		OuterPerimeter += FVector::Distance(Positions[Index] + Directions[Index] * Radii[Index],
			Positions[Index - 1] + Directions[Index - 1] * Radii[Index - 1]);
		OuterDistance[Index] = OuterPerimeter;
	}
	OuterPerimeter = FMath::Max(OuterPerimeter, 1.0f);

	const FVector3f LocalNormal = FVector3f(WorldToMesh.TransformVectorNoScale(WorldNormal).GetSafeNormal());
	const FVector2f UV2(Endpoint.Speed, Endpoint.Flow);
	const FVector2f UV3(UVSeed, TopSplinePosition);
	TArray<int32> VertexIDs, NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs;
	for (int32 OutlineIndex = 0; OutlineIndex < Positions.Num(); ++OutlineIndex)
	{
		for (int32 RingIndex = 0; RingIndex <= RingCount; ++RingIndex)
		{
			const float Falloff = static_cast<float>(RingIndex) / RingCount;
			const FVector Position = Positions[OutlineIndex] + Directions[OutlineIndex] * Radii[OutlineIndex] * Falloff;
			VertexIDs.Add(Mesh.AppendVertex(FVector3d(WorldToMesh.TransformPosition(Position))));
			NormalIDs.Add(Attributes.Normals->AppendElement(LocalNormal));
			// Splash: UV0 follows the closed perimeter and uses Y as radial falloff
			// (1 at the outline, 0 at the displaced inner ring).
			UV0IDs.Add(Attributes.UV0->AppendElement(FVector2f(OuterDistance[OutlineIndex] / OuterPerimeter, 1.0f - Falloff)));
			const FVector LocalUV = LocalDirections[OutlineIndex] * Radii[OutlineIndex] * Falloff;
			// Splash: UV1 stores the local across/forward projection of the radial
			// displacement, in Unreal units. UV2/UV3 remain endpoint metadata.
			UV1IDs.Add(Attributes.UV1->AppendElement(FVector2f(LocalDistances[OutlineIndex] + FVector::DotProduct(WorldAcross, LocalUV), FVector::DotProduct(WorldForward, LocalUV))));
			UV2IDs.Add(Attributes.UV2->AppendElement(UV2));
			UV3IDs.Add(Attributes.UV3->AppendElement(UV3));
			ColorIDs.Add(Attributes.Colors->AppendElement(MakeWaterfallVertexColor(Directions[OutlineIndex], Endpoint.Turbulence)));
		}
	}
	for (int32 OutlineIndex = 0; OutlineIndex < Positions.Num() - 1; ++OutlineIndex)
	{
		for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
		{
			const int32 A = OutlineIndex * (RingCount + 1) + RingIndex;
			const int32 B = A + 1;
			const int32 C = A + RingCount + 1;
			const int32 D = C + 1;
			const FIndex3i First(C, B, A), Second(C, D, B);
			const int32 T0 = Mesh.AppendTriangle(VertexIDs[First.A], VertexIDs[First.B], VertexIDs[First.C]);
			const int32 T1 = Mesh.AppendTriangle(VertexIDs[Second.A], VertexIDs[Second.B], VertexIDs[Second.C]);
			SetTriangleAttributes(Attributes, T0, First, NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs, EWaterfallMaterialSlot::Splash);
			SetTriangleAttributes(Attributes, T1, Second, NormalIDs, UV0IDs, UV1IDs, UV2IDs, UV3IDs, ColorIDs, EWaterfallMaterialSlot::Splash);
		}
	}
	return true;
}
}

UTYWaterfallMeshComponent::UTYWaterfallMeshComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetTangentsType(EDynamicMeshComponentTangentsMode::AutoCalculated);
	SetVisibility(false);
}

#if WITH_EDITOR
bool UTYWaterfallMeshComponent::BuildCombinedMesh(
	const TArray<TObjectPtr<UTYWaterfallPathComponent>>& Paths,
	FVector WorldWidthAxis,
	bool bGenerateSingular,
	bool bGeneratePerPath,
	bool bGenerateCross,
	bool bGenerateSplash,
	float RibbonWidth,
	float CrossWidth,
	float BottomWidthScale,
	int32 PerPathSubdivisions,
	int32 CrossSubdivisions,
	FVector2D BaseUVScale,
	float FrontRadius,
	float BackRadius,
	int32 RadialSegments,
	int32 Rings)
{
	FDynamicMesh3 NewMesh;
	WorldWidthAxis = WorldWidthAxis.GetSafeNormal();
	if (WorldWidthAxis.IsNearlyZero())
	{
		// A failed rebuild must not leave geometry from an earlier successful run.
		SetMesh(MoveTemp(NewMesh));
		SetVisibility(false);
		MarkPackageDirty();
		return false;
	}

	const FWaterfallMeshAttributes Attributes = InitializeAttributes(NewMesh);
	const FTransform WorldToMesh = GetComponentTransform().Inverse();
	const float SafeRibbonWidth = FMath::Max(RibbonWidth, 1.0f);
	const float SafeCrossWidth = FMath::Max(CrossWidth, 1.0f);
	// Keep a non-zero endpoint width so the final ribbon row cannot collapse into
	// degenerate triangles when users request a very narrow bottom.
	const float SafeBottomWidthScale = FMath::Max(BottomWidthScale, 0.01f);
	const float SafeFrontRadius = FMath::Max(FrontRadius, 1.0f);
	const float SafeBackRadius = FMath::Max(BackRadius, 1.0f);
	const int32 SafeRadialSegments = FMath::Clamp(RadialSegments, 3, 128);
	const int32 SafeRings = FMath::Clamp(Rings, 1, 32);
	int32 BuiltSurfaceCount = 0;

	// Singular joins all valid paths into one sheet after remapping their samples
	// to a shared row count. It can coexist with any of the per-path modes.
	if (bGenerateSingular)
	{
		BuiltSurfaceCount += AppendSingular(NewMesh, Attributes, Paths,
			WorldToMesh, WorldWidthAxis, BaseUVScale) ? 1 : 0;
	}

	if (bGeneratePerPath)
	{
		for (const UTYWaterfallPathComponent* Path : Paths)
		{
			if (!IsValid(Path))
			{
				continue;
			}

			BuiltSurfaceCount += AppendRibbon(NewMesh, Attributes,
				Path->GetResampledSamples(), WorldToMesh, WorldWidthAxis,
				SafeRibbonWidth, SafeBottomWidthScale, PerPathSubdivisions,
				BaseUVScale, 0.0f, Path->GetUVSeed(),
				Path->GetNormalizedTopSplinePosition(),
				EWaterfallMaterialSlot::PerPath) ? 1 : 0;
		}
	}

	if (bGenerateCross)
	{
		for (const UTYWaterfallPathComponent* Path : Paths)
		{
			if (IsValid(Path))
			{
				BuiltSurfaceCount += AppendRibbon(NewMesh, Attributes,
					Path->GetResampledSamples(), WorldToMesh, WorldWidthAxis,
					SafeCrossWidth, SafeBottomWidthScale, CrossSubdivisions,
					BaseUVScale, 90.0f, Path->GetUVSeed(),
					Path->GetNormalizedTopSplinePosition(),
					EWaterfallMaterialSlot::Cross) ? 1 : 0;
			}
		}
	}

	if (bGenerateSplash)
	{
		for (const UTYWaterfallPathComponent* Path : Paths)
		{
			if (!IsValid(Path) || Path->GetResampledSamples().IsEmpty())
			{
				continue;
			}

			BuiltSurfaceCount += AppendSplashReference(NewMesh, Attributes,
				Path->GetResampledSamples().Last(), WorldToMesh, WorldWidthAxis,
				SafeRibbonWidth,
				SafeFrontRadius, SafeBackRadius, SafeRadialSegments,
				SafeRings, Path->GetUVSeed(), Path->GetNormalizedTopSplinePosition()) ? 1 : 0;
		}
	}

	SetMesh(MoveTemp(NewMesh));
	SetVisibility(BuiltSurfaceCount > 0);
	MarkPackageDirty();
	return BuiltSurfaceCount > 0;
}

void UTYWaterfallMeshComponent::ClearWaterfallMesh()
{
	FDynamicMesh3 EmptyMesh;
	SetMesh(MoveTemp(EmptyMesh));
	SetVisibility(false);
	MarkPackageDirty();
}
#endif
