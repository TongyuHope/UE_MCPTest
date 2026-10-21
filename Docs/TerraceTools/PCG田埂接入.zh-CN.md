# PCG 沿线田埂接入

日期：2026-10-21。版本：0.4.0。用户已反馈 0.3.1 可以生成田埂/护坡，现改为先验证 PCG 资产沿线生成，之后再尝试地形抬高田埂。**本轮未编译、未运行 UE 或自动化测试，按用户要求先提交 Git。**

## 当前职责

TerraceTools 负责 Plot、台地整平和供 PCG 使用的样条/参数。田埂资产、分布、材质及碰撞由用户自己的 PCG 图负责。本轮没有创建 PCG Graph 资产或自动挂载 PCG Component，也没有实现沿线抬高地形田埂。

原过程网格生成保留在 Group 的“梯田工具 / Terrace Tools · 网格实验”分类，按钮改为“生成旧版过程网格”。主操作中保留“清理旧版网格”；它不清理用户 PCG 输出。更新地形仍会清理本组旧过程网格，但不会自动清理或重新生成用户的外部 PCG 结果。

本轮不新增 PCG C++ 模块依赖，输出使用标准 USplineComponent 和反射属性。搭图时请在项目启用引擎 PCG 插件。

## 首次操作

1. 保存工作，关闭 UE，完整编译并重启。新增原生组件与反射结构，不依赖 Live Coding 验收旧 Plot 迁移。
2. 选择原 Group，点击“清理旧版网格”，避免旧碰撞或显示与 PCG 资产重叠。
3. 编辑 Plot 的 BoundarySpline；设置 Elevation Offset、Bund Width、Bund Height 等。保持 Enable PCG Output 开启。
4. 选择 Group，点击“更新地形”，等待 Landscape 及碰撞更新。沿用当前普通非 World Partition 测试关卡限制。
5. 点击“同步 PCG 输入 / Refresh PCG Inputs”。选择 Plot 时只同步自己；选择 Group 时同步已加载成员，报告有效、关闭和无效数量。
6. 先只用一块 Plot 配合 PCG Volume，按下一节取数并查看采样点，确认高度后再放田埂资产。
7. PCG Generate 后检查闭环、转角、资产底部、角色行走和碰撞。地形再次改变时，主动清理/重新 Generate PCG。

同步输入只生成几何数据，不证明台地已经更新，也不保证碰撞就绪。即使只同步输入成功，仍须先完成地形步骤再验收 PCG 田埂。

## 标签和样条约定

| 对象 | 标签 / 名称 | 含义 |
| --- | --- | --- |
| Plot Actor Tags | TerracePlot | 所有 Plot 的固定类别标签；不覆盖用户额外标签 |
| Plot Actor Tags | TerracePCGReady | 此 Plot 当前边界有效、输出开启且已同步；不表示地形/碰撞已就绪 |
| 原编辑组件 | BoundarySpline；Component Tag=TerraceBoundary | 编辑源，可有不同的控制点 Z；不用于本轮田埂生成 |
| 派生输出组件 | PCGBundSpline；Component Tag=TerracePCGBundSpline | 在田面世界标高上的闭合边界样条，用它生成田埂 |

两个 USplineComponent 位于同一个 Plot Actor。**PCG 必须按组件标签 TerracePCGBundSpline 筛选**；只按 Actor 标签或 SplineComponent 类取数，可能同时读到原始样条，导致两圈资产或高度错误。

输出与地形使用同一份边界采样快照，为线性闭合折线，不直接保留原曲线的曲率。Sample Spacing 控制原曲线离散精度；降低它会增加输出点数，仍受 512 点预算约束。

输出所有点的世界 Z = Actor 世界 Z + Elevation Offset，忽略原控制点 Z。统一为从世界 +Z 看 XY 数学逆时针绕序。组件采用单位世界缩放，源 Plot 的缩放已体现在采样后的世界坐标中，不应再给资产重复套用 Plot Scale。

## 最小 PCG 图

建议先使用：

