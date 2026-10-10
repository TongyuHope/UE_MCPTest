# World Partition RVT Bake

This editor-only plugin builds the streaming low mips for one large Runtime Virtual Texture. It loads the World Partition actors overlapping each RVT output tile, captures the tile, then unloads the region. The final pixels are assembled into one SVT asset and saved once. PCG can be generated separately by UE's official `PCGWorldPartitionBuilder` and reused as prebuilt content during the RVT pass.

Why this is needed: UE's stock RVT builder (`RuntimeVirtualTextureBuildStreamingMips.cpp`) loads every RVT writer actor at once, which does not fit an open world. The tiled builder keeps only one tile's writers resident at a time.

## Selective bake-map export (UE 5.8)

`WorldPartitionRVTBakeMapBuilder` creates a new World Partition map from actor descriptors.
It keeps all Landscape/StreamingProxy/LandscapeSpline actors, the PCG actor(s) selected
by name/label/Blueprint class, one selected RVT Volume, WorldDataLayers/PCGWorldActor,
and the transitive persisted actor references and attachment parents of this selection.
The external-package relocation and reference repair follow UE 5.8's
`WorldPartitionRenameDuplicateBuilder.cpp`, but only selected actor clusters are saved.
Landscape components, height/weight textures and edit-layer objects retain their original
representation; the exporter does not convert terrain to heightmaps.

Save the source map and close its editor before exporting. First inspect the manifest:

```powershell
& '.\Plugins\WorldPartitionRVTBake\Scripts\Export-BakeMap.ps1' -ReportOnly
& '.\Plugins\WorldPartitionRVTBake\Scripts\Export-BakeMap.ps1'
```

The script defaults to UE_MCPTest's `/Game/Level/BakeTest`, PCG label `BakeTest`, and
`/Game/Level/BakeTest_RVTBake`. A destination map or external-package directory that
already exists is rejected. Export never saves/deletes the source map or its actor packages.
The manifest is `Saved/RVTBakeExport/<destination-name>.json`; it lists source/selected
actor counts, estimated source package bytes and each actor's inclusion reason.
`-ReportOnly` writes only this manifest and does not load the selected spatial actors or
create destination packages.

For ZDG, supply the project and selectors explicitly:

```powershell
& '.\Plugins\WorldPartitionRVTBake\Scripts\Export-BakeMap.ps1' `
  -Project 'E:\ZDG\ZDG.uproject' -Map '/Game/Maps/ZDGMain' `
  -NewPackage '/Game/Maps/ZDG_RVTBake' `
  -PCGActor 'PCG_Bake_Biome_Runtime_CPUGrass' -RVTVolume 'RVT_Mat' `
  -IncludeActors @('RVT_Height') -ReportOnly
```

Actor descriptors cannot discover dynamic queries in PCG graphs (Get Actor Data by
tag/class, Biome inputs, exclusion volumes, projection geometry, etc.). Add these inputs
with `-IncludeActors` or `-IncludeActorTags`; unmatched selectors fail instead of silently
omitting data. Selected Level Instances/child containers and external data layers are
rejected in this first version. Source PCG partition results are not selected by default;
they are included only when a persisted reference requires them. Regenerate the selected
PCG in the destination before relying on its output.

Materials, meshes, graphs, RVT assets and SVT Builder assets remain shared references.
Before a test bake, assign a separate SVT Builder to the exported RVT Volume if the source
SVT must remain untouched. Source and destination RVT bounds/settings must stay identical
when the baked result will be used by the original map.

### Export validation

`Scripts/Validate-BakeMap.py` reloads the source and destination with UE's PythonScript
commandlet, checks the exact exported actor set, then compares actor transforms,
landscape component grid positions/bounds, landscape material and edit-layer settings,
PCG graph/trigger/partitioning, and RVT/SVT Builder/low-mip settings. It does not save
assets or bake SVT. It requires the PythonScriptPlugin (enabled in UE_MCPTest).

```powershell
& 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject' `
  -run=pythonscript `
  '-script=D:\UEProjectReal\UE_MCPTest\Plugins\WorldPartitionRVTBake\Scripts\Validate-BakeMap.py' `
  -Unattended -SCCProvider=None
