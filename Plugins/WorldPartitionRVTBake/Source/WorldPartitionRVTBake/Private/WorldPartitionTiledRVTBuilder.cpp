// Tile rendering follows UE 5.8 RuntimeVirtualTextureBuildStreamingMips.cpp.
#include "WorldPartitionTiledRVTBuilder.h"

#include "AssetCompilingManager.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "ComponentRecreateRenderStateContext.h"
#include "ContentStreaming.h"
#include "Engine/World.h"
#include "EngineModule.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGHelpers.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RendererInterface.h"
#include "RenderTargetPool.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "VirtualTexturingEditorModule.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureRender.h"
#include "VT/VirtualTextureBuilder.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionTiledRVTBuilder)

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionRVTBake, Log, All);

namespace
{
class FTileRenderResources final : public FRenderResource
{
public:
    FTileRenderResources(int32 InTileSize, int32 InTilesX, int32 InTilesY, TArrayView<const EPixelFormat> InFormats)
        : TileSize(InTileSize)
    {
        for (EPixelFormat Format : InFormats)
        {
            const EPixelFormat CaptureFormat = (Format == PF_G16 || Format == PF_BC4) ? PF_G16 : PF_B8G8R8A8;
            LayerOffsets.Add(TotalBytes);
            LayerFormats.Add(CaptureFormat);
            TotalBytes += CalculateImageBytes(TileSize, TileSize, 0, CaptureFormat) * InTilesX * InTilesY;
        }
    }

    virtual void InitRHI(FRHICommandListBase& RHICmdList) override
    {
        RenderTargets.Init(nullptr, LayerFormats.Num());
        StagingTextures.Init(nullptr, LayerFormats.Num());
        for (int32 Layer = 0; Layer < LayerFormats.Num(); ++Layer)
        {
            const FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
                FIntPoint(TileSize, TileSize), LayerFormats[Layer], FClearValueBinding::None,
                TexCreate_None, TexCreate_ShaderResource, false);
            GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RenderTargets[Layer], TEXT("TiledRVT"));

            FRHITextureCreateDesc StagingDesc = FRHITextureCreateDesc::Create2D(TEXT("TiledRVTReadback"), TileSize, TileSize, LayerFormats[Layer]);
            StagingDesc.SetFlags(ETextureCreateFlags::CPUReadback);
            StagingTextures[Layer] = RHICmdList.CreateTexture(StagingDesc);
        }
        Fence = RHICreateGPUFence(TEXT("Tiled RVT Build"));
    }

    virtual void ReleaseRHI() override
    {
        RenderTargets.Empty();
        StagingTextures.Empty();
        Fence.SafeRelease();
    }

    int64 GetTotalBytes() const { return TotalBytes; }
    int64 GetLayerOffset(int32 Layer) const { return LayerOffsets[Layer]; }
    EPixelFormat GetLayerFormat(int32 Layer) const { return LayerFormats[Layer]; }
    TRefCountPtr<IPooledRenderTarget> GetRenderTarget(int32 Layer) const { return RenderTargets[Layer]; }
    FRHITexture* GetStagingTexture(int32 Layer) const { return StagingTextures[Layer]; }
    FRHIGPUFence* GetFence() const { return Fence; }

private:
    int32 TileSize;
    int64 TotalBytes = 0;
    TArray<int64> LayerOffsets;
    TArray<EPixelFormat> LayerFormats;
    TArray<TRefCountPtr<IPooledRenderTarget>> RenderTargets;
    TArray<FTextureRHIRef> StagingTextures;
    FGPUFenceRHIRef Fence;
};

FBox TileWorldBounds(const FTransform& Transform, const FBox& RVTBounds, const FBox2D& UVRange, float Overlap)
{
    FBox Bounds(ForceInit);
    for (const double U : { UVRange.Min.X, UVRange.Max.X })
    {
        for (const double V : { UVRange.Min.Y, UVRange.Max.Y })
        {
            Bounds += Transform.TransformPosition(FVector(U, V, 0.5));
        }
    }
    Bounds.Min.Z = RVTBounds.Min.Z;
    Bounds.Max.Z = RVTBounds.Max.Z;
    return Bounds.ExpandBy(FVector(Overlap, Overlap, 0));
}

