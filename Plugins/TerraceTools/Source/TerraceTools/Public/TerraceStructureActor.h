#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerraceStructureActor.generated.h"

class ATerraceGroupActor;
class UProceduralMeshComponent;

/** Saved runtime geometry. Built explicitly in the editor; never generated on Tick/BeginPlay. */
UCLASS(NotPlaceable, NotBlueprintable)
class TERRACETOOLS_API ATerraceStructureActor : public AActor
{
    GENERATED_BODY()

public:
    ATerraceStructureActor();
    virtual void Destroyed() override;

    UPROPERTY()
    TSoftObjectPtr<ATerraceGroupActor> SourceGroup;

    UPROPERTY(VisibleAnywhere, Category="梯田工具 / Terrace Tools")
    TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;

    UPROPERTY(VisibleAnywhere, Category="梯田工具 / Terrace Tools")
    TArray<FGuid> PlotIds;
};
