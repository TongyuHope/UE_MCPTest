#pragma once

#include "CoreMinimal.h"

class ATerraceGroupActor;

namespace TerraceTerrainBuilder
{
    bool Update(ATerraceGroupActor* Group, FText& OutReport);
    bool RemoveContribution(ATerraceGroupActor* Group, FText& OutReport);
}
