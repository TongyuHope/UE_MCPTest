#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartitionTiledRVTBuilder.generated.h"

/** Builds one RVT streaming texture while loading World Partition writers one tile at a time. */
UCLASS()
class UWorldPartitionTiledRVTBuilder : public UWorldPartitionBuilder
{
    GENERATED_BODY()

public:
    virtual bool RequiresCommandletRendering() const override { return true; }
    virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::Custom; }
    virtual bool CanProcessNonPartitionedWorlds() const override { return false; }

protected:
    virtual bool RunInternal(UWorld* World, const FCellInfo& CellInfo, FPackageSourceControlHelper& PackageHelper) override;
};

