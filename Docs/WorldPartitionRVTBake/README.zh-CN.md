# WorldPartitionRVTBake 烘焙插件使用手册

适用版本：本项目当前 UE 5.8 插件源码。最近核对：2026-10-10。
下文命令使用 PowerShell，工程路径为 `D:\UEProjectReal\UE_MCPTest`。

## 1. 插件用途与入口

插件提供两个 World Partition 命令行构建器：

| 构建器 | 用途 | 输出 |
| --- | --- | --- |
| `WorldPartitionRVTBakeMapBuilder` | 根据 Actor 描述筛选地形、指定 PCG、RVT 及引用依赖，导出精简地图 | 新 `.umap`、外部 Actor 包和 JSON 清单 |
| `WorldPartitionTiledRVTBuilder` | 按 RVT 输出 tile 加载区域，准备 PCG，逐块采集并拼接 Streaming Virtual Texture | RVT Volume 绑定的 SVT Builder 资源包 |

导出保留 Landscape、StreamingProxy、地形组件和编辑层原有表示，不通过导出高度图重建地形。
分块烘焙仍会加载每块区域内的 Actor；先使用精简地图，可以减少区域中不相关的 Actor。

建议流程：**保存源地图 → 导出预览 → 导出精简地图 → 检查输入与资源引用 → RVT 配置验证 → PCG/SVT 烘焙 → 核对日志与画面。**

当前项目已有 `/Game/Level/BakeTest_RVTBake`，可以直接从第 3 节开始。
需要重新制作精简地图时，按第 4 节操作。

## 2. 烘焙前设置

1. 确认插件已启用、编译通过，工程使用 UE 5.8。
2. 保存地图及 Actor 配置。命令行读取磁盘上的已保存版本；运行前关闭该工程的编辑器，避免并发写入。
3. 地图必须使用 World Partition。
4. RVT Volume 关闭 `Is Spatially Loaded`，指定正确的 Runtime Virtual Texture 和 `Streaming Texture`（VirtualTextureBuilder 资源），`Build Levels` 至少为 1。
5. 当前构建器要求 Deferred 渲染路径，不支持单独的移动端 streaming texture。
6. 需要生成 PCG 时，PCG 组件使用 `GenerateOnLoad` 或 `GenerateOnDemand`；`GenerateAtRuntime` 会被拒绝。分区 PCG 需要有界生成网格。
7. PCG 生成的组件、地形或其他写入物体，必须实际配置为向目标 RVT 写入。能生成物体不等于能写入 RVT。

**导出后的材质、网格、PCG 图、RVT 和 SVT 资源仍共享。** 如果希望独立测试，先在内容浏览器创建或复制一份 SVT Builder，再在精简地图的 RVT Volume 上替换 `Streaming Texture`，保存地图。真实烘焙会更新该字段指向的资源。

如果最终需要把烘焙结果用于原地图，应保持 RVT 资源、Volume 位置、旋转、缩放和低 Mip 配置一致，并确认原地图使用预期的 SVT Builder。

## 3. 快速使用已有 Bake 地图

### 3.1 初始化路径

以下变量在同一个 PowerShell 窗口中设置一次；后续命令复用这些变量。

```powershell
$Editor = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Project = 'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject'
$ProjectRoot = Split-Path -Parent $Project
$Map = '/Game/Level/BakeTest_RVTBake'
$RVTVolume = 'RuntimeVirtualTextureVolume_UAID_047C160156AB250903_2076114856'
$PCGActor = 'BakeTest'
$ExportScript = Join-Path $ProjectRoot 'Plugins\WorldPartitionRVTBake\Scripts\Export-BakeMap.ps1'
```

`$Map` 使用 Unreal 的 `/Game/...` 资源路径，不填写磁盘 `.umap` 路径。
地图内 Actor 被重新命名后，应同步修改选择参数。

### 3.2 先验证 RVT 配置

```powershell
& $Editor $Project $Map `
  -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  "-RVTVolume=$RVTVolume" `
  -ValidateOnly -AllowCommandletRendering `
  -Unattended -stdout -FullStdOutLogOutput -SCCProvider=None

