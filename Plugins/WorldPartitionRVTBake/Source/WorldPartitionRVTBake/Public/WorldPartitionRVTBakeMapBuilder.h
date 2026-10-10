#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartitionRVTBakeMapBuilder.generated.h"

/** Exports terrain and explicitly selected PCG/RVT actors to a new World Partition map. */
UCLASS(meta = (DisplayName = "选择性导出 RVT 烘焙地图"))
class UWorldPartitionRVTBakeMapBuilder : public UWorldPartitionBuilder
{
    GENERATED_BODY()

public:
    virtual bool RequiresCommandletRendering() const override { return false; }
    virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::Custom; }
    virtual bool CanProcessNonPartitionedWorlds() const override { return false; }

protected:
    virtual bool RunInternal(UWorld* World, const FCellInfo& CellInfo, FPackageSourceControlHelper& PackageHelper) override;

private:
    // Keep the duplicated world and its objects referenced while processing actor clusters.
    UPROPERTY()
    TMap<TObjectPtr<UObject>, TObjectPtr<UObject>> DuplicatedObjects;
};