```

The default manifest is `Saved/RVTBakeExport/BakeTest_RVTBake.json`.
Set the `RVT_BAKE_MANIFEST` environment variable for another export. The comparison
report is next to the manifest, with the suffix `-validation.json`.
Texture pixel contents are not compared by this structural check.

## PCG modes

This is the part that surprises people, so it is worth stating precisely.

When `-PCGActor` or `-PCGGraph` is supplied, `GeneratePCGForTile` calls `GenerateLocalGetTaskId(GenerateOnDemand, /*bForce=*/true, GridSize)` on each partition actor's **local** component. Two facts matter:

1. **`bForce=true` is required.** A local component loads back with `bGenerated` serialized to `true` and `bDirtyGenerated` cleared by `PostLoad` ("We should never refresh on load"). The `WITH_EDITOR` "always generate when partitioned" branch in `UPCGComponent::ShouldGenerate` tests `IsPartitioned()`, which is `bIsComponentPartitioned && Cast<APCGPartitionActor>(GetOwner()) == nullptr` — a local component's owner *is* the partition actor, so that branch never applies to it. With `bForce=false` the call is a silent no-op.
2. **The `GridSize` argument is a filter, not a data source.** `UPCGSubsystem::ScheduleComponent` rejects a request whose grid size does not match the component's own `GenerationGridSize`, logging "Didn't schedule any task". Generation is scheduled per local component, deliberately not on the original component, because dispatching on the original also hits cells outside the current bake tile.

For the recommended two-stage workflow, run `PCGWorldPartitionBuilder` first and then pass `-UsePrebuiltPCG` to this builder. In that mode the RVT builder does not schedule or wait for PCG. It only lets asset compilation and render-state updates settle before capturing the loaded, saved PCG output. This avoids re-running GPU PCG during every RVT tile.

Consequences of tile-local PCG generation mode:

- Every cell is regenerated for **every tile whose load bounds overlap it**. The build cost therefore scales with the number of tiles overlapping each PCG grid cell, not with the map size. Raising `-TileOverlap` makes this worse, not better.
- Cross-grid pin data (`PerPinGeneratedOutput`) is transient. Hierarchical graphs are generated largest-grid-first so children can consume their parents within the same session, but a child grid whose parent grid is not loaded generates from **empty** upstream data with no error.
- Only the SVT package is saved. Regenerated PCG partition actors are discarded with the process, so on-disk PCG content is never overwritten. This also means the run is not resumable and the next run pays the full generation cost again.

## Prerequisites

- The RVT Volume must be non-spatially-loaded and configured with a Virtual Texture Builder asset and streaming low mips.
- The selected PCG component must use a non-runtime generation trigger. Partitioned components need bounded generation grids; a PCG component placed directly in the world is also supported and is generated when its actor is loaded for the current tile. A component still managed by the runtime generation system is rejected, because a non-runtime generation request for it is refused by PCG itself.
- The builder currently supports the deferred shading path only. Separate mobile SVT output is rejected.
- The GPU scene must be initialized. The builder asserts this via `RuntimeVirtualTexture::IsSceneReadyToRender` before rendering, and flushes level streaming plus recreates render state before the first tile. Without that guard a blank result would be saved as a successful bake.

## Commands

Run these from PowerShell with the editor closed.

```powershell
$Editor  = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Project = 'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject'
$Map     = '/Game/Level/BakeTest'

# 1. Prepass: generate the whole map's PCG output and save it with UE's official builder.
#    -GenerateComponentEditingModeNormal=true is mandatory in practice: the default is
#    LoadAsPreview only, so a component saved in Normal mode is skipped silently and
#    nothing is generated or saved at all.
& $Editor $Project $Map -run=WorldPartitionBuilderCommandlet `
  -Builder=PCGWorldPartitionBuilder `
  -IterativeCellLoading -IterativeCellSize=25600 `
  '-IncludeGraphNames=PCG_Runtime_GPUGrass' `
  -GenerateComponentEditingModeNormal `
  -AllowCommandletRendering -Unattended

# 2. Check the RVT selection and the required buffer size, without baking.
& $Editor $Project $Map -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  -RVTVolume=YourRVTVolumeActor `
  -ValidateOnly -AllowCommandletRendering -Unattended

# 3. The real bake. No PCG graph is scheduled here; saved PCG output is loaded per tile.
& $Editor $Project $Map -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  -RVTVolume=YourRVTVolumeActor `
  -UsePrebuiltPCG `
  -TileOverlap=0 -MaxBufferMiB=4096 `
  -AllowCommandletRendering -Unattended
```