```text
Get Spline Data → Spline Sampler → Transform Points → Static Mesh Spawner
```

Get Spline Data 的本机 UE 5.8 取数设置：

- Actor Filter：All World Actors。
- Actor Selection：By Tag。
- Actor Selection Tag：TerracePCGReady。
- Select Multiple：验证多块田时开启，否则可能只取一个 Actor。
- Component Selection：By Tag。
- Component Selection Tag：TerracePCGBundSpline。
- 如果改用 Get Actor Data，Mode 使用 Parse Actor Components，并加同样的组件筛选，只保留 Spline 数据。

初次验证可只放一个 Plot；或给该 Plot 额外加 Actor Tag，例如 TerraceBund_Test，用它替换 Actor Selection Tag。多个 Group 在同一世界时，可以给各组的 Plot 手工添加不同的类别标签，控制不同 PCG 图的取数范围；固定 TerracePlot 标签不是组 ID。

PCG Volume 覆盖田块及资产范围。首次诊断可关闭 Must Overlap Self 来排除范围筛选问题，验证完成后再收紧。只查询已加载 Plot，不自动解决 World Partition 加载/分区生成。

Spline Sampler 使用 On Spline、Distance，Distance Increment 根据资产沿长度方向的实际长度配置；首次关闭随机位置/旋转/缩放。先 Debug 采样点，确认只有一圈、标高正确、切线方向正确，再添加资产。

若闭环长度不能整除模块长度，可尝试样条采样的距离拟合选项，再检查模块实际长度及末段间隙。普通 Static Mesh Spawner 只放置刚性资产，不会将它们自动弯曲或处理转角；短模块、转角模块、适当重叠或后续 Spline Mesh 都是可选资产方案。

## 宽度、轴心和标高

PCGBundSpline 仍代表**田埂外沿**，并非中心线。输出世界 Z 为田面基准，不含 Bund Height，也没有之前过程网格的 Surface Lift。

- 资产轴心在田埂宽度中心：向田内偏移 BundCenterInset（Bund Width / 2）。输出逆时针，沿切线 T 的田内方向可按 Up × T 计算。若使用采样点局部 Y 位移，先 Debug 确认方向；不要依赖未经检查的资产朝向约定。
- 资产轴心在外沿、底部：可直接沿输出边界放置，使资产宽度朝向田内，资产底面贴田面。
- 资产轴心在几何中心：Z 需要再加资产实际半高；如果尺寸已缩放，按缩放后高度计算。
- Bund Width / Height 是可读的设计参数，不会自动改变 Spawner 中的资产尺寸或碰撞；图中自行决定是否据此缩放。
- 只在切线方向逐点偏移，不等于已构建严格的偏移多边形。急弯、窄部和角点仍需处理模块间隙/交叠。

一般不需要再把样条采样点投射到地形：输出已经是田面标高。若另有贴地需求，只采目标 Landscape，避免投到旧田埂或 PCG 自己生成的资产，也不要让坡面法线随机倾斜原本应水平的田埂。

田埂资产应有可用碰撞；在 PCG 生成组件设置中选择需要的碰撞 Profile。重点检查模块连接处和闭环接缝，视觉连续不等于胶囊能平稳行走。

## 参数取数

Plot 暴露只读平坦结构属性 PCGData，可通过 Get Actor Property 读取 Property Name=PCGData。包含：

| 字段 | 含义 |
| --- | --- |
| bValid | 当前输出是否有效；关闭/非法输入时为 false |
| PlotId | 稳定 Plot ID 的字符串；普通复制产生新 ID |
| WorldElevation | 田面世界标高，cm |
| BundWidth / BundHeight | 田埂设计宽度/高度，cm |
| BundCenterInset | 建议中心线向田内偏移量，cm |
| Seed | Plot 种子 |

Get Actor Property 与样条节点使用同样的 Actor 筛选。第一次搭图只有一块田时，可直接验证这些数值。UE 5.8 提供 Struct Extractor Behavior，按需要提取 PCGData 的直接成员，实际查看输出属性名。