if ($LASTEXITCODE -ne 0) { throw 'RVT 配置验证失败，请检查日志。' }
```

核对 `RVT candidate` 中的 Actor、RVT、SVT 路径，必须是预期目标，且 `eligible=1`。
随后查看像素尺寸、tile 数量及缓冲大小。

`ValidateOnly` 不加载烘焙 tile、不生成或验证 PCG、不写入 SVT；它也不会替你判定输出画面是否正确。
当前代码在内存上限判断前返回，因此应自行对比报告的缓冲大小与实际烘焙的 `MaxBufferMiB`。

### 3.3 方式 A：烘焙时按 tile 准备 PCG

适用于本项目 `BakeTest` 这样的指定 PCG Actor。执行此段会更新所绑定的 SVT 资源。

```powershell
$LogDir = Join-Path $ProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
$BakeLog = Join-Path $LogDir ('RVTBake-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')

& $Editor $Project $Map `
  -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  "-RVTVolume=$RVTVolume" `
  "-PCGActor=$PCGActor" `
  -TileOverlap=2000 -PCGWaitSeconds=1800 -MaxBufferMiB=4096 `
  -AllowCommandletRendering -Unattended `
  -stdout -FullStdOutLogOutput -SCCProvider=None `
  2>&1 | Tee-Object -FilePath $BakeLog

if ($LASTEXITCODE -ne 0) { throw "SVT 烘焙失败，请检查 $BakeLog" }
```

非分区 PCG 组件会等待已有生成任务，或按需要生成；分区 PCG 按 tile 的重叠网格准备本地组件。
图中存在跨层级数据或复杂动态输入时，需核对每块生成结果，不能只看命令执行成功。

此方式只保存最终 SVT 包，运行中生成的 PCG 分区结果不会作为预生成内容保存。
相邻 tile 的加载范围重叠时，同一 PCG 单元可能多次生成，增加 `TileOverlap` 会增加这部分工作量。

### 3.4 方式 B：先保存 PCG，再采集 SVT

适用于想复用已保存的 PCG 输出、减少逐 tile 生成成本的情况。
先使用 UE 自带的 `PCGWorldPartitionBuilder`；下面的图名按当前测试图设置，换图时必须修改。

```powershell
$GraphName = 'BakeTest'

& $Editor $Project $Map `
  -run=WorldPartitionBuilderCommandlet `
  -Builder=PCGWorldPartitionBuilder `
  -IterativeCellLoading -IterativeCellSize=25600 `
  "-IncludeGraphNames=$GraphName" `
  -GenerateComponentEditingModeNormal `
  -AllowCommandletRendering -Unattended `
  -stdout -FullStdOutLogOutput -SCCProvider=None

if ($LASTEXITCODE -ne 0) { throw 'PCG 预生成失败，请先检查该阶段的日志。' }

& $Editor $Project $Map `
  -run=WorldPartitionBuilderCommandlet `
  -Builder=WorldPartitionTiledRVTBuilder `
  "-RVTVolume=$RVTVolume" `
  -UsePrebuiltPCG -TileOverlap=0 -MaxBufferMiB=4096 `
  -AllowCommandletRendering -Unattended `
  -stdout -FullStdOutLogOutput -SCCProvider=None

if ($LASTEXITCODE -ne 0) { throw '预生成 PCG 模式的 SVT 烘焙失败。' }
```

预生成阶段会保存 PCG 输出，检查日志是否真的处理了目标图并保存了 Actor 包。
组件保存为 Normal 编辑模式时，应添加 `GenerateComponentEditingModeNormal`，否则默认设置可能跳过它。
`IterativeCellSize=25600` 是此示例的加载单元大小，应结合项目的 PCG 网格设置调整。

`UsePrebuiltPCG` 不调度也不等待 PCG 生成；不能与 `PCGActor`、`PCGGraph` 或 `PCGComponent` 混用。
预生成地图 Actor 包有变化是此阶段的预期行为。

## 4. 选择性导出新的精简地图

### 4.1 预览与导出

当前 `/Game/Level/BakeTest_RVTBake` 已存在，不能重复覆盖。重新导出使用新名称，例如 `_v2`；后续重跑再换名称。

```powershell
$SourceMap = '/Game/Level/BakeTest'
$NewMap = '/Game/Level/BakeTest_RVTBake_v2'

# 只生成筛选清单。
& $ExportScript -Editor $Editor -Project $Project `
  -Map $SourceMap -NewPackage $NewMap `
  -PCGActor $PCGActor -RVTVolume $RVTVolume -ReportOnly

# 检查清单后，再创建地图和所选 Actor 包。
& $ExportScript -Editor $Editor -Project $Project `
  -Map $SourceMap -NewPackage $NewMap `
  -PCGActor $PCGActor -RVTVolume $RVTVolume

# 后续烘焙改用刚导出的地图。
$Map = $NewMap
```