void CopyTile(void* Source, int32 SourceStride, uint8* Destination, int32 ImageWidth, int32 TileSize,
    int32 TileX, int32 TileY, EPixelFormat Format)
{
    const int32 BytesPerPixel = Format == PF_G16 ? sizeof(uint16) : sizeof(FColor);
    for (int32 Y = 0; Y < TileSize; ++Y)
    {
        uint8* TargetRow = Destination + ((TileY * TileSize + Y) * ImageWidth + TileX * TileSize) * BytesPerPixel;
        const uint8* SourceRow = static_cast<const uint8*>(Source) + Y * SourceStride * BytesPerPixel;
        FMemory::Memcpy(TargetRow, SourceRow, TileSize * BytesPerPixel);
    }
}

bool WaitForPCG(UWorld* World, float MaxWaitSeconds, const TCHAR* Phase)
{
    FWorldPartitionHelpers::FakeEngineTick(World);
    if (UPCGSubsystem* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>())
    {
        const double StartTime = FPlatformTime::Seconds();
        double LastProgressTime = StartTime;
        while (PCGSubsystem->IsAnyGraphCurrentlyExecuting())
        {
            FWorldPartitionHelpers::FakeEngineTick(World);
            const double Now = FPlatformTime::Seconds();
            const double Elapsed = Now - StartTime;
            if (Now - LastProgressTime >= 10.0)
            {
                UE_LOG(LogWorldPartitionRVTBake, Display,
                    TEXT("Waiting for PCG during %s (%.0f seconds elapsed)."), Phase, Elapsed);
                LastProgressTime = Now;
            }
            if (MaxWaitSeconds > 0.f && Elapsed >= MaxWaitSeconds)
            {
                UE_LOG(LogWorldPartitionRVTBake, Error,
                    TEXT("PCG did not finish during %s within %.0f seconds."), Phase, MaxWaitSeconds);
                return false;
            }
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    // PCG can finish its graph task before the generated ISM/HISM components
    // have submitted their render state. Complete the normal end-of-frame
    // update before the RVT render command is enqueued.
    World->SendAllEndOfFrameUpdates();
    return true;
}

bool WaitForPCGComponents(UWorld* World, TArrayView<const TWeakObjectPtr<UPCGComponent>> Components,
    float MaxWaitSeconds, const TCHAR* Phase)
{
    FWorldPartitionHelpers::FakeEngineTick(World);
    const double StartTime = FPlatformTime::Seconds();
    double LastProgressTime = StartTime;

    auto HasGeneratingComponent = [&Components]()
    {
        for (const TWeakObjectPtr<UPCGComponent>& Component : Components)
        {
            if (Component.IsValid() && Component->IsGenerating())
            {
                return true;
            }
        }
        return false;
    };

    while (HasGeneratingComponent())
    {
        FWorldPartitionHelpers::FakeEngineTick(World);
        const double Now = FPlatformTime::Seconds();
        const double Elapsed = Now - StartTime;
        if (Now - LastProgressTime >= 10.0)
        {
            UE_LOG(LogWorldPartitionRVTBake, Display,
                TEXT("Waiting for %d tile-local PCG component(s) during %s (%.0f seconds elapsed)."),
                Components.Num(), Phase, Elapsed);
            LastProgressTime = Now;
        }
        if (MaxWaitSeconds > 0.f && Elapsed >= MaxWaitSeconds)
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("Tile-local PCG did not finish during %s within %.0f seconds."), Phase, MaxWaitSeconds);
            return false;
        }
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    World->SendAllEndOfFrameUpdates();
    return true;
}

bool DoesGraphMatch(const UPCGGraph* Graph, const FString& RequestedName)
{
    if (!Graph || RequestedName.IsEmpty())
    {
        return false;
    }

    const FString AssetName = Graph->GetName();
    const FString ObjectPath = Graph->GetPathName();
    const FString PackagePath = Graph->GetOutermost()->GetName();
    return RequestedName.Equals(AssetName, ESearchCase::IgnoreCase) ||
        RequestedName.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
        RequestedName.Equals(PackagePath, ESearchCase::IgnoreCase);
}

bool DoesActorMatch(const AActor* Actor, const FString& RequestedName)
{
    if (!Actor || RequestedName.IsEmpty())
    {
        return false;
    }

    const FString ClassName = Actor->GetClass()->GetName();
    return Actor->GetName().Equals(RequestedName, ESearchCase::IgnoreCase) ||
        Actor->GetActorNameOrLabel().Equals(RequestedName, ESearchCase::IgnoreCase) ||
        ClassName.Equals(RequestedName, ESearchCase::IgnoreCase) ||
        ClassName.Equals(RequestedName + TEXT("_C"), ESearchCase::IgnoreCase);
}

bool GeneratePCGForTile(UWorld* World, const FBox& Bounds, const FString& GraphName,
    const FString& PCGActorName, const FString& PCGComponentName,
    float MaxWaitSeconds, int32& GeneratedCells, int32& SkippedCells)
{
    UPCGSubsystem* Subsystem = World->GetSubsystem<UPCGSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error, TEXT("PCG subsystem is unavailable."));
        return false;
    }

    TArray<UPCGComponent*> OriginalComponents;
    for (TObjectIterator<UPCGComponent> It; It; ++It)
    {
        UPCGComponent* Original = *It;
        if (Original->HasAnyFlags(RF_ClassDefaultObject) || Original->GetWorld() != World ||
            !Original->IsRegistered() || Original->IsLocalComponent() ||
            !Original->GetGraph())
        {
            continue;
        }

        // Log every candidate before filtering so that a mistyped -PCGActor / -PCGGraph /
        // -PCGComponent, or a component that is still runtime-managed, is diagnosable
        // instead of silently producing no cells.
        UE_LOG(LogWorldPartitionRVTBake, Display,
            TEXT("PCG candidate: actor '%s' (%s) component '%s' graph '%s' trigger=%d editingMode=%d runtimeManaged=%d"),
            *Original->GetOwner()->GetName(), *Original->GetOwner()->GetClass()->GetName(),
            *Original->GetName(), *Original->GetGraph()->GetPathName(),
            static_cast<int32>(Original->GenerationTrigger),
            static_cast<int32>(Original->GetSerializedEditingMode()),
            Original->IsManagedByRuntimeGenSystem() ? 1 : 0);

        if (!PCGActorName.IsEmpty() && !DoesActorMatch(Original->GetOwner(), PCGActorName))
        {
            continue;
        }
        if (!PCGComponentName.IsEmpty() &&
            !Original->GetName().Equals(PCGComponentName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (!GraphName.IsEmpty() && !DoesGraphMatch(Original->GetGraph(), GraphName))
        {
            continue;
        }
        if (Original->IsManagedByRuntimeGenSystem())
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("PCG component %s on actor %s is still set to GenerateAtRuntime. Change it to GenerateOnLoad or GenerateOnDemand before baking."),
                *Original->GetName(), *Original->GetOwner()->GetName());
            return false;
        }
        OriginalComponents.Add(Original);
    }

    for (UPCGComponent* Original : OriginalComponents)
    {
        const FString ActualGraphName = Original->GetGraph()->GetPathName();
        const FBox GenerationBounds = Original->GetGridBounds().Overlap(Bounds);
        if (!GenerationBounds.IsValid)
        {
            continue;
        }

        // A PCG component placed directly in the world is not partitioned and
        // has no local partition actors. Its GenerateOnLoad result is created
        // when the tile loads, so wait for that component and capture it in the
        // same tile. Regenerate only when there is no usable saved result or when
        // procedural instances require a fresh generation.
        if (!Original->IsPartitioned())
        {
            TArray<TWeakObjectPtr<UPCGComponent>> ScheduledComponents;
            if (Original->IsGenerating())
            {
                ScheduledComponents.Add(Original);
            }
            else if (!Original->bGenerated || Original->AreProceduralInstancesInUse())
            {
                const FPCGTaskId TaskId = Original->GenerateLocalGetTaskId(
                    EPCGComponentGenerationTrigger::GenerateOnDemand,
                    /*bForce=*/true);
                if (TaskId == InvalidPCGTaskId)
                {
                    UE_LOG(LogWorldPartitionRVTBake, Error,
                        TEXT("Failed to schedule non-partitioned PCG actor %s component %s graph %s."),
                        *Original->GetOwner()->GetName(), *Original->GetName(), *ActualGraphName);
                    return false;
                }
                ScheduledComponents.Add(Original);
            }

            ++GeneratedCells;
            UE_LOG(LogWorldPartitionRVTBake, Display,
                TEXT("Using scene PCG actor %s component %s graph %s (non-partitioned)."),
                *Original->GetOwner()->GetName(), *Original->GetName(), *ActualGraphName);
            if (!WaitForPCGComponents(World, ScheduledComponents, MaxWaitSeconds, TEXT("scene PCG generation")))
            {
                return false;
            }
            continue;
        }

        PCGHiGenGrid::FSizeArray GridSizes;
        bool bHasUnbounded = false;
        if (!PCGHelpers::GetGenerationGridSizes(Original->GetGraph(), Subsystem->GetPCGWorldActor(), GridSizes, bHasUnbounded) ||
            bHasUnbounded || GridSizes.IsEmpty())
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("PCG graph %s needs bounded partition grids for tiled baking."), *ActualGraphName);
            return false;
        }

        // Generate larger grids first so hierarchical child grids can consume their results.
        GridSizes.Sort(TGreater<uint32>());
        for (uint32 GridSize : GridSizes)
        {
            PCGHiGenGrid::FSizeArray SingleGrid;
            SingleGrid.Add(GridSize);
            bool bScheduleFailure = false;
            int32 FoundCells = 0;
            int32 GridSkippedCells = 0;
            TArray<TWeakObjectPtr<UPCGComponent>> ScheduledComponents;

            // Pass bCanCreateActor=true so that a cell with no partition actor on disk is
            // still created. With false, UPCGSubsystem::ForAllOverlappingCells silently
            // skips those cells (its InUnloadedFunc only fires when an actor record exists
            // but is not loaded), which would bake grass coverage gaps into the SVT with
            // no diagnostic at all.
            Subsystem->ForAllOverlappingCells(Original, GenerationBounds, SingleGrid, /*bCanCreateActor=*/true, {},
                [Original, GridSize, &FoundCells, &ScheduledComponents, &bScheduleFailure](APCGPartitionActor* PartitionActor, const FBox&)
                {
                    PartitionActor->AddGraphInstance(Original);
                    if (UPCGComponent* Local = PartitionActor->GetLocalComponent(Original))
                    {
                        ++FoundCells;
                        ScheduledComponents.Add(Local);

                        // Schedule only this partition actor's local component. Calling
                        // GenerateLocalGetTaskId on the original component dispatches to
                        // every registered local component, including cells outside this
                        // bake tile, and can leave the commandlet waiting indefinitely.
                        if (Local->IsGenerating())
                        {
                            return Local->GetGenerationTaskId();
                        }

                        // bForce is required: for a partition actor saved in Normal edit
                        // mode, the local component loads back with bGenerated=true and a
                        // transient bDirtyGenerated that PostLoad clears, so a non-forced
                        // request is a no-op and nothing would be regenerated.
                        const FPCGTaskId TaskId = Local->GenerateLocalGetTaskId(
                            EPCGComponentGenerationTrigger::GenerateOnDemand,
                            /*bForce=*/true,
                            GridSize);
                        if (TaskId == InvalidPCGTaskId)
                        {
                            bScheduleFailure = true;
                        }
                        return TaskId;
                    }
                    return InvalidPCGTaskId;
                },
                [GridSize, &GridSkippedCells](const FPCGGridDescriptor&, const FIntVector& CellCoords, const FBox& CellBounds)
                {
                    ++GridSkippedCells;
                    if (GridSkippedCells <= 5)
                    {
                        UE_LOG(LogWorldPartitionRVTBake, Warning,
                            TEXT("PCG grid %u cell (%d, %d, %d) has an actor record but is not loaded (bounds %s..%s)."),
                            GridSize, CellCoords.X, CellCoords.Y, CellCoords.Z,
                            *CellBounds.Min.ToString(), *CellBounds.Max.ToString());
                    }
                    return InvalidPCGTaskId;
                });
            if (bScheduleFailure)
            {
                UE_LOG(LogWorldPartitionRVTBake, Error,
                    TEXT("Failed to schedule one or more local PCG components for graph %s on actor %s at grid %u. "
                         "The graph or component did not accept generation."),
                    *ActualGraphName, *Original->GetOwner()->GetName(), GridSize);
                return false;
            }
            if (GridSkippedCells > 0)
            {
                UE_LOG(LogWorldPartitionRVTBake, Warning,
                    TEXT("%d PCG cell(s) on grid %u for graph %s have a saved actor record that is not loaded in this tile. "
                         "Run PCGWorldPartitionBuilder -IterativeCellLoading -GenerateComponentEditingModeNormal=true over the full map and save it."),
                    GridSkippedCells, GridSize, *ActualGraphName);
                SkippedCells += GridSkippedCells;
            }
            if (FoundCells == 0)
            {
                // A tile can legitimately contain no PCG cells (the RVT volume is usually
                // larger than the generation volume). Record it and keep baking; the
                // whole-run check in RunInternal catches a genuinely empty graph.
                UE_LOG(LogWorldPartitionRVTBake, Display,
                    TEXT("No PCG cells for graph %s on grid %u inside this tile; nothing to regenerate here."),
                    *ActualGraphName, GridSize);
                continue;
            }
            GeneratedCells += FoundCells;
            UE_LOG(LogWorldPartitionRVTBake, Display,
                TEXT("Scheduled PCG actor %s component %s graph %s for grid %u across %d loaded cells (local-only)."),
                *Original->GetOwner()->GetName(), *Original->GetName(),
                *ActualGraphName, GridSize, FoundCells);
            if (!WaitForPCGComponents(World, ScheduledComponents, MaxWaitSeconds, TEXT("PCG tile generation")))
            {
                return false;
            }
        }
    }
    return true;
}
}

