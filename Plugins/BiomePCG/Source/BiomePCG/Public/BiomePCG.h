// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Delegates/Delegate.h"

class FBiomePCGModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDelegateHandle OnRegisterTabsHandle;
	FDelegateHandle OnRegisterTabsHandle2;
	FDelegateHandle OnRegisterTabsHandle3;
	FDelegateHandle MenuCallbackHandle;
};
