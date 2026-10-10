// Selective adaptation of UE 5.8 WorldPartitionRenameDuplicateBuilder.cpp.
// The external-package relocation/fixup procedure follows Epic's builder; no source package is saved.
#include "WorldPartitionRVTBakeMapBuilder.h"

#include "Dom/JsonObject.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "EditorWorldUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "LandscapeProxy.h"
#include "LandscapeSplineActor.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PCGComponent.h"
#include "PCGWorldActor.h"
#include "ReferenceCluster.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "VT/RuntimeVirtualTextureVolume.h"
#include "WorldPartition/ActorDescContainerInstanceCollection.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRVTBakeMapBuilder)

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionRVTBakeMap, Log, All);

namespace
{
// As in Epic's builder: only traverse persisted objects owned by the root, never shared assets.
class FReplaceBakeMapReferences : public FArchiveUObject
{
public:
    FReplaceBakeMapReferences(UObject* InRoot, const TMap<TObjectPtr<UObject>, TObjectPtr<UObject>>& InReplacements)
        : Root(InRoot), Replacements(InReplacements)
    {
        SetIsPersistent(true);
        SetShouldSkipCompilingAssets(true);
        ArIgnoreOuterRef = true;
        ArIsObjectReferenceCollector = true;
        ArShouldSkipBulkData = true;
        ArIsModifyingWeakAndStrongReferences = true;
        Visited.Add(Root);
        Root->Serialize(*this);
    }

    virtual FArchive& operator<<(UObject*& Object) override
    {
        if (Object && !Object->IsTemplate() && !Object->HasAnyFlags(RF_Transient))
        {
            // Every reference slot must be repaired, including repeated pointers to one object.
            if (const TObjectPtr<UObject>* Replacement = Replacements.Find(Object))
            {
                Object = *Replacement;
            }
            else
            {
                bool bVisited = false;
                Visited.Add(Object, &bVisited);
                if (!bVisited && Object->IsInOuter(Root))
                {
                    Object->Serialize(*this);
                }
            }
        }
        return *this;
    }

private:
    UObject* Root;
    const TMap<TObjectPtr<UObject>, TObjectPtr<UObject>>& Replacements;
    TSet<UObject*> Visited;
};

bool MatchesActor(const FWorldPartitionActorDescInstance& Desc, const FString& Selector)
{
    return Desc.GetActorName().ToString().Equals(Selector, ESearchCase::IgnoreCase)
        || Desc.GetActorLabel().ToString().Equals(Selector, ESearchCase::IgnoreCase)
        || Desc.GetActorSoftPath().ToString().Equals(Selector, ESearchCase::IgnoreCase)
        || Desc.GetBaseClass().GetAssetName().ToString().Equals(Selector, ESearchCase::IgnoreCase)
        || Desc.GetBaseClass().GetAssetName().ToString().Equals(Selector + TEXT("_C"), ESearchCase::IgnoreCase);
}

bool IsClass(const FWorldPartitionActorDescInstance& Desc, const UClass* Class)
{
    const UClass* NativeClass = Desc.GetActorNativeClass();
    return NativeClass && NativeClass->IsChildOf(Class);
}
}

