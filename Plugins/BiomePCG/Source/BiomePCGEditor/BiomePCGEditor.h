#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UBlueprint;
class UEdGraph;
class UPCGGraph;
class IAssetEditorInstance;
class FObjectPreSaveContext;

class FBiomePCGEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterSplinePainterMenu();
    void HandleAssetsOpening(const TArray<UObject*>& Assets, IAssetEditorInstance* AssetEditorInstance);
    void HandleObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
    bool RunRequestedMigration(float DeltaTime);

    int32 EnsureAssetProjectionDebug(UObject* Asset);
    int32 EnsureBlueprintProjectionDebug(UBlueprint* Blueprint);
    bool EnsureBlueprintGraphProjectionDebug(UEdGraph* Graph);
    bool EnsurePCGGraphProjectionDebug(UPCGGraph* PCGGraph);
    void ApplyAllProjectionDebugAndExit();

    FDelegateHandle AssetEditorOpeningHandle;
    FDelegateHandle ObjectPreSaveHandle;
    FTSTicker::FDelegateHandle MigrationTickerHandle;
    FDelegateHandle SplineMenuCallbackHandle;
    bool bSplinePainterRegistered = false;
};
