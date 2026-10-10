// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/TYWaterfallSettingsComponent.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"

UTYWaterfallSettingsComponent::UTYWaterfallSettingsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector2D UTYWaterfallSettingsComponent::GetSpawnRange() const
{
	// Accept reversed ranges in Details while always returning a sorted,
	// normalized interval to the generation code.
	return FVector2D(
		FMath::Clamp(FMath::Min(SpawnRange.X, SpawnRange.Y), 0.0f, 1.0f),
		FMath::Clamp(FMath::Max(SpawnRange.X, SpawnRange.Y), 0.0f, 1.0f));
}

#if WITH_EDITOR
void UTYWaterfallSettingsComponent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(UTYWaterfallSettingsComponent, bShowPathDebug))
	{
		if (ATYWaterfallActor* Waterfall = GetOwner<ATYWaterfallActor>())
		{
			Waterfall->SetPathDebugVisible(bShowPathDebug);
		}
	}

	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(UTYWaterfallSettingsComponent, bShowMeshWireframe))
	{
		if (const ATYWaterfallActor* Waterfall = GetOwner<ATYWaterfallActor>())
		{
			if (UTYWaterfallMeshComponent* Mesh = Waterfall->GetDynamicMeshComponent())
			{
				Mesh->SetEnableWireframeRenderPass(bShowMeshWireframe);
				Mesh->MarkRenderStateDirty();
			}
		}
	}

	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(UTYWaterfallSettingsComponent, bShowBakedMesh))
	{
		if (ATYWaterfallActor* Waterfall = GetOwner<ATYWaterfallActor>())
		{
			Waterfall->SetShowBakedMesh(bShowBakedMesh);
		}
	}
}

void UTYWaterfallSettingsComponent::SetShowBakedMesh(bool bInShowBakedMesh)
{
	Modify();
	bShowBakedMesh = bInShowBakedMesh;
	if (ATYWaterfallActor* Waterfall = GetOwner<ATYWaterfallActor>())
	{
		Waterfall->SetShowBakedMesh(bShowBakedMesh);
	}
	MarkPackageDirty();
}
#endif
