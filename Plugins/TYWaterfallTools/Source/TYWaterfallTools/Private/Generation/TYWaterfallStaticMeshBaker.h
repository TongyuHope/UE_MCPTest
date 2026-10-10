// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class ATYWaterfallActor;

/** Editor-only bridge from the generated dynamic mesh to a saved Static Mesh asset. */
struct FTYWaterfallStaticMeshBaker
{
	static bool Bake(ATYWaterfallActor& Waterfall);
};
