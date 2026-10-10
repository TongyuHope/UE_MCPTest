// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"
#include "TYWaterfallToolsEditorMode.h"

class ATYWaterfallActor;
class SWidget;

/** Compact authoring panel for the currently selected waterfall actor. */
class FTYWaterfallToolsEditorModeToolkit : public FModeToolkit
{
public:
	FTYWaterfallToolsEditorModeToolkit();

	/** FModeToolkit interface */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	void SetSelectedWaterfall(ATYWaterfallActor* InWaterfall);

private:
	FText GetSelectionText() const;
	FText GetStatusText() const;
	TOptional<float> GetGenerationProgress() const;
	bool CanRunGenerationCommand() const;
	bool CanCancelGeneration() const;
	bool CanEditSettings() const;
	FReply OnEditTopSplineClicked();
	FReply OnEditKillPlaneClicked();
	FReply OnGeneratePathsClicked();
	FReply OnGenerateMeshClicked();
	FReply OnBakeStaticMeshClicked();
	FReply OnRefreshNiagaraClicked();
	FReply OnClearClicked();
	FReply OnCancelClicked();

	TWeakObjectPtr<ATYWaterfallActor> SelectedWaterfall;
};
