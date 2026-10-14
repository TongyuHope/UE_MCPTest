# HLOD 与 World Partition MiniMap 构建（UE 5.8）

这两个功能直接调用 Epic 的官方构建器，插件只提供 PowerShell 入口、参数检查、可选 PCG 预生成和独立日志。
不需要复制或修改引擎构建器，也不需要为本次脚本变更重新编译插件。

| 功能 | 构建器 | 输出 |
| --- | --- | --- |
| HLOD | `WorldPartitionHLODsBuilder` | 指定地图的 HLOD Actor、代理网格及相关资源 |
| MiniMap | `WorldPartitionMiniMapBuilder` | 世界分区编辑器预览图，保存在该地图的 MiniMap Actor 外部包中 |

MiniMap 是编辑器功能，Actor 是 EditorOnly，纹理是 Actor 内的对象；它不会自动提供打包游戏中的 UMG 小地图、玩家图标或世界坐标映射。若需要游戏内小地图，应另做独立纹理导出和运行时界面。

## 1. 地图选择与 PCG

- 完整场景的 HLOD 应在实际运行地图上生成。RVT 专用 Bake 地图只含地形、选中的 PCG/RVT 和引用依赖，不能代替原地图的完整 HLOD。
- MiniMap 同样只显示目标地图中实际存在并被捕获的内容。选择精简地图时，需要自行保留所需建筑、场景物体和照明；默认 SceneColor 捕获依赖场景渲染。
- 两个官方构建器都不负责插件的逐 tile PCG 强制生成。希望包含 PCG 输出时，先生成并保存 PCG。RVT 构建时的临时 PCG 输出不会保存，不能供另一个进程复用。
- 脚本的 `-GeneratePCG` 先运行官方 `PCGWorldPartitionBuilder`，采用迭代加载并包含 Normal 编辑模式组件。`-PCGGraphs` 按图资产名称筛选；不提供则不按图筛选。
- PCG 预生成会保存地图相关的生成结果。Runtime PCG / GPU 输出是否能够持久化，取决于组件、节点和输出类型；设置 Runtime 触发的组件不能视为已有离线可用结果。先确认生成日志和保存后的场景。
- PCG 预生成成功不代表输出会自动加入 HLOD。对应 Actor/组件还需要适当的 HLOD Layer 和参与 HLOD 设置。

## 2. 快速使用

保存目标地图并关闭对应编辑器。默认项目为 `D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject`，默认地图为 `/Game/Level/BakeTest`。
脚本文件若被 PowerShell 执行策略阻止，可只为当前窗口设置：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
$Scripts = 'D:\UEProjectReal\UE_MCPTest\Plugins\WorldPartitionRVTBake\Scripts'
```

### 2.1 HLOD

先在地图中配置 HLOD Layer（默认层、Actor 或 Data Layer 的层分配）及参与 HLOD 的对象。脚本不会自动替对象选择合并、简化或实例化策略。

预览参数；不会启动引擎、生成 PCG、创建日志目录或写地图：

```powershell
$Plan = & "$Scripts\Build-HLOD.ps1" -Map '/Game/Level/BakeTest' -Preview
$Plan | ForEach-Object { $_.Editor; $_.Arguments -join "`n" }
```

创建 HLOD Actor 并构建代理资源：

```powershell
& "$Scripts\Build-HLOD.ps1" -Map '/Game/Level/BakeTest'
```

需要先生成 BakeTest 图对应的 PCG 时：

```powershell
& "$Scripts\Build-HLOD.ps1" -Map '/Game/Level/BakeTest' `
  -GeneratePCG -PCGGraphs @('BakeTest') -PCGCellSize 25600
```

阶段控制和层筛选：

```powershell
# 仅创建/更新 HLOD Actor，尚未构建代理网格。
& "$Scripts\Build-HLOD.ps1" -Action Setup

# 构建已存在的 HLOD Actor。先确保 Setup 已运行且拓扑没有变更。
& "$Scripts\Build-HLOD.ps1" -Action Build

# 更新 HLOD Actor，然后强制重建；不是先删除全部 HLOD。
& "$Scripts\Build-HLOD.ps1" -Action Rebuild

# 按 HLOD Layer 资产名称筛选构建阶段，不接受 /Game/... 资产路径。
# 默认 SetupBuild 的 Setup 仍处理地图；筛选只作用于 Build。
& "$Scripts\Build-HLOD.ps1" -Action Build -HLODLayer 'HLODLayer_Default'
```

`Setup` 本身可能添加、更新或删除已失效的 HLOD Actor；不是只读检查。脚本没有封装 `DeleteHLODs` 和 `AutoSubmit`。

### 2.2 MiniMap

```powershell
# 参数预览。
& "$Scripts\Build-MiniMap.ps1" -Map '/Game/Level/BakeTest' -Preview |
  ForEach-Object { $_.Editor; $_.Arguments -join "`n" }

# 生成/更新编辑器小地图。
& "$Scripts\Build-MiniMap.ps1" -Map '/Game/Level/BakeTest'

# 尚无保存的 PCG 结果时，先生成 PCG 再捕获。
& "$Scripts\Build-MiniMap.ps1" -Map '/Game/Level/BakeTest' `
  -GeneratePCG -PCGGraphs @('BakeTest') -DebugCapture
```

`DebugCapture` 额外输出 tile 和完整图的 BMP，位置为项目 `Intermediate/Minimap`。这些是调试图片，不是独立保存的游戏纹理资产。

### 2.3 切换到 ZDG

入口仍使用当前测试项目插件中的脚本，通过参数切换宿主项目和地图：

