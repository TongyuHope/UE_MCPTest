// Copyright Epic Games, Inc. All Rights Reserved.

#include "TYWaterfallToolsEditorMode.h"
#include "TYWaterfallToolsEditorModeToolkit.h"

#include "Actors/TYWaterfallActor.h"
#include "Editor.h"
#include "Engine/Selection.h"


#define LOCTEXT_NAMESPACE "TYWaterfallToolsEditorMode"

const FEditorModeID UTYWaterfallToolsEditorMode::EM_TYWaterfallToolsEditorModeId = TEXT("EM_TYWaterfallToolsEditorMode");

UTYWaterfallToolsEditorMode::UTYWaterfallToolsEditorMode()
{
	Info = FEditorModeInfo(UTYWaterfallToolsEditorMode::EM_TYWaterfallToolsEditorModeId,
		LOCTEXT("ModeName", "TYWaterfallTools"),
		FSlateIcon(),
		true);
}


UTYWaterfallToolsEditorMode::~UTYWaterfallToolsEditorMode()
{
}


void UTYWaterfallToolsEditorMode::ActorSelectionChangeNotify()
{
	Super::ActorSelectionChangeNotify();
	RefreshSelectedWaterfall();
}

void UTYWaterfallToolsEditorMode::Enter()
{
	Super::Enter();
	RefreshSelectedWaterfall();
}

void UTYWaterfallToolsEditorMode::Exit()
{
	// Generation is owned by the actor, so leaving the mode must explicitly stop
	// any editor-time task before the toolkit releases its selection reference.
	if (ATYWaterfallActor* Waterfall = SelectedWaterfall.Get())
	{
		Waterfall->SetKillPlaneEditorVisible(false);
		if (Waterfall->IsGeneratingPaths())
		{
			Waterfall->CancelPathGeneration();
		}
	}

	SelectedWaterfall.Reset();
	if (Toolkit.IsValid())
	{
		StaticCastSharedPtr<FTYWaterfallToolsEditorModeToolkit>(Toolkit)->SetSelectedWaterfall(nullptr);
	}

	Super::Exit();
}

void UTYWaterfallToolsEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FTYWaterfallToolsEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UTYWaterfallToolsEditorMode::GetModeCommands() const
{
	return {};
}

void UTYWaterfallToolsEditorMode::RefreshSelectedWaterfall()
{
	if (ATYWaterfallActor* PreviousWaterfall = SelectedWaterfall.Get())
	{
		PreviousWaterfall->SetKillPlaneEditorVisible(false);
	}
	SelectedWaterfall.Reset();
	if (GEditor)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				if (ATYWaterfallActor* Waterfall = Cast<ATYWaterfallActor>(*It))
				{
					SelectedWaterfall = Waterfall;
					Waterfall->SetKillPlaneEditorVisible(true);
					break;
				}
			}
		}
	}

	if (Toolkit.IsValid())
	{
		StaticCastSharedPtr<FTYWaterfallToolsEditorModeToolkit>(Toolkit)
			->SetSelectedWaterfall(SelectedWaterfall.Get());
	}
}

#undef LOCTEXT_NAMESPACE
