#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TerracePreviewComponent.generated.h"

/** Editor visualizer anchor. Does not render geometry or provide collision. */
UCLASS(ClassGroup=(Terrace), meta=(DisplayName="Terrace Preview"))
class TERRACETOOLS_API UTerracePreviewComponent : public USceneComponent
{
    GENERATED_BODY()
};