bool UWorldPartitionTiledRVTBuilder::RunInternal(UWorld* World, const FCellInfo& CellInfo,
    FPackageSourceControlHelper& PackageHelper)
{
    IVirtualTexturingEditorModule& VTModule = FModuleManager::LoadModuleChecked<IVirtualTexturingEditorModule>(TEXT("VirtualTexturingEditor"));
    FString SelectedVolume;
    GetParamValue(TEXT("RVTVolume="), SelectedVolume);
    FString PCGGraphName;
    GetParamValue(TEXT("PCGGraph="), PCGGraphName);
    FString PCGActorName;
    GetParamValue(TEXT("PCGActor="), PCGActorName);
    FString PCGComponentName;
    GetParamValue(TEXT("PCGComponent="), PCGComponentName);
    const bool bGeneratePCG = !PCGActorName.IsEmpty() || !PCGGraphName.IsEmpty();
    const bool bUsePrebuiltPCG = HasParam(TEXT("UsePrebuiltPCG"));
    if (bUsePrebuiltPCG && bGeneratePCG)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error,
            TEXT("-UsePrebuiltPCG cannot be combined with -PCGActor, -PCGComponent, or -PCGGraph."));
        return false;
    }
    if (!PCGComponentName.IsEmpty() && !bGeneratePCG)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error,
            TEXT("-PCGComponent requires -PCGActor or -PCGGraph."));
        return false;
    }

    URuntimeVirtualTextureComponent* Component = nullptr;
    for (URuntimeVirtualTextureComponent* Candidate : VTModule.GatherRuntimeVirtualTextureComponents(World))
    {
        const AActor* Owner = Candidate->GetOwner();
        if (!Owner)
        {
            continue;
        }
        const bool bHasStreamedMips = VTModule.HasStreamedMips(EShadingPath::Deferred, Candidate);
        UE_LOG(LogWorldPartitionRVTBake, Display,
            TEXT("RVT candidate: actor='%s' label='%s' spatial=%d RVT='%s' SVT='%s' streamedMips=%d eligible=%d"),
            *Owner->GetName(), *Owner->GetActorNameOrLabel(), Owner->GetIsSpatiallyLoaded() ? 1 : 0,
            *GetPathNameSafe(Candidate->GetVirtualTexture()),
            *GetPathNameSafe(Candidate->GetStreamingTexture()),
            Candidate->NumStreamingMips(), bHasStreamedMips ? 1 : 0);
        if (!SelectedVolume.IsEmpty() &&
            !Owner->GetName().Equals(SelectedVolume, ESearchCase::IgnoreCase) &&
            !Owner->GetActorNameOrLabel().Equals(SelectedVolume, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (SelectedVolume.IsEmpty() && !bHasStreamedMips)
        {
            continue;
        }
        if (Component)
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("Multiple RVT volumes match '%s': '%s' and '%s'. Pass a unique internal actor name to -RVTVolume."),
                *SelectedVolume, *Component->GetOwner()->GetName(), *Owner->GetName());
            return false;
        }
        Component = Candidate;
    }
    if (!Component)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error,
            TEXT("No registered RVT volume matches '%s'. Check the actor or label in the RVT candidate list above."),
            *SelectedVolume);
        return false;
    }
    if (Component->GetOwner()->GetIsSpatiallyLoaded() ||
        !VTModule.HasStreamedMips(EShadingPath::Deferred, Component))
    {
        UE_LOG(LogWorldPartitionRVTBake, Error,
            TEXT("Selected RVT volume '%s' is not eligible: spatial=%d RVT=%d SVT=%d streamedMips=%d. "
                 "Disable Is Spatially Loaded and assign an RVT, streaming texture, and at least one streaming mip."),
            *Component->GetOwner()->GetName(), Component->GetOwner()->GetIsSpatiallyLoaded() ? 1 : 0,
            Component->GetVirtualTexture() ? 1 : 0, Component->GetStreamingTexture() ? 1 : 0,
            Component->NumStreamingMips());
        return false;
    }
    if (Component->GetStreamingTexture()->bSeparateTextureForMobile)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error, TEXT("Separate mobile streaming mips are not supported by this builder."));
        return false;
    }

    URuntimeVirtualTexture* RVT = Component->GetVirtualTexture();
    FSceneInterface* Scene = Component->GetScene();
    if (!Scene || Scene->GetShadingPath() != EShadingPath::Deferred)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error, TEXT("This builder currently requires the deferred shading path."));
        return false;
    }

    float TileOverlap = 0.f;
    GetParamValue(TEXT("TileOverlap="), TileOverlap);
    TileOverlap = FMath::Max(TileOverlap, 0.f);
    int32 MaxBufferMiB = 4096;
    GetParamValue(TEXT("MaxBufferMiB="), MaxBufferMiB);
    float PCGWaitSeconds = 1800.f;
    GetParamValue(TEXT("PCGWaitSeconds="), PCGWaitSeconds);
    PCGWaitSeconds = FMath::Max(PCGWaitSeconds, 0.f);

    const FTransform Transform = Component->GetComponentTransform();
    const FBox RVTBounds = Component->Bounds.GetBox();
    const FVector4f CustomMaterialData = Component->GetCustomMaterialData();
    const FLinearColor FixedColor = Component->GetStreamingMipsFixedColor();
    const int32 RVTId = RVT->GetUniqueID();
    const ERuntimeVirtualTextureMaterialType MaterialType = RVT->GetMaterialType();

    FVTProducerDescription VTDesc;
    RVT->GetProducerDescription(VTDesc, URuntimeVirtualTexture::FInitSettings(), Transform);
    const int32 TileSize = VTDesc.TileSize;
    const int32 TextureWidth = VTDesc.WidthInBlocks * VTDesc.BlockWidthInTiles * TileSize;
    const int32 TextureHeight = VTDesc.HeightInBlocks * VTDesc.BlockHeightInTiles * TileSize;
    const int32 MaxLevel = FMath::CeilLogTwo(FMath::Max(VTDesc.BlockWidthInTiles, VTDesc.BlockHeightInTiles));
    const int32 RenderLevel = FMath::Max(MaxLevel - Component->NumStreamingMips() + 1, 0);
    const int32 ImageWidth = FMath::Max(TileSize, TextureWidth >> RenderLevel);
    const int32 ImageHeight = FMath::Max(TileSize, TextureHeight >> RenderLevel);
    const int32 TilesX = ImageWidth / TileSize;
    const int32 TilesY = ImageHeight / TileSize;
    const int32 NumLayers = RVT->GetLayerCount();

    if (bUsePrebuiltPCG)
    {
        UE_LOG(LogWorldPartitionRVTBake, Display,
            TEXT("Using prebuilt PCG results. The RVT builder will not schedule or wait for PCG generation."));
    }

    TArray<EPixelFormat> LayerFormats;
    for (int32 Layer = 0; Layer < NumLayers; ++Layer)
    {
        LayerFormats.Add(RVT->GetLayerFormat(Layer));
    }
    FTileRenderResources RenderResources(TileSize, TilesX, TilesY, LayerFormats);
    UE_LOG(LogWorldPartitionRVTBake, Display, TEXT("RVT %s: %d x %d pixels, %d x %d tiles, %.1f MiB buffer"),
        *Component->GetOwner()->GetName(), ImageWidth, ImageHeight, TilesX, TilesY,
        RenderResources.GetTotalBytes() / (1024.0 * 1024.0));
    if (HasParam(TEXT("ValidateOnly")))
    {
        return true;
    }
    if (MaxBufferMiB <= 0 || RenderResources.GetTotalBytes() > static_cast<int64>(MaxBufferMiB) * 1024 * 1024)
    {
        UE_LOG(LogWorldPartitionRVTBake, Error, TEXT("SVT pixel buffer needs %.1f MiB (limit %d MiB)."),
            RenderResources.GetTotalBytes() / (1024.0 * 1024.0), MaxBufferMiB);
        return false;
    }

    // Make sure the world is streamed in and the render state reflects shader compilation
    // before the first tile is rendered, mirroring the initial flush of
    // RuntimeVirtualTextureBuildStreamingMips.cpp. Per-tile FakeEngineTick covers the
    // remaining tiles, but the first one would otherwise render with a stale scene.
    World->FlushLevelStreaming();
    FAssetCompilingManager::Get().FinishAllCompilation();
    {
        FGlobalComponentRecreateRenderStateContext Context;
    }
    FlushRenderingCommands();

    // Commandlets do not run a normal viewport frame. Give the renderer a
    // commandlet frame to submit scene updates before the first RVT page.
    FWorldPartitionHelpers::FakeEngineTick(World);
    FlushRenderingCommands();

    // The regular RVT commandlet builder renders pages without requiring the
    // GPU scene to have entered a normal viewport rendering frame. In a
    // commandlet IsRendering() can remain false even though the scene and
    // render resources are valid, so this is diagnostic only.
    if (!RuntimeVirtualTexture::IsSceneReadyToRender(Scene))
    {
        UE_LOG(LogWorldPartitionRVTBake, Warning,
            TEXT("GPU scene has not entered a normal viewport rendering frame; continuing with commandlet RVT page rendering."));
    }

    TArray64<uint8> FinalPixels;
    FinalPixels.SetNumUninitialized(RenderResources.GetTotalBytes());
    BeginInitResource(&RenderResources);
    FScopedSlowTask Task(TilesX * TilesY + 1, FText::FromString(TEXT("Building tiled RVT streaming mips")));
    Task.MakeDialog(true);
    bool bSuccess = true;
    int32 GeneratedPCGCells = 0;
    int32 SkippedPCGCells = 0;

    for (int32 TileY = 0; TileY < TilesY && bSuccess; ++TileY)
    {
        for (int32 TileX = 0; TileX < TilesX && !Task.ShouldCancel(); ++TileX)
        {
            if (Task.ShouldCancel())
            {
                bSuccess = false;
                break;
            }
            Task.EnterProgressFrame();
            const FBox2D UVRange(
                FVector2D(static_cast<double>(TileX) / TilesX, static_cast<double>(TileY) / TilesY),
                FVector2D(static_cast<double>(TileX + 1) / TilesX, static_cast<double>(TileY + 1) / TilesY));
            const FBox LoadBounds = TileWorldBounds(Transform, RVTBounds, UVRange, TileOverlap);
            const int32 TileIndex = TileY * TilesX + TileX + 1;
            UE_LOG(LogWorldPartitionRVTBake, Display,
                TEXT("Loading tile %d/%d: X=%d Y=%d Bounds=%s..%s"),
                TileIndex, TilesX * TilesY, TileX, TileY,
                *LoadBounds.Min.ToString(), *LoadBounds.Max.ToString());

            TUniquePtr<FLoaderAdapterShape> Loader = MakeUnique<FLoaderAdapterShape>(World, LoadBounds, TEXT("Tiled RVT region"));
            Loader->Load();
            World->BlockTillLevelStreamingCompleted();
            if (bGeneratePCG)
            {
                // GeneratePCGForTile discovers the requested component after the
                // tile is loaded and waits only for that component. Waiting on the
                // whole subsystem here would include unrelated GPU PCG graphs.
                if (!GeneratePCGForTile(World, LoadBounds, PCGGraphName,
                    PCGActorName, PCGComponentName, PCGWaitSeconds, GeneratedPCGCells, SkippedPCGCells))
                {
                    bSuccess = false;
                    break;
                }
            }
            else
            {
                // Prebuilt PCG output may be loaded as generated components. Allow
                // asset compilation and render-state creation to finish, but do not
                // wait for unrelated GenerateOnLoad GPU graphs.
                FWorldPartitionHelpers::FakeEngineTick(World);
                FAssetCompilingManager::Get().FinishAllCompilation();
                FAssetCompilingManager::Get().ProcessAsyncTasks();
                World->SendAllEndOfFrameUpdates();
            }

            const FVector StreamingWorldPos = Transform.TransformPosition(FVector(UVRange.GetCenter(), 0.5f));
            IStreamingManager::Get().Tick(0.f);
            IStreamingManager::Get().AddViewLocation(StreamingWorldPos);
            IStreamingManager::Get().StreamAllResources(0);

            ENQUEUE_RENDER_COMMAND(BakeTiledRVT)([
                Scene, RVTId, &RenderResources, MaterialType, NumLayers, Transform, RVTBounds, UVRange,
                RenderLevel, MaxLevel, TileX, TileY, TileSize, ImageWidth, &FinalPixels,
                CustomMaterialData, FixedColor](FRHICommandListImmediate& RHICmdList)
            {
                const FIntRect TileRect(0, 0, TileSize, TileSize);
                {
                    FRDGBuilder GraphBuilder(RHICmdList);
                    FScenePrimitiveRenderingContextScopeHelper RenderingScope(GetRendererModule().BeginScenePrimitiveRendering(GraphBuilder, *Scene));
                    RuntimeVirtualTexture::FRenderPageBatchDesc Desc;
                    Desc.SceneRenderer = RenderingScope.ScenePrimitiveRenderingContext->GetSceneRenderer();
                    Desc.RuntimeVirtualTextureId = RVTId;
                    Desc.UVToWorld = Transform;
                    Desc.WorldBounds = RVTBounds;
                    Desc.MaterialType = MaterialType;
                    Desc.MaxLevel = IntCastChecked<uint8>(MaxLevel);
                    Desc.bClearTextures = true;
                    Desc.bIsThumbnails = false;
                    Desc.FixedColor = FixedColor;
                    Desc.CustomMaterialData = CustomMaterialData;
                    Desc.NumPageDescs = 1;
                    for (int32 Layer = 0; Layer < NumLayers; ++Layer)
                    {
                        Desc.Targets[Layer].PooledRenderTarget = RenderResources.GetRenderTarget(Layer);
                        Desc.PageDescs[0].DestRect[Layer] = TileRect;
                    }
                    Desc.PageDescs[0].UVRange = UVRange;
                    Desc.PageDescs[0].vLevel = IntCastChecked<uint8>(RenderLevel);
                    RuntimeVirtualTexture::RenderPages(GraphBuilder, Desc);
                    for (int32 Layer = 0; Layer < NumLayers; ++Layer)
                    {
                        FRDGTextureRef Target = GraphBuilder.RegisterExternalTexture(RenderResources.GetRenderTarget(Layer), ERDGTextureFlags::None);
                        FRDGTextureRef Staging = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RenderResources.GetStagingTexture(Layer), TEXT("TiledRVTStaging")));
                        AddCopyTexturePass(GraphBuilder, Target, Staging, FRHICopyTextureInfo());
                    }
                    GraphBuilder.Execute();
                }

                RenderResources.GetFence()->Clear();
                RHICmdList.WriteGPUFence(RenderResources.GetFence());
                for (int32 Layer = 0; Layer < NumLayers; ++Layer)
                {
                    void* Pixels = nullptr;
                    int32 Width = 0;
                    int32 Height = 0;
                    RHICmdList.MapStagingSurface(RenderResources.GetStagingTexture(Layer), RenderResources.GetFence(), Pixels, Width, Height);
                    check(Pixels && Height == TileSize);
                    CopyTile(Pixels, Width, FinalPixels.GetData() + RenderResources.GetLayerOffset(Layer),
                        ImageWidth, TileSize, TileX, TileY, RenderResources.GetLayerFormat(Layer));
                    RHICmdList.UnmapStagingSurface(RenderResources.GetStagingTexture(Layer));
                }
            });

            FlushRenderingCommands();
            Loader.Reset();
            FWorldPartitionHelpers::DoCollectGarbage();
            // No explicit view-location cleanup is needed: AddViewLocation uses a zero
            // duration and IStreamingManager::Tick (called at the top of every tile) drains
            // those locations. IStreamingManager::RemoveStreamingViews is not exported from
            // the Engine module, so it is deliberately not called here.
            UE_LOG(LogWorldPartitionRVTBake, Display, TEXT("Captured tile %d/%d"), TileY * TilesX + TileX + 1, TilesX * TilesY);
        }
    }

    BeginReleaseResource(&RenderResources);
    FlushRenderingCommands();
    if (!bSuccess)
    {
        return false;
    }
    if (bGeneratePCG && GeneratedPCGCells == 0)
    {
        if (SkippedPCGCells > 0)
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("%d PCG cell(s) matched actor '%s' / component '%s' / graph '%s' but none of their partition actors were loaded. "
                     "Run PCGWorldPartitionBuilder -IterativeCellLoading -GenerateComponentEditingModeNormal=true over the full map and save it first."),
                SkippedPCGCells, *PCGActorName, *PCGComponentName, *PCGGraphName);
        }
        else
        {
            UE_LOG(LogWorldPartitionRVTBake, Error,
                TEXT("No PCG cells were found for actor '%s', component '%s', graph '%s'. Check that the actor is saved in the map, "
                     "its bounds intersect the RVT, the PCG component is loaded and not set to GenerateAtRuntime, and that "
                     "-PCGActor/-PCGComponent/-PCGGraph match the actor."),
                *PCGActorName, *PCGComponentName, *PCGGraphName);
        }
        return false;
    }
    if (SkippedPCGCells > 0)
    {
        UE_LOG(LogWorldPartitionRVTBake, Warning,
            TEXT("%d PCG cell(s) were skipped because their saved partition actors never loaded; those regions may have no generated grass in the SVT."),
            SkippedPCGCells);
    }

    Task.EnterProgressFrame();
    Component->InitializeStreamingTexture(EShadingPath::Deferred, ImageWidth, ImageHeight, FinalPixels.GetData());
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Component->GetStreamingTexture()->GetPackage());
    return SavePackages(PackagesToSave, PackageHelper, false);
}

