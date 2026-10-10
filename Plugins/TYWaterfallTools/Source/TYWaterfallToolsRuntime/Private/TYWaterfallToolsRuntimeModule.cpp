// Copyright Epic Games, Inc. All Rights Reserved.

#include "TYWaterfallToolsRuntimeModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogTYWaterfallToolsRuntime, Log, All);

void FTYWaterfallToolsRuntimeModule::StartupModule()
{
	UE_LOG(LogTYWaterfallToolsRuntime, Log, TEXT("TYWaterfallToolsRuntime module started."));
}

void FTYWaterfallToolsRuntimeModule::ShutdownModule()
{
	UE_LOG(LogTYWaterfallToolsRuntime, Log, TEXT("TYWaterfallToolsRuntime module shutting down."));
}

IMPLEMENT_MODULE(FTYWaterfallToolsRuntimeModule, TYWaterfallToolsRuntime)
