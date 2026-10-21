# TerraceTools 使用与当前进度

更新日期：2026-10-21。对应代码版本：0.4.0，当前采用 Landscape 台地 + 用户 PCG 图沿线生成田埂。

- [完整规划](规划.zh-CN.md)
- [编译与测试清单](测试清单.zh-CN.md)
- [M1 地形整平使用与验收](M1地形整平.zh-CN.md)
- [M2 田埂与护坡使用及验收](M2田埂与护坡.zh-CN.md)
- [PCG 田埂接入与搭图说明](PCG田埂接入.zh-CN.md)
- [参考文章与后续方案](参考文章与方案调整.zh-CN.md)

## 当前交付

已编写 Runtime / Editor 模块、Terrace Group / Terrace Plot 原生 Actor、闭合样条默认边界、稳定 Plot ID、边界快照和二维几何工具，以及编辑器详情操作和视口辅助线。

用户已反馈 0.1.0 和 0.2.0 当前检验通过，未单独提供逐项记录。0.3.0 新增田埂/护坡网格、材质、碰撞及清理源码，**本次未编译或运行 UE/自动化测试，继续由用户执行**。

0.3.0 用户反馈台地可见、结构生成触发支撑高度错误。0.3.1 增加自动平坦支撑余量、同步护坡范围及具体高度诊断；用户随后反馈可以生成田埂护坡，未提供逐项验收记录。余量会扩大台地占地，详见 [M2 故障说明与操作](M2田埂与护坡.zh-CN.md)。

0.4.0 给 Plot 增加 TerracePlot / TerracePCGReady 标签、田面标高闭合输出样条 PCGBundSpline（组件标签 TerracePCGBundSpline）、可读取的 PCGData，以及 Plot/Group 的同步按钮。用户自行构建 PCG 图和田埂资产；本轮未创建 Graph 资产，未编译/运行测试，按用户要求先提交版本控制。地形抬高田埂留到后续试验。

Plot 参数位于靠前的 **梯田工具 / Terrace Tools · 参数**；操作位于 **梯田工具 / Terrace Tools · 操作**。主流程为更新地形、等待完成、同步 PCG 输入、在用户图中手动 Generate。原过程网格保留在 **梯田工具 / Terrace Tools · 网格实验**；主栏可清理旧网格。插件不自动清理或重建外部 PCG 结果。水面、台阶、垂直石墙、作物与烘焙尚未实现。

升级后完整编译重启，按 [PCG 接入文档](PCG田埂接入.zh-CN.md) 操作。PCG 必须按组件标签筛选输出样条，避免把原始 BoundarySpline 一并读入。

## 编译后首次使用

本次增加了新模块和 UCLASS。先保存工作，按测试清单完整编译并重新启动 UE；不以 Live Coding 成功作为新模块已正确加载的依据。确认插件浏览器中的 TerraceTools 已启用。

1. 在 Place Actors / 所有类中搜索 **Terrace Group** 并放入测试关卡。也可使用原生类 `TerraceGroupActor`。
2. 选中组，在 **梯田工具 / Terrace Tools** 分类中点击 **新增田块 / Add Plot**。
3. 从 Outliner 选择新增的 `TerracePlot`，选择其 `BoundarySpline`，用 UE 原生样条编辑方式调整控制点。默认边界为 20 m × 12 m 的闭合矩形，默认直线插值，可手动改曲线类型。
4. 修改 Plot 的 Actor 世界 Z 或 `Elevation Offset`，查看田面标高辅助线。
5. 点击 **校验边界 / Validate Boundaries**。组按钮校验所有显式成员；Plot 按钮只校验自己。结果显示在详情面板，并写入 Output Log 的 `LogTerraceTools`。
6. 需要整理控制点时点击 **控制点归平 / Flatten Control Points**，将各控制点局部 Z 设为 0。此操作不改变田面偏移，也不改地形。
7. 保存测试关卡，重开后检查边界、参数和 Plot ID。

仅验证边界不需要 Landscape。实际整平需要设置组的 `Target Landscape`，并按照 [M1 文档](M1地形整平.zh-CN.md) 在非 World Partition 普通测试关卡执行。

## 辅助线含义

- 青色闭合线：离散边界在目标世界标高上的投影。
- 橙色短竖线：田埂高度参考，每四个采样点绘制一次；不表示最终田埂断面或宽度。
- UE 原生样条线：实际可编辑的控制点和曲线，可能位于另一高度。
- PCGBundSpline：派生的 PCG 输入折线，与台地快照共用 XY，Z 为田面标高；编辑源仍为 BoundarySpline。

青色不代表校验通过，视口只进行轻量采样；需要主动点击校验。无效变换、未闭合或超预算时可能不显示青色线，点击校验查看原因。辅助线需要选中 Plot 或包含它的 Group；关闭 Game View（G）并确认编辑器辅助显示开启。

## 参数约定

