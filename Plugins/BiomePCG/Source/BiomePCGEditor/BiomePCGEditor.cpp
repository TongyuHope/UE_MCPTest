#include "BiomePCGEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ProjectionDebug.h"
#include "SplinePainter/BiomeSplinePainterEdMode.h"
#include "EditorModeManager.h"
#include "EditorModeRegistry.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PCGGraph.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "FBiomePCGEditorModule"

namespace PCGProjectionDebug
{
    static const FString ScopePath(TEXT("/Game/PCG"));

    bool IsInScope(const UObject* Asset)
    {
        if (!Asset)
        {
            return false;
        }

        const FString PackageName = Asset->GetOutermost()->GetName();
        return PackageName == ScopePath || PackageName.StartsWith(ScopePath + TEXT("/"));
    }
}

void FBiomePCGEditorModule::StartupModule()
{
    if (!IsRunningCommandlet())
    {
        FEditorModeRegistry::Get().RegisterMode<FBiomeSplinePainterEdMode>(
            FBiomeSplinePainterEdMode::ModeId,
            LOCTEXT("SplinePainterMode", "Biome 样条绘制"), FSlateIcon(), true);
        bSplinePainterRegistered = true;
        SplineMenuCallbackHandle = UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBiomePCGEditorModule::RegisterSplinePainterMenu));
    }

    ObjectPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddRaw(
        this, &FBiomePCGEditorModule::HandleObjectPreSave);

    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditorOpeningHandle = AssetEditorSubsystem->OnEditorOpeningPreWidgets().AddRaw(
                this, &FBiomePCGEditorModule::HandleAssetsOpening);
        }
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("ProjectionDebugApplyAll")))
    {
        MigrationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FBiomePCGEditorModule::RunRequestedMigration), 0.2f);
    }
}

void FBiomePCGEditorModule::ShutdownModule()
{
    if (bSplinePainterRegistered)
    {
        UToolMenus::UnRegisterStartupCallback(SplineMenuCallbackHandle);
        UToolMenus::UnregisterOwner(this);
        FEditorModeRegistry::Get().UnregisterMode(FBiomeSplinePainterEdMode::ModeId);
        bSplinePainterRegistered = false;
    }

    if (MigrationTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(MigrationTickerHandle);
        MigrationTickerHandle.Reset();
    }

    if (AssetEditorOpeningHandle.IsValid() && GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditorSubsystem->OnEditorOpeningPreWidgets().Remove(AssetEditorOpeningHandle);
        }
        AssetEditorOpeningHandle.Reset();
    }

    if (ObjectPreSaveHandle.IsValid())
    {
        FCoreUObjectDelegates::OnObjectPreSave.Remove(ObjectPreSaveHandle);
        ObjectPreSaveHandle.Reset();
    }
}

void FBiomePCGEditorModule::RegisterSplinePainterMenu()
{
    FToolMenuOwnerScoped OwnerScoped(this);
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.TATools");
    FToolMenuSection& Section = Menu->FindOrAddSection("BiomeSplinePainter");
    Section.AddMenuEntry(
        "BiomeSplinePainter",
        LOCTEXT("SplinePainterMenu", "绘制样条植被"),
        LOCTEXT("SplinePainterTooltip", "单击放置 Actor，之后每次按住左键绘制一条新样条。"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([]()
        {
            GLevelEditorModeTools().ActivateMode(FBiomeSplinePainterEdMode::ModeId);
        }), FCanExecuteAction::CreateLambda([]()
        {
            return GEditor && !GEditor->PlayWorld;
        })));
}

void FBiomePCGEditorModule::HandleAssetsOpening(
    const TArray<UObject*>& Assets,
    IAssetEditorInstance* AssetEditorInstance)
{
    for (UObject* Asset : Assets)
    {
        EnsureAssetProjectionDebug(Asset);
    }
}

void FBiomePCGEditorModule::HandleObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
{
    EnsureAssetProjectionDebug(Object);
}

bool FBiomePCGEditorModule::RunRequestedMigration(float DeltaTime)
{
    MigrationTickerHandle.Reset();
    ApplyAllProjectionDebugAndExit();
    return false;
}

int32 FBiomePCGEditorModule::EnsureAssetProjectionDebug(UObject* Asset)
{
    if (!PCGProjectionDebug::IsInScope(Asset))
    {
        return 0;
    }

    if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
    {
        return EnsureBlueprintProjectionDebug(Blueprint);
    }

    if (UPCGGraph* PCGGraph = Cast<UPCGGraph>(Asset))
    {
        return EnsurePCGGraphProjectionDebug(PCGGraph) ? 1 : 0;
    }

    return 0;
}

