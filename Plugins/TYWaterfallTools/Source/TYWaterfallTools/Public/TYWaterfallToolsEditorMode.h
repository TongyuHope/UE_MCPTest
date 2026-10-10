// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "TYWaterfallToolsEditorMode.generated.h"

class ATYWaterfallActor;

/** Editor mode that coordinates selection and the waterfall authoring toolkit. */
UCLASS()
class UTYWaterfallToolsEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_TYWaterfallToolsEditorModeId;

	UTYWaterfallToolsEditorMode();
	virtual ~UTYWaterfallToolsEditorMode();

	/** UEdMode interface */
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;

	ATYWaterfallActor* GetSelectedWaterfall() const { return SelectedWaterfall.Get(); }

private:
	void RefreshSelectedWaterfall();

	TWeakObjectPtr<ATYWaterfallActor> SelectedWaterfall;
};