清单位于 `Saved/RVTBakeExport/<目标地图短名>.json`，例如 `BakeTest_RVTBake_v2.json`。
查看 `sourceActorCount`、`selectedActorCount`、`actors` 中的 `reason`；
`selectedPackageBytes` 是选中源 Actor 包的体积估计，不包含共享资产总量。

`ReportOnly` 写清单，不创建目标地图、不加载所选空间 Actor。地图或目标外部包目录已存在时，预览和导出都会拒绝继续。
导出成功清单包含 `exportSucceeded: true`。保存后先打开新地图，确认地形和 PCG 输入完整。

### 4.2 筛选规则与额外输入

默认保留全部 Landscape、LandscapeStreamingProxy、LandscapeSplineActor，指定 PCG Actor、
一个指定 RVT Volume、WorldDataLayers、PCGWorldActor，以及所选 Actor 的持久化引用和挂接父 Actor。

Actor 描述无法推断 PCG 图中的动态标签/类查询、Biome 输入、排除 Volume 和投影几何。
需要这些输入时，给脚本增加 `IncludeActors` 或 `IncludeActorTags`；参数值必须对应地图中真实存在的 Actor 或标签。

```powershell
# 示例：只有源地图确实存在 RVT_Height 和 BakeInput 标签时才使用。
& $ExportScript -Editor $Editor -Project $Project `
  -Map $SourceMap -NewPackage $NewMap `
  -PCGActor $PCGActor -RVTVolume $RVTVolume `
  -IncludeActors @('RVT_Height') -IncludeActorTags @('BakeInput') `
  -ReportOnly
```

实际导出时复用相同输入参数，并移除 `ReportOnly`。额外选择条件未匹配时直接报错。
当前版本只支持主地图 Actor；选中 Level Instance/子容器、外部 Data Layer 或无法找到的必要引用时会失败。
源 PCGPartitionActor 不默认全部保留，只有持久化引用确实需要时才纳入。

### 4.3 导出结构验证（可选）

要求启用 PythonScriptPlugin。先完成导出，再执行以下命令。