int32 FBiomePCGEditorModule::EnsureBlueprintProjectionDebug(UBlueprint* Blueprint)
{
    if (!Blueprint || !PCGProjectionDebug::IsInScope(Blueprint))
    {
        return 0;
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);

    int32 ChangedGraphCount = 0;
    for (UEdGraph* Graph : Graphs)
    {
        ChangedGraphCount += EnsureBlueprintGraphProjectionDebug(Graph) ? 1 : 0;
    }

    if (ChangedGraphCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    return ChangedGraphCount;
}

bool FBiomePCGEditorModule::EnsureBlueprintGraphProjectionDebug(UEdGraph* Graph)
{
    if (!Graph)
    {
        return false;
    }

    UBlueprintProjectionDebug* ProjectionDebugNode = nullptr;
    TArray<UProjectionDebug*> LegacyProjectionDebugNodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UBlueprintProjectionDebug* Existing = Cast<UBlueprintProjectionDebug>(Node))
        {
            ProjectionDebugNode = Existing;
        }
        else if (UProjectionDebug* Legacy = Cast<UProjectionDebug>(Node))
        {
            LegacyProjectionDebugNodes.Add(Legacy);
        }
    }

    bool bChanged = false;
    if (!LegacyProjectionDebugNodes.IsEmpty())
    {
        Graph->Modify();
        for (UProjectionDebug* Legacy : LegacyProjectionDebugNodes)
        {
            Graph->Nodes.Remove(Legacy);
        }
        bChanged = true;
    }

    if (!ProjectionDebugNode)
    {
        Graph->Modify();
        ProjectionDebugNode = NewObject<UBlueprintProjectionDebug>(Graph, NAME_None, RF_Transactional);
        ProjectionDebugNode->CreateNewGuid();
        ProjectionDebugNode->PostPlacedNewNode();
        ProjectionDebugNode->AllocateDefaultPins();
        Graph->AddNode(ProjectionDebugNode, false, false);
        bChanged = true;
    }

    bChanged |= ProjectionDebugNode->EnforceProjectionDebugProperties();

    if (bChanged)
    {
        Graph->NotifyGraphChanged();
    }

    return bChanged;
}

bool FBiomePCGEditorModule::EnsurePCGGraphProjectionDebug(UPCGGraph* PCGGraph)
{
    if (!PCGGraph || !PCGProjectionDebug::IsInScope(PCGGraph))
    {
        return false;
    }

    for (UObject* ExtraNode : PCGGraph->GetExtraEditorNodes())
    {
        if (UProjectionDebug* Existing = Cast<UProjectionDebug>(ExtraNode))
        {
            if (Existing->EnforceProjectionDebugProperties())
            {
                PCGGraph->MarkPackageDirty();
                return true;
            }
            return false;
        }
    }

    TArray<TObjectPtr<const UObject>> ExtraNodes;
    ExtraNodes.Reserve(PCGGraph->GetExtraEditorNodes().Num() + 1);
    for (const UObject* ExtraNode : PCGGraph->GetExtraEditorNodes())
    {
        ExtraNodes.Add(ExtraNode);
    }

    UProjectionDebug* ProjectionDebugNode = NewObject<UProjectionDebug>(GetTransientPackage());
    ProjectionDebugNode->CreateNewGuid();
    ProjectionDebugNode->PostPlacedNewNode();
    ProjectionDebugNode->AllocateDefaultPins();
    ExtraNodes.Add(ProjectionDebugNode);

    PCGGraph->Modify();
    PCGGraph->SetExtraEditorNodes(ExtraNodes);
    PCGGraph->MarkPackageDirty();
    return true;
}

void FBiomePCGEditorModule::ApplyAllProjectionDebugAndExit()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.SearchAllAssets(true);

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPath(FName(PCGProjectionDebug::ScopePath), Assets, true, false);

    int32 MatchingAssetCount = 0;
    int32 ModifiedGraphCount = 0;
    TArray<UPackage*> PackagesToSave;
    TArray<TSharedPtr<FJsonValue>> ModifiedAssetValues;

    for (const FAssetData& AssetData : Assets)
    {
        UObject* Asset = AssetData.GetAsset();
        if (!Cast<UBlueprint>(Asset) && !Cast<UPCGGraph>(Asset))
        {
            continue;
        }

        ++MatchingAssetCount;
        const int32 AssetModifiedGraphCount = EnsureAssetProjectionDebug(Asset);
        if (AssetModifiedGraphCount > 0)
        {
            ModifiedGraphCount += AssetModifiedGraphCount;
            PackagesToSave.AddUnique(Asset->GetOutermost());
            ModifiedAssetValues.Add(MakeShared<FJsonValueString>(AssetData.PackageName.ToString()));
        }
    }

    const bool bSaveSucceeded = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);

    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"), PCGProjectionDebug::ScopePath);
    Report->SetNumberField(TEXT("scanned_asset_count"), Assets.Num());
    Report->SetNumberField(TEXT("matching_asset_count"), MatchingAssetCount);
    Report->SetNumberField(TEXT("modified_asset_count"), PackagesToSave.Num());
    Report->SetNumberField(TEXT("modified_graph_count"), ModifiedGraphCount);
    Report->SetBoolField(TEXT("save_succeeded"), bSaveSucceeded);
    Report->SetStringField(TEXT("projection_debug_text"), UProjectionDebug::GetProjectionDebugText());
    Report->SetArrayField(TEXT("modified_assets"), ModifiedAssetValues);

    FString ReportText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportText);
    FJsonSerializer::Serialize(Report, Writer);

    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Codex"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString ReportPath = FPaths::Combine(ReportDirectory, TEXT("projection_debug_migration_report.json"));
    FFileHelper::SaveStringToFile(ReportText, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UE_LOG(LogTemp, Display,
        TEXT("ProjectionDebug migration: scanned=%d matching=%d modified_assets=%d modified_graphs=%d saved=%s report=%s"),
        Assets.Num(), MatchingAssetCount, PackagesToSave.Num(), ModifiedGraphCount,
        bSaveSucceeded ? TEXT("true") : TEXT("false"), *ReportPath);

    FPlatformMisc::RequestExit(false);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBiomePCGEditorModule, BiomePCGEditor)
