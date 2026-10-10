// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "TYWaterfallMeshComponent.generated.h"

class UTYWaterfallPathComponent;

/** Dynamic mesh component that stores the combined waterfall representation. */
UCLASS(ClassGroup = (Waterfall), meta = (DisplayName = "TY Waterfall Mesh"))
class TYWATERFALLTOOLSRUNTIME_API UTYWaterfallMeshComponent : public UDynamicMeshComponent
{
	GENERATED_BODY()

public:
	UTYWaterfallMeshComponent(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	/** Replaces the mesh with the enabled Singular, Per-Path, Cross and Splash surfaces. */
	bool BuildCombinedMesh(
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
		int32 Rings);

	void ClearWaterfallMesh();
#endif
};
