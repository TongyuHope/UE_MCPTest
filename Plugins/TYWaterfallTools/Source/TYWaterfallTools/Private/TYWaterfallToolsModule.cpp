// Copyright Epic Games, Inc. All Rights Reserved.

#include "TYWaterfallToolsModule.h"
#include "TYWaterfallToolsEditorModeCommands.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "Details/TYWaterfallDetailsCustomization.h"
#include "Editor/UnrealEdEngine.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"
#include "Visualizers/TYWaterfallComponentVisualizers.h"

#define LOCTEXT_NAMESPACE "TYWaterfallToolsModule"

void FTYWaterfallToolsModule::StartupModule()
{
	FTYWaterfallToolsEditorModeCommands::Register();

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
		TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomClassLayout(
		ATYWaterfallActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FTYWaterfallActorDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(
		UTYWaterfallSettingsComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FTYWaterfallSettingsDetails::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();

	if (GUnrealEd)
	{
		TSharedPtr<FComponentVisualizer> MeshVisualizer =
			MakeShared<FTYWaterfallMeshComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(
			UTYWaterfallMeshComponent::StaticClass()->GetFName(), MeshVisualizer);
		MeshVisualizer->OnRegister();
	}
}

void FTYWaterfallToolsModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UTYWaterfallMeshComponent::StaticClass()->GetFName());
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(ATYWaterfallActor::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomClassLayout(
			UTYWaterfallSettingsComponent::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}

	FTYWaterfallToolsEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTYWaterfallToolsModule, TYWaterfallTools)
