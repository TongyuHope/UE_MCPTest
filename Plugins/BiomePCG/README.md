# BiomePCG

从旧工程以下两个位置迁入的 PCG 节点及编辑器工具插件：

- `D:\UEProjectReal\PCGBiome_test\Plugins\PCGSenc\Source`
- `D:\UEProjectReal\PCGBiome_test\Source\PCG_Senc`

保留原有功能节点算法和默认参数；旧 ProjectionDebug 水印已停用，保留类型用于兼容旧资产并清理残留节点。

## 功能和入口

新增多样条绘制工具：`TA Tools → 绘制样条植被`。单击地表放置蓝图 Actor，之后每次按住左键绘制一条独立样条，松开提交。
面板“绘制方式”还可选择“逐点点击”：单击依次确认控制点，按 Enter 或点击“完成当前线 / 绘制下一条”提交；支持 Backspace 回退和 Esc 取消。
首次放置可自动创建 `/Game/PCG/BP_PCG/BP_BiomeSplinePainter`，PCG 图由用户指定。
详细操作与待验收项目见工程 `Docs/BiomePCG/SplineVegetationPainterUsage.md`。
此功能尚未运行 C++ 编译或编辑器交互验证。

| 功能 | 使用方式 |
| --- | --- |
| Model Showcase / 资产排列 | 在 Content Browser 选择文件夹，在场景中选择作为排列原点的 Actor，通过顶栏 `TA Tools → 资产排列` 打开。支持 X/Y 行轴、尺寸排序、自动适配、文件夹及高度文字。 |
| PCG To Foliage / 烘焙 Foliage | 在世界大纲选择一个或多个带有 ISM/HISM 的 Actor，通过 `TA Tools → 烘焙Foliage` 打开。递归处理附属 Actor，保留世界变换、材质、剔除距离和碰撞预设，按位置分发到 Foliage 分区。默认保留原实例。 |
| Collision Preset Manager | 通过 `Window → TA Tools → Collision Preset Manager` 打开，选择文件夹或 StaticMesh 资产后批量修改并保存碰撞预设。 |
| 蓝图工具库 | `BiomePCG`、`BiomePCG\|Model Showcase`、`BiomePCG\|Collision Preset` 及 `PCG\|Foliage` 分类下提供原有工具函数。 |
| Convert SMA | 在蓝图中调用 `PCG\|ConvertSMA → Convert ISM To Static Mesh Actors`，递归将 Actor 层级中的 ISM/HISM 实例转为独立 StaticMeshActor。保留变换、材质、阴影、Mobility 和网格碰撞预设，并放入指定文件夹；沿用原函数默认清空源实例的设置。 |
| 旧水印清理 | 打开、保存 `/Game/PCG` 及其子目录中的 Blueprint/PCG Graph 时，只移除旧 ProjectionDebug 水印节点，不再插入任何水印。 |

Foliage 烘焙新建或更新的 FoliageType 资产会标记为待保存，使用后应保存相关资产和关卡。

## 自定义 PCG 节点

以下节点迁入 `Source/BiomePCGRuntime`，保留原节点名、类名及引脚定义，可在 PCG Graph 中使用：

| 节点 | 功能 | 源文件 |
| --- | --- | --- |
| Attribute String Split | 解析带括号、引号的逗号分隔字符串，写入 M1、M2 等点属性。 | `PCGAttributeStringSplit.h/.cpp` |
| Check Attribute | 检查点数据中是否存在指定名称或名称前缀的属性，输出布尔 AttributeSet。 | `PCGCheckAttributeSettings.h/.cpp` |
| Check String | 检查输入 AttributeSet 的首个字符串属性是否包含指定子串，输出布尔 AttributeSet。 | `PCGCheckString.h/.cpp` |
| DiffencePoints | 进行空间差集运算，正常差集路径转换为点数据输出，沿用原实现及名称拼写。 | `PCGDiffencePoints.h/.cpp` |
| StringToAttributeTest | 按指定分隔符拆分字符串，并清理片段边界的括号、引号，写入多个点属性。 | `PCGStringToAttributeTest.h/.cpp` |

## 模块与迁移

