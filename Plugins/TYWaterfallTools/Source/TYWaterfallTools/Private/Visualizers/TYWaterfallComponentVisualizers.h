// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

/** Draws mesh vertices while the component's wireframe debug mode is enabled. */
class FTYWaterfallMeshComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(
		const UActorComponent* Component,
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;
};