For a PCG graph placed directly in the map (for example an actor labelled `BakeTest`), use the graph generation mode instead of `-UsePrebuiltPCG`:

```powershell
& $Editor $Project $Map -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  -RVTVolume=YourRVTVolumeActor `
  -PCGActor=BakeTest `
  -TileOverlap=2000 -MaxBufferMiB=4096 -PCGWaitSeconds=1800 `
  -AllowCommandletRendering -Unattended
```

The component is loaded with each tile, its `GenerateOnLoad` result is allowed to finish, and the tile is then captured. If the component is already generated and uses non-procedural output, the builder reuses that result; otherwise it schedules that one scene component on demand.

If the map has exactly one eligible RVT Volume, `-RVTVolume` can be omitted; the builder uses the actor's internal name, not its display label. Read the `RVT <name>: ... tiles` line in the log to confirm it picked the volume you meant.

## Parameters

| Parameter | Meaning |
| --- | --- |
| `-RVTVolume=` | Selects the RVT volume actor by internal name. Must resolve to exactly one eligible candidate. |
| `-PCGActor=` | Selects a placed actor by internal name, label, or Blueprint class name (for example `BP_Runtime_GPUGrass`). |
| `-PCGComponent=` | Narrows to one component name on the selected actor (for example `PCG`). Requires `-PCGActor` or `-PCGGraph`. |
| `-PCGGraph=` | Filters by graph asset name (`PCG_Runtime_GPUGrass`), package path (`/Game/PCG/PCG_Runtime_GPUGrass`), or full object path. Optional when `-PCGActor` already selects one component. |
| `-UsePrebuiltPCG` | Uses PCG partition output generated and saved by `PCGWorldPartitionBuilder`; this builder does not schedule or wait for PCG. Do not combine it with `-PCGActor`, `-PCGComponent`, or `-PCGGraph`. |
| `-TileOverlap=` | Centimeters of extra load bounds around each tile, to keep edge writers resident. Defaults to `0`. Does **not** help when a partition actor simply was never saved. |
| `-MaxBufferMiB=` | Limit for the complete uncompressed low-mip pixel buffer. Defaults to `4096`. Run `-ValidateOnly` first to read the actual requirement. |
| `-PCGWaitSeconds=` | Per-phase PCG wait budget. Defaults to `1800`; `0` disables the timeout. |
| `-ValidateOnly` | Reports the RVT selection and buffer size, then returns before any tile is loaded. |

Omit both `-PCGActor` and `-PCGGraph` only when all writer geometry was already generated and saved and you want a pure capture pass with no regeneration.

## Reading the log

Generation of PCG is skipped entirely when neither `-PCGActor` nor `-PCGGraph` is given, so a bad selector silently yields a capture-only bake. Two log lines exist to make that visible:

- `PCG candidate: actor '...' component '...' graph '...' trigger=.. editingMode=.. runtimeManaged=..` — printed for every partitioned, non-local PCG component **before** filtering. If your actor is missing here it is not a candidate at all; if it is present with `runtimeManaged=1` the bake will refuse it.
- `PCG grid N cell (X, Y, Z) has an actor record but is not loaded` / `N PCG cell(s) ... have a saved actor record that is not loaded in this tile` — the cell exists on disk but no partition actor is loaded. This is almost always the prepass not having covered or saved that region; re-run command 1 rather than raising `-TileOverlap`.

A tile with no PCG cells at all is **not** an error: the RVT volume is normally larger than the generation volume, so empty tiles are expected, logged at Display, and skipped. The run only fails if no cell anywhere matched, or if a local component refuses to schedule.

Missing partition actors are always created in memory for the current tile (`bCanCreateActor=true`) rather than silently skipped. Without this, cells with no saved actor record are dropped by `ForAllOverlappingCells` with no callback at all, which would bake grass coverage gaps into the SVT without any diagnostic.

## Notes

- `-ValidateOnly` does not load tiles or validate the PCG actor; it only checks the RVT selection and the buffer budget.
- Only the streaming texture package is saved. Use `-AutoSubmit` if you want the result submitted to source control.
- The stock RVT builder still loads all RVT writer actors at once. Use `WorldPartitionTiledRVTBuilder` for the second command.