```powershell
$ZDG = 'E:\ZDG\ZDG.uproject'
& "$Scripts\Build-HLOD.ps1" -Project $ZDG -Map '/Game/Maps/ZDGMain' `
  -EnablePlugins @('LandscapePatch') -SourceControlProvider Default -Preview

& "$Scripts\Build-MiniMap.ps1" -Project $ZDG -Map '/Game/Maps/ZDGMain' `
  -EnablePlugins @('LandscapePatch') -SourceControlProvider Default -Preview
```

这两个示例先预览；正式执行时移除 `-Preview`。ZDG 默认地图依赖 LandscapePatch 编辑层，构建进程需要加载该插件。
`Default` 使用项目配置的源代码管理提供方；脚本默认 `None`，要求待保存文件可写。

## 3. 参数

| 参数 | 入口 | 用途 |
| --- | --- | --- |
| `Editor` | 两者 | `UnrealEditor-Cmd.exe`，默认 `E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| `Project` | 两者 | `.uproject` 路径，默认 UE_MCPTest |
| `Map` | 两者 | `/Game/...` 地图包路径，默认 `/Game/Level/BakeTest`；要求文件存在 |
| `Action` | HLOD | `SetupBuild`（默认）、`Setup`、`Build`、`Rebuild` |
| `HLODLayer` | HLOD | 按层资产名称筛选构建；不能与仅 `Setup` 组合 |
| `DebugCapture` | MiniMap | 输出调试 BMP |
| `GeneratePCG` | 两者 | 先运行 PCG 构建器并保存结果，再运行目标构建器 |
| `PCGGraphs` | 两者 | 图资产名称数组，需要 `GeneratePCG`；不提供则不过滤图 |
| `PCGCellSize` | 两者 | PCG 预生成迭代单元大小（厘米），默认 25600；不改变 HLOD/MiniMap 单元大小 |
| `SourceControlProvider` | 两者 | 默认 `None`；`Default` 不传 SCC 参数，使用项目配置；也可传 `Subversion` 等提供方名称 |
| `EnablePlugins` | 两者 | 本次进程额外启用的插件名称数组，不改 `.uproject` |
| `EnablePython` | 两者 | 默认传 `-DisablePython` 防止启动 UI 工具脚本；该开关省略禁用参数，恢复项目的正常 Python 启用规则 |
| `Preview` | 两者 | 返回每个阶段的可检查参数对象，不启动引擎；不等于引擎侧配置验证 |

日志分别存放在目标项目的 `Saved/Logs/WorldPartitionBuild`，文件名包含地图、构建器和时间戳。
任一阶段返回非零退出码，脚本会报错并停止后续阶段。引擎启动阶段的其他资源错误也可能造成非零退出码；脚本不会自动忽略它们，也不会回滚已经保存的包。

## 4. MiniMap 的分辨率、范围和外观

这些设置由地图中的 `WorldPartitionMiniMap` Actor 保存。首次构建会创建该 Actor；之后可在编辑器中检查或调整并保存，再重新构建。

| 属性 | 作用 | UE 5.8 新 Actor 默认值 |
| --- | --- | --- |
| `WorldUnitsPerPixel` | 每像素世界单位；数值越小，目标图越精细，实际分辨率仍受引擎限制 | 50 cm/pixel |
| `BuilderCellSize` | 单次迭代加载范围；减小可降低每次捕获的资源压力，但增加迭代次数 | 102400 cm |
| `CaptureSource` | 颜色或其他捕获通道 | SceneColor HDR |
| `CaptureWarmupFrames` | 每次捕获前的预热帧数 | 5 |
| `IncludedDataLayers` / `ExcludedDataLayers` | 控制参与捕获的数据层 | 空集合 |

需要限定范围时，在地图中设置 `WorldPartitionMiniMapVolume`。没有范围 Volume 时，构建器按世界范围计算；多个 Volume 合并为范围。
项目应启用 Virtual Texture Support。若图过暗、缺建筑或没有 PCG，先检查目标地图内容、照明、数据层和持久化生成结果。

官方 MiniMap 构建器的 `PreRun` 会从该 Actor 读取 `BuilderCellSize`，所以脚本没有提供一个实际上会被覆盖的 `-IterativeCellSize`。需要命令行修改这些属性或输出独立运行时纹理时，再扩展预配置/导出能力。

## 5. 验收与实现边界

- HLOD：检查日志中的实际 HLOD Actor 数量。0 个不代表已经生成远景，需要回查 Layer 和参与设置。确认外部包/代理资源保存，再在目标地图验证远景和加载行为。
- MiniMap：确认 MiniMap Actor 外部包保存，重新打开目标地图的世界分区窗口查看覆盖范围和图像内容。
- PCG：确认匹配的图确实生成，重新加载地图后输出仍存在。没有匹配输出的成功进程不能证明场景内容正确。
- 脚本语法、参数映射、阶段顺序和失败停止逻辑已通过检查。2026-10-14，用户在 UE_MCPTest 的 `/Game/Level/BakeTest` 实际运行两个构建器：HLOD 处理 80 个 Actor，HLOD / MiniMap 日志均为 result 0、0 error(s)、4 warning(s)，并有资源保存记录；用户随后确认在编辑器中成功查看 HLOD。没有验证所有地图及 PCG 预生成后的完整覆盖。
- 本次工作在 UE_MCPTest 插件中进行，未反向同步 ZDG，也没有修改引擎或 C++ 构建器。

官方文档：[构建器命令参考](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/world-partition-builder-commandlet-reference)、[HLOD 设置](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine)。
参数和默认值同时核对了本机 UE 5.8 的 `WorldPartitionHLODsBuilder.cpp`、`WorldPartitionMiniMapBuilder.cpp`、`WorldPartitionMiniMap.h/.cpp` 和 `PCGWorldPartitionBuilder.cpp`。