- `BiomePCGRuntime`（Runtime）：原工程模块中的五个 PCG 节点，以及 `ConvertSMALib`、`PCGToFoliageLib` 的蓝图工具类。公开依赖 Core、CoreUObject、Engine、PCG；转换工具所需的 AssetRegistry、Foliage、PhysicsCore、RenderCore、UnrealEd 依赖仅在编辑器构建中添加。
- `BiomePCG`（Editor）：原 `PCGSenc` 工具库、Slate 面板和 TA Tools 菜单；通过依赖 `BiomePCGRuntime` 调用 Foliage 烘焙库。
- `BiomePCGEditor`：样条绘制模式、面板、旧 ProjectionDebug 节点的兼容类型和清理回调。
- `Source\PCG_Senc` 中七组功能 `.h/.cpp` 均已迁入 `Source/BiomePCGRuntime`：五组 PCG 节点、一组 ISM 转 StaticMeshActor、一组 Foliage 烘焙库。
- 原工程的 `PCG_Senc.cpp` 只注册主游戏模块，`PCG_Senc.h` 只包含 CoreMinimal；插件使用自身模块入口替代，不复制 `IMPLEMENT_PRIMARY_GAME_MODULE`。原 `PCG_Senc.Build.cs` 的依赖按实际用途整合进插件 Build.cs，无需旧工程模块或额外头文件路径。
- 原 `UPCGSencBPLibrary` 更名为 `UBiomePCGBPLibrary`；其他反射类型保留类名，更新模块名和导出宏。
- `Config/DefaultBiomePCG.ini` 提供旧工具类、五个 PCG Settings 类、枚举、ProjectionDebug 及历史 ContactWatermarkNode 的 Core Redirects，支持后续迁入引用这些类型的资产。
- 两个转换工具库同时提供 `/Script/PCG_Senc` 和此前 `/Script/BiomePCG` 路径到 `/Script/BiomePCGRuntime` 的重定向，兼容旧工程资产及此前迁移后保存的资产。
- 为适应插件独立编译补充直接使用的头文件引用，并将 ConvertSMA 内部递归辅助函数更名，避免与 Foliage 烘焙库在 Unity 编译中重复定义。
- 本次只迁移源码及必要配置，没有迁移旧工程 Content，旧工程文件未改动。

编辑器工具模块不会编入游戏运行时；自定义 PCG 节点位于独立 Runtime 模块。
两个转换工具类位于 Runtime 模块，使普通 Actor 蓝图（例如 `BP_PCGBiome_Cluster`）可以引用其节点。转换实现和专用头文件使用 `WITH_EDITOR` 隔离；非编辑器构建中函数仅记录提示并返回，不执行转换或烘焙。
依赖的 `PCG` 和 `EditorScriptingUtilities` 插件由描述文件声明，插件默认启用。
迁移后需要重启编辑器，以加载新增模块和反射类型。

## 普通 Actor 蓝图中的转换节点

此前将 `ConvertSMALib` 和 `PCGToFoliageLib` 放在 Editor 模块中，会导致普通 Actor 蓝图编译提示“无法在此运行时蓝图中使用编辑器函数”。
这属于工具类所在模块的限制，与当前是否正在编辑器中运行无关。
现已将这两个类及其声明移入 Runtime 模块，编辑器专用转换逻辑继续通过 `WITH_EDITOR` 隔离。
更改模块归属后请关闭编辑器、完整编译并重新打开工程，再编译受影响蓝图；如仍保留旧节点引用，可刷新节点后再编译保存。

## 批处理入口

可选 `-ProjectionDebugRemoveAll` 参数：扫描 `/Game/PCG` 下的 Blueprint/PCG Graph，仅移除旧水印节点、保存变更并退出。
报告输出到 `Saved/Codex/projection_debug_cleanup_report.json`。
旧 `-ProjectionDebugApplyAll` 插入水印入口已移除。
普通启动不会执行全量扫描保存；本次没有调用该批处理入口。

已有 `BP_BiomeSplinePainter` 无需重建：编译并重启编辑器后，重新打开蓝图并保存，即可清除已序列化的旧水印节点。
兼容类型本身也已停止绘制，可在清理保存前避免继续显示旧水印。清理仅针对插件的旧水印类型，不按文字内容删除用户自己的注释或功能节点。

## 验证

按用户要求，编译和功能验证由用户执行。本轮扩展迁移没有运行编译、测试或编辑器加载验证。
此前首次插件迁移曾完成当时源码和 UHT 的编译，但链接 `BiomePCG.dll` 时被当前运行的 Unreal Editor 占用，未完成完整构建；该记录不代表本轮新增代码已通过编译。
请关闭编辑器后编译 `UE_MCPTestEditor / Win64 / Development`，再重新打开工程。
本次没有执行资产转换、碰撞预设修改或 ProjectionDebug 全量保存。
