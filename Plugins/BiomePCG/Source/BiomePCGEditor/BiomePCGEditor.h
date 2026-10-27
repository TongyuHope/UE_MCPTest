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
    bool RunRequestedCleanup(float DeltaTime);

    int32 RemoveAssetProjectionDebug(UObject* Asset);
    int32 RemoveBlueprintProjectionDebug(UBlueprint* Blueprint);
    bool RemoveBlueprintGraphProjectionDebug(UEdGraph* Graph);
    bool RemovePCGGraphProjectionDebug(UPCGGraph* PCGGraph);
    void RemoveAllProjectionDebugAndExit();

    FDelegateHandle AssetEditorOpeningHandle;
    FDelegateHandle ObjectPreSaveHandle;
    FTSTicker::FDelegateHandle CleanupTickerHandle;
    FDelegateHandle SplineMenuCallbackHandle;
    bool bSplinePainterRegistered = false;
};