```powershell
$ValidateScript = Join-Path $ProjectRoot 'Plugins\WorldPartitionRVTBake\Scripts\Validate-BakeMap.py'
$ManifestName = ($Map -split '/')[-1] + '.json'
$PreviousManifest = $env:RVT_BAKE_MANIFEST

try {
  $env:RVT_BAKE_MANIFEST = Join-Path $ProjectRoot ('Saved\RVTBakeExport\' + $ManifestName)
  & $Editor $Project -run=pythonscript "-script=$ValidateScript" `
    -Unattended -stdout -FullStdOutLogOutput -SCCProvider=None
  if ($LASTEXITCODE -ne 0) { throw '导出结构验证失败，请检查 validation 报告。' }
}
finally {
  $env:RVT_BAKE_MANIFEST = $PreviousManifest
}
```

成功日志为 `RVT_BAKE_EXPORT_VALIDATION_OK`。
报告在清单旁边，文件名以 `-validation.json` 结尾，正常情况下 `differences` 为空。
脚本比较 Actor 集合、变换、地形组件网格与包围盒、编辑层、PCG 图及 RVT/SVT 配置；不比较地形纹理逐像素内容，也不执行 SVT 烘焙。
替换了目标地图的 SVT Builder 或修改了配置后，源/目标配置差异可能是有意修改；此时需自行核对报告。

## 5. 参数速查

| 烘焙参数 | 含义 |
| --- | --- |
| `RVTVolume` | RVT Actor 内部名称或标签，必须唯一；标签重名时用内部名称 |
| `PCGActor` | PCG Actor 内部名称、标签或 Blueprint 类名 |
| `PCGGraph` | PCG 图资源名称、包路径或完整对象路径；可进一步筛选 |
| `PCGComponent` | PCG 组件名称，需搭配 `PCGActor` 或 `PCGGraph` |
| `UsePrebuiltPCG` | 直接采集已保存的 PCG 内容，不生成 PCG |
| `TileOverlap` | 每个 tile 加载边界外扩距离，单位厘米；默认 0 |
| `PCGWaitSeconds` | 每个等待阶段的 PCG 超时秒数；默认 1800，0 表示不设超时 |
| `MaxBufferMiB` | 完整 SVT 像素拼接缓冲上限；默认 4096，不是整个进程的内存上限 |
| `ValidateOnly` | 检查 RVT 配置并报告输出尺寸和缓冲大小，然后退出 |
| `AllowCommandletRendering` | 允许命令行渲染，RVT 验证和真实烘焙命令都应带上 |

| 导出脚本参数 | 含义 |
| --- | --- |
| `Editor` / `Project` / `Map` | 引擎程序、项目文件、已保存源地图 |
| `NewPackage` | 新目标地图的 `/Game/...` 包路径，不能覆盖已有目标 |
| `PCGActor` | 所选 Actor 必须带有 PCG 组件；也可按 Blueprint 类名匹配多个实例 |
| `RVTVolume` | 指定一个 RVT Volume；有多个 RVT 时应明确指定 |
| `IncludeActors` | 额外 Actor 名称、标签或 Blueprint 类名数组 |
| `IncludeActorTags` | 额外 Actor 标签数组 |
| `ReportOnly` | 仅生成筛选清单 |

只采集已保存的普通写入几何时，可不传 PCG 选择参数。这样不会主动生成 PCG，必须自行确认场景中的写入内容已完整保存。

## 6. 日志判读与常见问题

工程默认日志目录是 `Saved/Logs`；第 3.3 节会另外保存一份带时间的完整控制台日志。

| 日志或现象 | 检查与处理 |
| --- | --- |
| `No registered RVT volume matches` | 查看 `RVT candidate`，确认名称/标签、Actor 已保存且组件已注册；Volume 关闭空间加载 |
| `not eligible` | 检查 RVT、Streaming Texture、Build Levels 和 Is Spatially Loaded |
| `Destination already exists` | 选新地图名；目标外部包目录存在也会拒绝，避免覆盖之前成果 |
| 生成物体未进入 SVT | 检查 PCG 图动态输入是否导出、生成触发方式、写入组件的 RVT 列表，以及实际选择的 RVT/SVT 路径 |
| `PCG candidate` 中找不到目标 | 检查已保存 Actor、组件是否加载、有无图资源以及选择参数；`ValidateOnly` 不检查这些 |
| `GenerateAtRuntime` | 将组件保存为 GenerateOnLoad 或 GenerateOnDemand |
| `needs bounded partition grids` | 检查分区 PCG 网格配置；当前分块生成不接受无界网格 |
| `saved actor record ... not loaded` / skipped PCG cells | 检查预生成是否覆盖并保存该区域；扩大 TileOverlap 不能代替缺失的输入或结果 |
| `SVT pixel buffer needs ... limit ...` | 降低输出尺寸/低 Mip 配置，或在可用内存允许时提高 MaxBufferMiB |
| PCG 超时 | 查看图任务及对应区域，检查资源编译/动态依赖；确认任务能完成后再调整等待时间 |
| 保存失败 | 检查磁盘权限、只读文件、可用空间和资源的版本控制状态 |

`Captured tile N/N` 表示采集结束；还需看到目标 SVT 包保存、临时输出移动到目标 `.uasset`，以及 Commandlet `result 0`。
这些日志支持“采集和写盘完成”，不能单独证明 PCG 画面正确。保存成功后重新加载 SVT/场景，检查 PCG 覆盖区域与 tile 边界。

## 7. 当前验证记录与后续维护

2026-10-10 的精简地图导出测试：源地图 4,449 个 Actor，保留 69 个；
目标地图及 Actor 包约 7.5 MiB，256 个地形组件的公开结构与配置对比通过，
源地图及外部包 4,450 个文件哈希未变化。已有目标覆盖保护通过。
RVT `ValidateOnly` 报告 8192×8192、32×32 个 tile、512 MiB 像素缓冲。
该结构测试不等于对所有 PCG 图和 SVT 像素结果的完整验证。

源码、脚本和本手册由新 GitHub 仓库管理；Content 和运行日志没有随源码上传。
从仓库恢复到其他项目时，还需恢复实际测试地图、RVT/SVT 资源和 PCG 图等内容，并修改脚本路径。

`Sync-ToTestProject.ps1` 用于从 ZDG 插件同步源码到测试项目，不能从测试副本调用。
同步只更新代码和说明，不会自动编译，也不会把测试项目内的修改反向同步到 ZDG。

相关文件：

- [插件 README](../../Plugins/WorldPartitionRVTBake/README.md)
- [导出测试记录](../../Plugins/WorldPartitionRVTBake/TEST_RESULTS.md)
- [选择性导出脚本](../../Plugins/WorldPartitionRVTBake/Scripts/Export-BakeMap.ps1)
- [导出验证脚本](../../Plugins/WorldPartitionRVTBake/Scripts/Validate-BakeMap.py)
