// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ATYWaterfallActor;

/** Converts completed path splines into the current dynamic mesh representation. */
struct TYWATERFALLTOOLSRUNTIME_API FTYWaterfallMeshBuilder
{
#if WITH_EDITOR
public:
	void Initialize(ATYWaterfallActor* InOwner);
	bool BuildMesh();
	void ClearMesh();

private:
	TWeakObjectPtr<ATYWaterfallActor> Owner;
#endif
};
