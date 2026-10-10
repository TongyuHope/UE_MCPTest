// Copyright Epic Games, Inc. All Rights Reserved.

#include "TYWaterfallToolsEditorModeCommands.h"
#include "TYWaterfallToolsEditorMode.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "TYWaterfallToolsEditorModeCommands"

FTYWaterfallToolsEditorModeCommands::FTYWaterfallToolsEditorModeCommands()
	: TCommands<FTYWaterfallToolsEditorModeCommands>("TYWaterfallToolsEditorMode",
		NSLOCTEXT("TYWaterfallToolsEditorMode", "TYWaterfallToolsEditorModeCommands", "TYWaterfallTools Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FTYWaterfallToolsEditorModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);

	UI_COMMAND(SimpleTool, "Show Actor Info", "Opens message box with info about a clicked actor", EUserInterfaceActionType::Button, FInputChord());
	ToolCommands.Add(SimpleTool);

	UI_COMMAND(InteractiveTool, "Measure Distance", "Measures distance between 2 points (click to set origin, shift-click to set end point)", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(InteractiveTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FTYWaterfallToolsEditorModeCommands::GetCommands()
{
	return FTYWaterfallToolsEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
