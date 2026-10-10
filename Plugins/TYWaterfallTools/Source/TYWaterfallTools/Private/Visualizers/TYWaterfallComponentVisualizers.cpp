// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/TYWaterfallComponentVisualizers.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "SceneManagement.h"

void FTYWaterfallMeshComponentVisualizer::DrawVisualization(
	const UActorComponent* Component,
	const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UTYWaterfallMeshComponent* MeshComponent = Cast<UTYWaterfallMeshComponent>(Component);
	const ATYWaterfallActor* Waterfall = MeshComponent
		? MeshComponent->GetOwner<ATYWaterfallActor>() : nullptr;
	const UTYWaterfallSettingsComponent* Settings = Waterfall
		? Waterfall->GetWaterfallSettings() : nullptr;
	const UE::Geometry::FDynamicMesh3* Mesh = MeshComponent ? MeshComponent->GetMesh() : nullptr;
	if (!Mesh || !Settings || !Settings->ShouldShowMeshWireframe())
	{
		return;
	}

	const FTransform ComponentTransform = MeshComponent->GetComponentTransform();
	for (int32 VertexID : Mesh->VertexIndicesItr())
	{
		const FVector WorldPosition = ComponentTransform.TransformPosition(
			FVector(Mesh->GetVertex(VertexID)));
		PDI->DrawPoint(WorldPosition, FLinearColor::Green, 4.0f, SDPG_World);
	}
}