多块 Plot 的参数输出和样条输出**不能假定数组顺序一致**，也不能把全部参数与全部样条直接进行无区分的笛卡尔组合。建议第一版使用相同尺寸的资产；之后按源 Actor 引用分别处理每块 Plot。Get Actor Property 可以输出 Actor Reference，结合按对象路径读取属性或逐 Actor 子图完成对应关系。PlotId 用于识别，不是本轮已自动注入每个采样点的元数据。

## 同步和失效处理

Plot 构造、编辑器加载/注册、Actor 参数修改、移动结束及 Actor Undo 后尝试刷新；Editor 模块监听原 BoundarySpline 的非交互属性变更。更新地形成功提交时同步该组输入，不自动 Generate PCG。

对脚本直接修改组件、只撤销某个组件、蓝图自定义重建或编辑过程中不确定的情况，使用“同步 PCG 输入”作为明确同步入口。运行时通过 Blueprint 改 Settings 后，也需显式调用 RefreshPCGOutput；本轮没有 Tick 或运行时地形/PCG 自动更新链路。

自交、未闭合、无效参数、超采样预算时清空派生样条、清除 TerracePCGReady 标签并设置 PCGData.bValid=false，不让旧输入冒充当前有效结果。关闭 Enable PCG Output 同样清空。同步按钮不会直接删除已经生成的外部 PCG 网格，仍需用户执行 PCG Cleanup/Generate。

## 人工验收（用户执行）

| 编号 | 操作 | 预期 |
| --- | --- | --- |
| PCG-01 | 旧关卡完整编译重启，选择原 Plot 同步 | 出现 PCGBundSpline、固定标签和 PCGData；无第二个手摆 Actor |
| PCG-02 | Actor Z=1000，Elevation Offset=200，原控制点 Z 有变化 | 输出所有世界 Z=1200；原样条不被归平或覆盖 |
| PCG-03 | Plot Yaw/非等比正向缩放，BoundarySpline 有相对变换 | 输出 XY 与原采样边界一致，世界 Z 一致，输出组件世界 Scale=1 |
| PCG-04 | 正反两种绕序，矩形与凹形 | 输出均闭合、数学 CCW；PCG 只读取一个输出组件 |
| PCG-05 | 自交/断开闭环/非法参数，或关闭输出 | 无派生点、无 Ready 标签，bValid=false；Cleanup/Generate 后无旧资产 |
| PCG-06 | 移动控制点、修改标高、同步组、Undo/Redo | 检查派生输入一致；组件级 Undo 后必要时显式同步，不把旧生成资产当成当前结果 |
| PCG-07 | 复制 Plot、保存重开、PIE | 普通复制的新 ID 与 PCGData 对应；重开输入和 PCG 资产正确；PIE 无逐帧重新生成 |
| PCG-08 | 两块 Plot 不同高度、宽度，Select Multiple 开启 | 两圈各自标高正确；按 Actor 关联各自参数，不串用配置 |
| PCG-09 | 先生成旧过程网格，再更新地形和 PCG | 旧网格清理，外部 PCG 由用户更新，无两份重叠碰撞 |
| PCG-10 | 沿直边、转角和闭环接缝行走，保存重开 | 碰撞连续，角色无悬空、穿透或卡角；打包/Cook 仍需后续目标平台验证 |

前述九组几何/地形/结构 Automation 保留，不能代替 PCG 图的人工验收。本轮不为普通接口同步新增镜像测试，重点由上表验证编辑器生命周期、取数与行走效果。

## 依据与下一步

接口设置已对照本机 UE 5.8.3 的 PCGActorSelector.h、PCGDataFromActor.h、PCGGetActorProperty.h 与 PCGSplineSampler.h。概念说明可查 [Epic PCG 节点参考](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)。

开发沿用 Unreal Engine C++ Pro 的 UObject 引用、模块隔离和避免 Tick 原则，本轮未复制其示例源码。先收集用户 PCG 资产和行走反馈，再在同一 Plot 数据上增加地形土埂试验；不在本轮改变台地后端的支撑余量策略。
