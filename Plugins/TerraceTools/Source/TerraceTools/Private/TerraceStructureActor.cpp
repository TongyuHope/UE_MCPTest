#include "TerraceStructureActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "TerraceGroupActor.h"

ATerraceStructureActor::ATerraceStructureActor()
{
    PrimaryActorTick.bCanEverTick = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);
}

void ATerraceStructureActor::Destroyed()
{
#if WITH_EDITOR
    ATerraceGroupActor* Group = SourceGroup.Get();
    if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor && IsValid(Group)
        && Group->GeneratedStructureOutput.ToSoftObjectPath() == FSoftObjectPath(this))
    {
        Group->Modify();
        Group->GeneratedStructureOutput.Reset();
        Group->MarkPackageDirty();
    }
#endif
    Super::Destroyed();
}