| 参数 | 默认值 | 当前行为 |
| --- | --- | --- |
| Elevation Offset | 0 cm | 目标标高 = Plot Actor 世界 Z + 偏移 |
| Sample Spacing | 100 cm | 沿样条长度采样；每段起点必保留，最多 512 点 |
| Bund Height | 30 cm | 橙色参考线长度；M2 生成埂高 |
| Bund Width | 60 cm | M2 田埂向田内占用宽度 |
| Bund Material / Bank Material | 空 | M2 顶部及护坡/侧面材质 |
| Transition Width | 150 cm | 遮罩在自动平坦支撑余量之外的平滑过渡宽度 |
| Planting Yaw | 0° | 预留世界 XY 种植方向 |
| Row Spacing / Plant Spacing | 35 / 25 cm | 预留种植参数 |
| Edge Clearance | 80 cm | 预留边缘留白 |
| Seed | 1337 | 预留稳定随机参数 |

首版允许平移、Yaw 旋转、正向缩放。Actor 和边界组件需保持水平，Pitch/Roll 接近 0。Spline 控制点的 Z 不驱动田面起伏，所有边界均投影到世界 XY。边界采样是离散近似，急弯应增加采样密度并检查结果；超预算需增大 Sample Spacing 或简化边界。

边界允许凹形，不支持孔洞。自交、自触、重复点、短于 0.1 cm 的边、回折重叠边和面积小于 1 cm² 的区域会被拒绝。

组新增 `Terrain Raster Spacing`，默认 100 cm，控制地形遮罩采样间距，不改变 Landscape 顶点间距。它与 Plot 的 `Sample Spacing` 分别控制遮罩采样和轮廓采样，不能互相替代。

M2 组参数增加 Structure Sample Spacing、Generate Bank、Embed Depth、Surface Lift、Ground Height Tolerance、UV Tile Size 和 Create Structure Collision，含义见 M2 文档。

## 组与成员

`Plots` 是显式软引用列表。新增按钮同时创建田块并加入组；手工创建或复制的 Plot 需要手工加入列表。新增后若列表尚未刷新，重新选择组查看。

组的变换只决定新田块初始摆放位置，不会驱动已有成员移动。新增田块按世界 Y 方向每块错开 1500 cm，不会自动生成层高。组仍保持选中，便于连续添加和预览多块田。

普通复制 Plot / 粘贴会分配新 ID，保存重开保持 ID；PIE 复制保留 ID。复制 Group 只复制成员引用列表，不克隆田块，并清空生成输出引用；不能将此操作视为创建含独立田块的梯田组。

失效、未加载、跨世界或重复成员会导致整组快照失败，不返回部分田块。“校验边界”只检查边界；“更新地形”增加组内间距/过渡带、地形范围、标高范围和遮罩预算预检。跨组重叠、共享成员、Visibility 洞和碰撞就绪状态尚未检查。

## 源码入口

| 文件 | 职责 |
| --- | --- |
| `Plugins/TerraceTools/TerraceTools.uplugin` | Runtime 与 Editor 模块声明 |
| `Source/TerraceTools/Public/TerraceTypes.h` | 参数与边界快照 |
| `Source/TerraceTools/Public/TerracePlotActor.h` | 田块及稳定 ID |
| `Source/TerraceTools/Private/TerracePlotActor.cpp` | 样条采样与快照构建 |
| `Source/TerraceTools/Public/TerraceGroupActor.h` | 显式成员与目标地形引用 |
| `Source/TerraceTools/Private/TerraceGeometry.cpp` | 面积、距离、内外判断和自交校验 |
| `Source/TerraceToolsEditor/Private/TerraceToolsEditorModule.cpp` | 详情操作、事务、辅助线和模块注册 |
| `Source/TerraceToolsEditor/Private/Tests/TerraceGeometryTests.cpp` | 三组 UE 自动化测试 |
| `Source/TerraceToolsEditor/Private/TerraceTerrainRaster.cpp` | 覆盖遮罩、过渡带和计算预算 |
| `Source/TerraceToolsEditor/Private/TerraceTerrainBuilder.cpp` | 预检、Patch 层、纹理和事务替换 |
| `Source/TerraceToolsEditor/Private/TerraceTerrainOutputActor.h` | 编辑器专用输出及持久化遮罩 |
| `Source/TerraceToolsEditor/Private/Tests/TerraceTerrainTests.cpp` | 两组新增地形数学测试 |
| `Source/TerraceTools/Public/TerraceStructureActor.h` | 可保存到运行时的结构输出 |
| `Source/TerraceTools/Private/TerraceStructureGeometry.cpp` | 轮廓偏移、护坡布局、封口、UV 与法线 |
| `Source/TerraceToolsEditor/Private/TerraceStructureBuilder.cpp` | 地形状态/快照检查、碰撞采样、材质和输出替换 |
| `Source/TerraceToolsEditor/Private/Tests/TerraceStructureTests.cpp` | 三组结构几何测试 |

表中 `Source/...` 路径相对插件根目录。0.2.0 引入 LandscapePatch，0.3.0 引入 ProceduralMeshComponent；Runtime 模块仍不依赖 UnrealEd。尚未引入 PCG 与 Geometry Script。

## 下一步

先验证用户 PCG 图获取单块 Plot、资产轴心/偏移、角色行走与更新清理，再扩展多块田及各自参数关联。之后尝试地形抬高土埂作对比。共享边、台阶、垂直墙和网格烘焙仍属后续扩展。
