// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/TYWaterfallMeshBuilder.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallPathComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#if WITH_EDITOR
void FTYWaterfallMeshBuilder::Initialize(ATYWaterfallActor* InOwner)
{
	Owner = InOwner;
}

bool FTYWaterfallMeshBuilder::BuildMesh()
{
	ATYWaterfallActor* Waterfall = Owner.Get();
	if (!IsValid(Waterfall) || !IsValid(Waterfall->DynamicMeshComponent)
		|| !IsValid(Waterfall->WaterfallSettings) || Waterfall->GeneratedPaths.IsEmpty())
	{
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT(
		"TYWaterfallTools", "GenerateWaterfallMesh", "Generate Waterfall Mesh"));
	Waterfall->Modify();
	Waterfall->DynamicMeshComponent->Modify();
	for (UTYWaterfallPathComponent* Path : Waterfall->GeneratedPaths)
	{
		if (IsValid(Path))
		{
			Path->BuildResampledSamples(Waterfall->WaterfallSettings->GetMeshSampleSpacing());
		}
	}

	const FVector WidthAxis = Waterfall->TopSpline->GetDirectionAtTime(
		0.5f, ESplineCoordinateSpace::World, true).GetSafeNormal();
	const bool bBuilt = Waterfall->DynamicMeshComponent->BuildCombinedMesh(
		Waterfall->GeneratedPaths,
		WidthAxis,
		Waterfall->WaterfallSettings->ShouldGenerateSingular(),
		Waterfall->WaterfallSettings->ShouldGeneratePerPath(),
		Waterfall->WaterfallSettings->ShouldGenerateCross(),
		Waterfall->WaterfallSettings->ShouldGenerateSplash(),
		Waterfall->WaterfallSettings->GetRibbonWidth(),
		Waterfall->WaterfallSettings->GetCrossWidth(),
		Waterfall->WaterfallSettings->GetBottomWidthScale(),
		Waterfall->WaterfallSettings->GetPerPathSubdivisions(),
		Waterfall->WaterfallSettings->GetCrossSubdivisions(),
		Waterfall->WaterfallSettings->GetBaseUVScale(),
		Waterfall->WaterfallSettings->GetSplashFrontRadius(),
		Waterfall->WaterfallSettings->GetSplashBackRadius(),
		Waterfall->WaterfallSettings->GetSplashRadialSegments(),
		Waterfall->WaterfallSettings->GetSplashRings());

	if (bBuilt)
	{
		Waterfall->DynamicMeshComponent->SetEnableWireframeRenderPass(
			Waterfall->WaterfallSettings->ShouldShowMeshWireframe());
		Waterfall->DynamicMeshComponent->MarkRenderStateDirty();
		// Slot indices are intentionally stable even when a mesh mode is disabled.
		// Triangle material IDs use the same Singular/Per-Path/Cross/Splash mapping.
		Waterfall->DynamicMeshComponent->SetMaterial(
			0, Waterfall->WaterfallSettings->GetSingularMaterial());
		Waterfall->DynamicMeshComponent->SetMaterial(
			1, Waterfall->WaterfallSettings->GetPerPathMaterial());
		Waterfall->DynamicMeshComponent->SetMaterial(
			2, Waterfall->WaterfallSettings->GetCrossMaterial());
		Waterfall->DynamicMeshComponent->SetMaterial(
			3, Waterfall->WaterfallSettings->GetSplashMaterial());
		Waterfall->SetShowBakedMesh(
			Waterfall->WaterfallSettings->ShouldShowBakedMesh());
		Waterfall->MarkPackageDirty();
	}
	return bBuilt;
}

void FTYWaterfallMeshBuilder::ClearMesh()
{
	if (ATYWaterfallActor* Waterfall = Owner.Get())
	{
		if (IsValid(Waterfall->DynamicMeshComponent))
		{
			Waterfall->DynamicMeshComponent->Modify();
			Waterfall->DynamicMeshComponent->ClearWaterfallMesh();
		}
	}
}
#endif