bool UWorldPartitionRVTBakeMapBuilder::RunInternal(UWorld* World, const FCellInfo&, FPackageSourceControlHelper& PackageHelper)
{
    FString NewPackageName, PCGActorName, RVTVolumeName, ExtraActorNames, ExtraTags;
    GetParamValue(TEXT("NewPackage="), NewPackageName);
    GetParamValue(TEXT("PCGActor="), PCGActorName);
    GetParamValue(TEXT("RVTVolume="), RVTVolumeName);
    GetParamValue(TEXT("IncludeActors="), ExtraActorNames);
    GetParamValue(TEXT("IncludeActorTags="), ExtraTags);
    const bool bReportOnly = HasParam(TEXT("ReportOnly"));
    const FString SourcePackageName = World->GetPackage()->GetName();

    FText InvalidReason;
    if (!NewPackageName.StartsWith(TEXT("/Game/"))
        || !FPackageName::IsValidLongPackageName(NewPackageName, false, &InvalidReason)
        || NewPackageName.Equals(SourcePackageName, ESearchCase::IgnoreCase)
        || PCGActorName.IsEmpty())
    {
        UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Provide -NewPackage=/Game/... (a new map) and -PCGActor=<name/label/class>. %s"), *InvalidReason.ToString());
        return false;
    }
    // Refuse stale external packages as well as an existing .umap. Never overwrite/delete a destination.
    FString DestinationFile;
    if (!FPackageName::TryConvertLongPackageNameToFilename(NewPackageName, DestinationFile, FPackageName::GetMapPackageExtension()))
    {
        return false;
    }
    if (FPackageName::DoesPackageExist(NewPackageName) || IFileManager::Get().FileExists(*DestinationFile))
    {
        UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Destination already exists: %s. Choose a new map name."), *NewPackageName);
        return false;
    }
    for (const FString& ExternalPath : ULevel::GetExternalObjectsPaths(NewPackageName))
    {
        if (IFileManager::Get().DirectoryExists(*FPackageName::LongPackageNameToFilename(ExternalPath)))
        {
            UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Destination external-package directory already exists: %s."), *ExternalPath);
            return false;
        }
    }

    UWorldPartition* Partition = World->GetWorldPartition();
    UActorDescContainerInstance* MainContainer = Partition->GetActorDescContainerInstance();
    TMap<FGuid, const FWorldPartitionActorDescInstance*> Descriptions;
    for (FActorDescContainerInstanceCollection::TConstIterator<> It(Partition); It; ++It)
    {
        if (It->GetContainerInstance() == MainContainer)
        {
            Descriptions.Add(It->GetGuid(), *It);
        }
    }

    TArray<FString> ActorSelectors, TagSelectors;
    ExtraActorNames.ParseIntoArray(ActorSelectors, TEXT(","), true);
    ExtraTags.ParseIntoArray(TagSelectors, TEXT(","), true);
    for (FString& Selector : ActorSelectors) { Selector.TrimStartAndEndInline(); }
    for (FString& Selector : TagSelectors) { Selector.TrimStartAndEndInline(); }
    TSet<FString> MatchedActors, MatchedTags;
    TMap<FGuid, FString> Reasons;
    int32 TerrainCount = 0, PCGCount = 0, RVTCount = 0;
    for (const auto& Pair : Descriptions)
    {
        const FWorldPartitionActorDescInstance& Desc = *Pair.Value;
        if (IsClass(Desc, ALandscapeProxy::StaticClass()) || IsClass(Desc, ALandscapeSplineActor::StaticClass()))
        {
            Reasons.Add(Pair.Key, TEXT("Landscape"));
            ++TerrainCount;
        }
        if (IsClass(Desc, AWorldDataLayers::StaticClass()) || IsClass(Desc, APCGWorldActor::StaticClass()) || IsClass(Desc, AWorldSettings::StaticClass()))
        {
            Reasons.Add(Pair.Key, TEXT("World infrastructure"));
        }
        if (MatchesActor(Desc, PCGActorName))
        {
            Reasons.Add(Pair.Key, TEXT("PCG selection"));
            ++PCGCount;
        }
        if (IsClass(Desc, ARuntimeVirtualTextureVolume::StaticClass()) && (RVTVolumeName.IsEmpty() || MatchesActor(Desc, RVTVolumeName)))
        {
            Reasons.Add(Pair.Key, TEXT("RVT selection"));
            ++RVTCount;
        }
        for (const FString& Selector : ActorSelectors)
        {
            if (MatchesActor(Desc, Selector))
            {
                MatchedActors.Add(Selector);
                Reasons.Add(Pair.Key, TEXT("Explicit actor: ") + Selector);
            }
        }
        for (const FString& Tag : TagSelectors)
        {
            if (Desc.GetTags().Contains(FName(*Tag)))
            {
                MatchedTags.Add(Tag);
                Reasons.Add(Pair.Key, TEXT("Explicit tag: ") + Tag);
            }
        }
    }
    if (TerrainCount == 0 || PCGCount == 0 || RVTCount != 1)
    {
        UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Selection requires terrain, at least one PCG match, and exactly one RVT match. Terrain=%d PCG=%d RVT=%d. Use -RVTVolume for multiple RVTs."), TerrainCount, PCGCount, RVTCount);
        return false;
    }
    for (const FString& Selector : ActorSelectors)
    {
        if (!MatchedActors.Contains(Selector))
        {
            UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("IncludeActors selector did not match: %s"), *Selector);
            return false;
        }
    }
    for (const FString& Tag : TagSelectors)
    {
        if (!MatchedTags.Contains(Tag))
        {
            UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("IncludeActorTags selector did not match: %s"), *Tag);
            return false;
        }
    }

    // Close over persisted actor references before loading. Dynamic PCG queries need explicit selectors.
    TArray<FGuid> Pending;
    Reasons.GenerateKeyArray(Pending);
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        const FWorldPartitionActorDescInstance& Desc = *Descriptions.FindChecked(Pending[Index]);
        TArray<FGuid> References = Desc.GetReferences();
        References.Append(Desc.GetEditorOnlyReferences());
        if (Desc.GetParentActor().IsValid()) { References.AddUnique(Desc.GetParentActor()); }
        for (const FGuid& Reference : References)
        {
            if (!Reasons.Contains(Reference))
            {
                if (!Descriptions.Contains(Reference))
                {
                    UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Required actor reference %s of %s is missing or belongs to another container. Export its inputs into the main map first."), *Reference.ToString(), *Desc.GetActorLabelOrName().ToString());
                    return false;
                }
                Reasons.Add(Reference, TEXT("Reference of ") + Desc.GetActorLabelOrName().ToString());
                Pending.Add(Reference);
            }
        }
    }

    Pending.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
    TArray<TSharedPtr<FJsonValue>> ManifestActors;
    int64 SelectedBytes = 0;
    for (const FGuid& Guid : Pending)
    {
        const FWorldPartitionActorDescInstance& Desc = *Descriptions.FindChecked(Guid);
        if (Desc.IsChildContainerInstance() || !Desc.GetExternalDataLayerAsset().IsNull())
        {
            UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Selected actor %s is a Level Instance/container or uses an external data layer. This exporter supports main-map actors only."), *Desc.GetActorLabelOrName().ToString());
            return false;
        }
        const FString ActorFile = FPackageName::LongPackageNameToFilename(Desc.GetActorPackage().ToString(), FPackageName::GetAssetPackageExtension());
        const int64 Bytes = FMath::Max<int64>(IFileManager::Get().FileSize(*ActorFile), 0);
        SelectedBytes += Bytes;
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("guid"), Guid.ToString());
        Entry->SetStringField(TEXT("name"), Desc.GetActorName().ToString());
        Entry->SetStringField(TEXT("label"), Desc.GetActorLabel().ToString());
        Entry->SetStringField(TEXT("class"), Desc.GetBaseClass().IsNull()
            ? Desc.GetActorNativeClass()->GetPathName() : Desc.GetBaseClass().ToString());
        Entry->SetStringField(TEXT("package"), Desc.GetActorPackage().ToString());
        Entry->SetStringField(TEXT("reason"), Reasons.FindChecked(Guid));
        Entry->SetNumberField(TEXT("bytes"), Bytes);
        ManifestActors.Add(MakeShared<FJsonValueObject>(Entry));
        UE_LOG(LogWorldPartitionRVTBakeMap, Display, TEXT("Keep '%s' (%s): %s"), *Desc.GetActorLabelOrName().ToString(), *Desc.GetActorName().ToString(), *Reasons.FindChecked(Guid));
    }
    TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("source"), SourcePackageName);
    Manifest->SetStringField(TEXT("destination"), NewPackageName);
    Manifest->SetBoolField(TEXT("reportOnly"), bReportOnly);
    Manifest->SetNumberField(TEXT("sourceActorCount"), Descriptions.Num());
    Manifest->SetNumberField(TEXT("selectedActorCount"), Pending.Num());
    Manifest->SetNumberField(TEXT("selectedPackageBytes"), SelectedBytes);
    Manifest->SetArrayField(TEXT("actors"), ManifestActors);
    FString ManifestText;
    FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&ManifestText));
    const FString ManifestPath = FPaths::ProjectSavedDir() / TEXT("RVTBakeExport") / (FPackageName::GetLongPackageAssetName(NewPackageName) + TEXT(".json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);
    if (!FFileHelper::SaveStringToFile(ManifestText, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { return false; }
    UE_LOG(LogWorldPartitionRVTBakeMap, Display, TEXT("Selected %d/%d actors (%.1f MiB source packages). Manifest: %s"), Pending.Num(), Descriptions.Num(), SelectedBytes / (1024.0 * 1024.0), *ManifestPath);
    if (bReportOnly) { return true; }

    // Validate explicit PCG matches before creating any destination packages.
    TArray<FWorldPartitionReference> PCGReferences;
    for (const FGuid& Guid : Pending)
    {
        if (MatchesActor(*Descriptions.FindChecked(Guid), PCGActorName))
        {
            FWorldPartitionReference& Ref = PCGReferences.Emplace_GetRef(Partition, Guid);
            TArray<UPCGComponent*> Components;
            if (Ref.IsValid()) { Ref.GetActor()->GetComponents(Components); }
            if (Components.IsEmpty())
            {
                UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Selected PCG actor has no PCG component: %s"), *Descriptions.FindChecked(Guid)->GetActorName().ToString());
                return false;
            }
        }
    }

    UPackage* NewPackage = CreatePackage(*NewPackageName);
    if (!NewPackage) { return false; }
    FObjectDuplicationParameters DuplicationParameters(World, NewPackage);
    DuplicationParameters.DuplicateMode = EDuplicateMode::World;
    DuplicationParameters.DestName = FName(*FPackageName::GetLongPackageAssetName(NewPackageName));
    TMap<UObject*, UObject*> CreatedObjects;
    DuplicationParameters.CreatedObjects = &CreatedObjects;
    TMap<TObjectPtr<UObject>, TObjectPtr<UObject>> ExcludedObjects;
    // A world can already contain non-spatial actors before RunInternal. Seed these to themselves
    // so duplication does not copy them (or run their construction), then null target references.
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (Actor && Descriptions.Contains(Actor->GetActorGuid()) && !Reasons.Contains(Actor->GetActorGuid()))
        {
            DuplicationParameters.DuplicationSeed.Add(Actor, Actor);
            ExcludedObjects.Add(Actor, nullptr);
            ForEachObjectWithOuter(Actor, [&DuplicationParameters, &ExcludedObjects](UObject* Object)
            {
                DuplicationParameters.DuplicationSeed.Add(Object, Object);
                ExcludedObjects.Add(Object, nullptr);
            }, EGetObjectsFlags::IncludeNestedObjects);
        }
    }
    DuplicatedObjects.Empty();
    ON_SCOPE_EXIT { DuplicatedObjects.Empty(); };
    UWorld* NewWorld = Cast<UWorld>(StaticDuplicateObjectEx(DuplicationParameters));
    if (!NewWorld) { return false; }
    NewWorld->PersistentLevel->Actors.RemoveAll([NewWorld](const TObjectPtr<AActor>& Actor)
    {
        return Actor && Actor->GetWorld() != NewWorld;
    });
    FReplaceBakeMapReferences RemoveExcluded(NewWorld, ExcludedObjects);
    // Snapshot root-owned packages before relocation. Restored actors leave empty temporary
    // destination packages, which must never be saved over the already written actor data.
    TArray<UPackage*> FinalPackages = NewPackage->GetExternalPackages();
    FinalPackages.AddUnique(NewPackage);
    TMap<FGuid, FGuid> DuplicatedActorGuids;
    for (const auto& Pair : CreatedObjects)
    {
        DuplicatedObjects.Add(Pair.Key, Pair.Value);
        // UE marks the SVT Builder NonPIEDuplicateTransient, but this map must retain
        // its bake configuration. Restore this shared asset explicitly after duplication.
        if (const URuntimeVirtualTextureComponent* SourceComponent = Cast<URuntimeVirtualTextureComponent>(Pair.Key))
        {
            CastChecked<URuntimeVirtualTextureComponent>(Pair.Value)->SetStreamingTexture(SourceComponent->GetStreamingTexture());
        }
        if (const AActor* SourceActor = Cast<AActor>(Pair.Key))
        {
            if (const AActor* TargetActor = Cast<AActor>(Pair.Value))
            {
                DuplicatedActorGuids.Add(SourceActor->GetActorGuid(), TargetActor->GetActorGuid());
            }
        }
    }

    UWorld::InitializationValues IVS;
    IVS.RequiresHitProxies(false).ShouldSimulatePhysics(false).EnableTraceCollision(false)
        .CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false).CreatePhysicsScene(true);
    FScopedEditorWorld DestinationWorldScope(NewWorld, IVS);
    FSoftObjectPathFixupArchive SoftPathFixup(SourcePackageName + TEXT(".") + World->GetName(),
        NewPackageName + TEXT(".") + NewWorld->GetName());
    SoftPathFixup.Fixup(NewWorld);

    TArray<TPair<FGuid, TArray<FGuid>>> ActorsWithReferences;
    for (const FGuid& Guid : Pending)
    {
        const FWorldPartitionActorDescInstance& Desc = *Descriptions.FindChecked(Guid);
        TArray<FGuid> References = Desc.GetReferences();
        References.Append(Desc.GetEditorOnlyReferences());
        if (Desc.GetParentActor().IsValid()) { References.AddUnique(Desc.GetParentActor()); }
        References.RemoveAll([&Reasons](const FGuid& Reference) { return !Reasons.Contains(Reference); });
        ActorsWithReferences.Emplace(Guid, MoveTemp(References));
    }
    const TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ActorsWithReferences);
    for (const TArray<FGuid>& Cluster : Clusters)
    {
        ON_SCOPE_EXIT
        {
            if (FWorldPartitionHelpers::ShouldCollectGarbage())
            {
                FWorldPartitionHelpers::DoCollectGarbage();
            }
        };
        TArray<FWorldPartitionReference> ActorReferences;
        for (const FGuid& Guid : Cluster)
        {
            if (!DuplicatedActorGuids.Contains(Guid))
            {
                FWorldPartitionReference& Ref = ActorReferences.Emplace_GetRef(Partition, Guid);
                if (!Ref.IsValid() || !Ref.GetActor())
                {
                    UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Failed to load selected actor: %s"), *Guid.ToString());
                    return false;
                }
            }
        }
        if (ActorReferences.IsEmpty()) { continue; }

        // Relocate complete external packages, not text-exported actors. This preserves terrain
        // components, edit layers and textures. Always restore the source objects, even on errors.
        struct FRelocatedActor
        {
            AActor* Actor;
            UPackage* OriginalPackage;
            TArray<UObject*> Dependants;
        };
        TArray<FRelocatedActor> RelocatedActors;
        ON_SCOPE_EXIT
        {
            for (FRelocatedActor& Entry : RelocatedActors)
            {
                Entry.Actor->Rename(nullptr, World->PersistentLevel,
                    REN_NonTransactional | REN_DontCreateRedirectors | REN_DoNotDirty | REN_AllowPackageLinkerMismatch);
                for (UObject* Object : Entry.Dependants)
                {
                    Object->Rename(nullptr, Entry.OriginalPackage,
                        REN_NonTransactional | REN_DontCreateRedirectors | REN_DoNotDirty | REN_AllowPackageLinkerMismatch);
                }
            }
        };
        TArray<UPackage*> PackagesToSave;
        for (const FWorldPartitionReference& Ref : ActorReferences)
        {
            AActor* Actor = Ref.GetActor();
            UPackage* OriginalPackage = Actor->GetExternalPackage();
            if (!OriginalPackage)
            {
                UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Selected actor has no external package: %s"), *Actor->GetPathName());
                return false;
            }
            FRelocatedActor& Entry = RelocatedActors.Emplace_GetRef();
            Entry.Actor = Actor;
            Entry.OriginalPackage = OriginalPackage;
            Actor->Rename(nullptr, NewWorld->PersistentLevel,
                REN_NonTransactional | REN_DontCreateRedirectors | REN_DoNotDirty | REN_AllowPackageLinkerMismatch);
            ForEachObjectWithPackage(OriginalPackage, [&Entry](UObject* Object)
            {
                Entry.Dependants.Add(Object);
                return true;
            }, EGetObjectsFlags::None);
            for (UObject* Object : Entry.Dependants)
            {
                Object->Rename(nullptr, Actor->GetExternalPackage(),
                    REN_NonTransactional | REN_DontCreateRedirectors | REN_DoNotDirty | REN_AllowPackageLinkerMismatch);
            }
            SoftPathFixup.Fixup(Actor);
            FReplaceBakeMapReferences FixReferences(Actor, DuplicatedObjects);
            PackagesToSave.AddUnique(Actor->GetPackage());
        }
        UE_LOG(LogWorldPartitionRVTBakeMap, Display, TEXT("Saving selected actor cluster: %d packages"), PackagesToSave.Num());
        if (!SavePackages(PackagesToSave, PackageHelper)) { return false; }
        // Scope restores all source outers before the next cluster is loaded.
    }

    // Do not save any source external package referenced through a shared object.
    const TArray<FString> SourceExternalPaths = ULevel::GetExternalObjectsPaths(SourcePackageName);
    for (UPackage* Package : FinalPackages)
    {
        for (const FString& SourceExternalPath : SourceExternalPaths)
        {
            if (Package->GetName().StartsWith(SourceExternalPath + TEXT("/")))
            {
                UE_LOG(LogWorldPartitionRVTBakeMap, Error, TEXT("Unexpected source package in destination: %s"), *Package->GetName());
                return false;
            }
        }
    }
    if (!SavePackages(FinalPackages, PackageHelper)) { return false; }
    Manifest->SetBoolField(TEXT("exportSucceeded"), true);
    ManifestText.Reset();
    FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&ManifestText));
    FFileHelper::SaveStringToFile(ManifestText, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogWorldPartitionRVTBakeMap, Display, TEXT("Exported %d selected actors to %s. Terrain coordinates and shared RVT/SVT asset references are preserved."), Pending.Num(), *NewPackageName);
    return true;
}
