// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/TYWaterfallDetailsCustomization.h"

#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "TYWaterfallDetailsCustomization"

TSharedRef<IDetailCustomization> FTYWaterfallActorDetails::MakeInstance()
{
	return MakeShared<FTYWaterfallActorDetails>();
}

void FTYWaterfallActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// These categories expose owned implementation components and generated data.
	// Authoring parameters live on WaterfallSettings and appear in the mode panel.
	DetailBuilder.HideCategory(TEXT("Components"));
	DetailBuilder.HideCategory(TEXT("Generated Paths"));
	DetailBuilder.HideCategory(TEXT("Editor"));
	DetailBuilder.HideCategory(TEXT("Waterfall|Simulation"));
	DetailBuilder.HideCategory(TEXT("Waterfall|Mesh"));
}

TSharedRef<IDetailCustomization> FTYWaterfallSettingsDetails::MakeInstance()
{
	return MakeShared<FTYWaterfallSettingsDetails>();
}

void FTYWaterfallSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// UActorComponent contributes many generic categories that are irrelevant for
	// this editor-only settings object and obscure the waterfall parameters.
	DetailBuilder.HideCategory(TEXT("Activation"));
	DetailBuilder.HideCategory(TEXT("AssetUserData"));
	DetailBuilder.HideCategory(TEXT("ComponentTick"));
	DetailBuilder.HideCategory(TEXT("Cooking"));
	DetailBuilder.HideCategory(TEXT("Events"));
	DetailBuilder.HideCategory(TEXT("Replication"));
	DetailBuilder.HideCategory(TEXT("Tags"));
	DetailBuilder.HideCategory(TEXT("Variable"));

	DetailBuilder.EditCategory(TEXT("Paths"), LOCTEXT("Paths", "Paths"), ECategoryPriority::Important);
	DetailBuilder.EditCategory(TEXT("Simulation"), LOCTEXT("Simulation", "Simulation"), ECategoryPriority::Important);
	DetailBuilder.EditCategory(TEXT("Mesh"), LOCTEXT("Mesh", "Mesh"), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory(TEXT("Material"), LOCTEXT("Material", "Material"), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory(TEXT("Niagara"), LOCTEXT("Niagara", "Niagara"), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory(TEXT("Bake"), LOCTEXT("Bake", "Bake"), ECategoryPriority::TypeSpecific);
	DetailBuilder.EditCategory(TEXT("Performance"), LOCTEXT("Performance", "Performance"), ECategoryPriority::Uncommon);
	DetailBuilder.EditCategory(TEXT("Debug"), LOCTEXT("Debug", "Debug"), ECategoryPriority::Uncommon);
}

#undef LOCTEXT_NAMESPACE
