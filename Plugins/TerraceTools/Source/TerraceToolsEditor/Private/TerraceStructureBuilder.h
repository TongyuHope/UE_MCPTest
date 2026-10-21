#pragma once

#include "CoreMinimal.h"

class ATerraceGroupActor;

namespace TerraceStructureBuilder
{
    bool Update(ATerraceGroupActor* Group, FText& OutReport);
    bool Clear(ATerraceGroupActor* Group, FText& OutReport);
}
