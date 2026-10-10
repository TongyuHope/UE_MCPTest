# TYWaterfallTools 实施路线图

> 当前工程：UE 5.8  
> 当前插件：`TYWaterfallTools`  
> 学习参考：`SHADERSOURCE Waterfall Tool 2`（目标插件声明为 UE 5.7）  
> 文档目标：作为后续开发的唯一入口，记录架构、阶段、文件映射、验收标准和进度。

## 1. 使用方式

每次继续开发时，先执行以下流程：

1. 阅读本文档的“当前状态”和当前阶段。
2. 运行 `git status --short`，确认工作区变化。
3. 只阅读当前阶段列出的本项目文件和参考文件。
4. 一次只完成一个可验证的小任务。
5. 编译并在编辑器中完成对应验收。
6. 更新本文档中的复选框、决策记录和已知问题。
7. 单独提交该阶段，避免把多个功能混在一个提交中。

后续开发不需要再次遍历整个参考插件。只有当当前阶段的说明不足时，才读取该阶段列出的参考源码。

## 2. 当前状态

### 已有内容

- [x] 插件基础目录和 `.uplugin`
- [x] UE Editor Mode 模板代码
- [x] Editor Mode Toolkit、Commands 和示例 Interactive Tool
- [x] Git 仓库初始化
- [x] Runtime 模块
- [ ] 瀑布 Actor
- [ ] 路径模拟
- [ ] 动态网格（阶段 4 已实现并通过编译，等待编辑器验收）
- [ ] 自定义瀑布编辑界面
- [ ] Niagara / Audio
- [ ] 静态网格烘焙

### 当前插件限制

当前 `TYWaterfallTools.uplugin` 只有一个 `Editor` 模块。因此：

- 该模块中的 Actor 和 Component 不能作为正常运行时功能打包。
- 编辑器 UI、生成算法和最终运行时数据会混在一起。
- 后续功能越多，编译依赖和维护成本越高。

第一阶段必须先增加 Runtime 模块。保留现有 `TYWaterfallTools` 作为 Editor 模块，可以避免立即重命名现有模板文件。

## 3. 目标范围

### 第一版必须实现

- 在关卡中放置 `ATYWaterfallActor`。
- 使用顶部样条定义瀑布的宽度、起点和初始方向。
- 从顶部样条生成多条水流路径。
- 路径受重力、初速度、阻力和场景碰撞影响。
- 根据路径生成可预览的动态带状网格。
- 在自定义 Editor Mode 中生成、清除和重新生成。
- 支持材质参数需要的 UV 和顶点颜色数据。
- 能将动态网格烘焙为静态网格资产。

### 第一版暂不追求

- 真实流体模拟。
- 完全复刻参考插件的全部 UI。
- 一开始就实现四种网格模式。
- 一开始就支持每个路径点放置任意 FX。
- 与参考插件完全相同的参数、算法和资源。

### 版权边界

参考插件源码带有版权声明。本项目学习其系统边界和功能思路，但应使用自己的：

- 类名和模块名；
- 算法实现；
- Slate 布局；
- 材质、Niagara、图标和其他内容资源；
- 文档和用户文案。

不要直接复制参考插件的大段实现或内容资源。

## 4. 推荐架构

```text
TYWaterfallTools.uplugin
├── TYWaterfallToolsRuntime       Type=Runtime
│   ├── Actors
│   │   └── ATYWaterfallActor
│   ├── Components
│   │   ├── UTYWaterfallPathComponent
│   │   ├── UTYWaterfallMeshComponent
│   │   ├── UTYWaterfallVFXComponent
│   │   └── UTYWaterfallSettingsComponent
│   ├── Generation
│   │   ├── FTYWaterfallPathBuilder
│   │   ├── FTYWaterfallMeshBuilder
│   │   └── FTYWaterfallBakeBuilder
│   └── Data
│       ├── FTYWaterfallSimPoint
│       ├── FTYWaterfallSample
│       └── ETYWaterfallPointState
└── TYWaterfallTools              Type=Editor（保留现有模块名）
    ├── EditorMode
    ├── Details
    ├── Slate
    ├── Visualizers
    └── Commands
```

### 数据流

```text
Editor Mode / Details Panel
            |
            v
UTYWaterfallSettingsComponent
            |
            v
FTYWaterfallPathBuilder
            |
            v
UTYWaterfallPathComponent[]
            |
            v
FTYWaterfallMeshBuilder
            |
            v
UTYWaterfallMeshComponent
        |                 |
        v                 v
Niagara / Audio     Static Mesh Bake
```

### 核心职责

`ATYWaterfallActor`

- 拥有所有默认组件。
- 提供路径、网格和 FX 的查询接口。
- 协调 Builder，但不直接实现复杂算法。
- 编辑器生成时允许 Tick；正常运行时默认不 Tick。

`UTYWaterfallSettingsComponent`

- 保存用户可编辑参数。
- 验证参数范围。
- 向 UI 提供生成、取消和清理入口。
- 不直接编写 Slate UI。

`UTYWaterfallPathComponent`

- 保存一条模拟路径及其采样缓存。
- 负责单条路径的模拟、重采样和查询。
- 默认关闭组件 Tick，由 Builder 统一调度。

`FTYWaterfallPathBuilder`

- 创建、排队和分帧处理路径。
- 管理开始、取消、成功和失败状态。
- 避免一次生成阻塞编辑器。

`FTYWaterfallMeshBuilder`

- 消费路径采样数据。
- 构建顶点、三角形、法线、UV 和顶点颜色。
- 第一版只实现 Per-Path 带状网格。

`TYWaterfallTools` Editor 模块

- Editor Mode、Toolkit、菜单、Details 定制和可视化器。
- 不承担运行时数据和核心生成算法。

## 5. 阶段路线图

## 阶段 0：建立基线和模块边界

### 目标

在不改变现有 Editor Mode 行为的前提下，增加 Runtime 模块，并确保插件可以编译和加载。

### 实施步骤

- [x] 在 `.uplugin` 中启用 `CanContainContent`。
- [x] 添加 `TYWaterfallToolsRuntime` Runtime 模块。
- [x] 保留现有 `TYWaterfallTools` 为 Editor 模块。
- [x] Editor 模块依赖 `TYWaterfallToolsRuntime`。
- [x] Runtime 模块初始只依赖 `Core`、`CoreUObject` 和 `Engine`。
- [x] 确认 Editor 模块不会被非 Editor Target 加载。
- [x] 添加 Runtime 模块启动/关闭日志。
- [x] 完成一次 Development Editor 编译。

### 预计文件

```text
TYWaterfallTools.uplugin
Source/TYWaterfallTools/TYWaterfallTools.Build.cs
Source/TYWaterfallToolsRuntime/TYWaterfallToolsRuntime.Build.cs
Source/TYWaterfallToolsRuntime/Public/TYWaterfallToolsRuntimeModule.h
Source/TYWaterfallToolsRuntime/Private/TYWaterfallToolsRuntimeModule.cpp
```

### 验收标准

- 插件可以在 UE 5.8 编辑器中启用。
- 现有 Editor Mode 仍然出现。
- Output Log 中没有模块加载错误。
- Runtime 模块可以被游戏模块引用。

## 阶段 1：瀑布 Actor 骨架

### 目标

创建可放入关卡的 Actor，建立后续路径、网格和 FX 的组件所有权。

### 实施步骤

- [x] 创建 `ATYWaterfallActor`。
- [x] 添加 `USceneComponent` 根组件。
- [x] 添加顶部 `USplineComponent`。
- [x] 设置两个默认样条点，形成可编辑的瀑布顶部宽度。
- [x] 添加用于最终烘焙结果的 `UStaticMeshComponent`。
- [x] 添加 Editor-only 的终止平面可视化组件。
- [x] 使用 `WITH_EDITORONLY_DATA` 隔离纯编辑器组件。
- [x] 默认关闭运行时 Tick。
- [x] 在编辑器中验证移动、旋转、复制和保存 Actor。

### 建议接口

```cpp
UCLASS()
class TYWATERFALLTOOLSRUNTIME_API ATYWaterfallActor : public AActor
{
    GENERATED_BODY()

public:
    ATYWaterfallActor();

    USplineComponent* GetTopSpline() const;
    UStaticMeshComponent* GetBakedMeshComponent() const;
};
```

### UObject 约束

- 所有持有的 `UObject*` / Component 指针必须通过 `UPROPERTY` 跟踪。
- 对外默认返回只读访问，避免任意对象改写内部状态。
- 只有确实需要蓝图修改的属性才使用 `BlueprintReadWrite`。

### 验收标准

- Actor 能从 Place Actors 或 Content Browser 放入关卡。
- 顶部样条可通过视口编辑。
- 保存并重开关卡后组件状态不丢失。
- PIE 时 Actor 不执行无意义 Tick。

## 阶段 2：单条路径模拟 MVP

### 目标

从顶部样条的中点生成一条路径，先证明基础运动和碰撞链路正确。

### 数据类型

```cpp
UENUM()
enum class ETYWaterfallPointState : uint8
{
    Start,
    Airborne,
    Sliding,
    Stopped,
    Terminated
};

USTRUCT()
struct FTYWaterfallSimPoint
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Position = FVector::ZeroVector;

    UPROPERTY()
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY()
    FVector HitNormal = FVector::UpVector;

    UPROPERTY()
    ETYWaterfallPointState State = ETYWaterfallPointState::Start;
};
```

### 实施步骤

- [x] 创建 `UTYWaterfallPathComponent : USplineComponent`。
- [x] 禁用 Path Component 自身 Tick。
- [x] 添加模拟点数组。
- [x] 从 Top Spline 中点取得位置和方向。
- [x] 实现初速度、重力和线性阻力。
- [x] 使用固定时间步长，保证结果可复现。
- [x] 使用 Sweep/Line Trace 检查场景碰撞。
- [x] 碰撞后将速度投影到碰撞平面，实现基础滑动。
- [x] 到达终止高度或最大步数时结束。
- [x] 将模拟点写入 Spline Points。
- [x] 实现 Clear Path。

### 算法基线

第一版使用半隐式欧拉或中点积分即可：

```text
Velocity += Gravity * DeltaTime
Velocity *= DragFactor
NextPosition = Position + Velocity * DeltaTime
Trace(Position, NextPosition)
```

先保证行为稳定和容易调试，不提前实现路径间排斥、湍流传播或搜索树。

### 验收标准

- 点击生成后出现一条可见 Spline。
- 无碰撞时路径受重力下落。
- 遇到斜面时沿表面滑动。
- 使用相同参数重复生成结果一致。
- 最大步数可以防止无限模拟。

## 阶段 3：多路径和分帧任务

### 目标

沿 Top Spline 生成多条路径，并将计算分散到编辑器多帧。

### 实施步骤

- [x] 创建 `UTYWaterfallSettingsComponent`。
- [x] 添加 `NumPaths`、`InitialSpeed`、`Gravity`、`Drag`、`FixedDeltaTime`、`MaxSteps`。
- [x] 添加 Seed 和 `FRandomStream`，禁止核心生成使用全局 `FMath::Rand`。
- [x] 创建 `FTYWaterfallPathBuilder`。
- [x] 将任务状态定义为 Idle、Generating、Cancelling、Completed、Failed。
- [x] 按 Top Spline 参数均匀创建多个 Path Component。
- [x] 每帧限制模拟预算，避免冻结编辑器。
- [x] 添加取消操作。
- [x] 添加 Undo/Redo 的 `FScopedTransaction`。
- [x] 完成生成后调用 `Modify()`、`MarkPackageDirty()`。
- [x] 删除路径时同步清理动态网格和 FX（当前尚无网格/FX，后续 Builder 将接入同一清理入口）。

### 生命周期要求

- Builder 若是普通 `F` 结构体，不拥有 UObject 生命周期。
- Builder 内部不要长期保存未经跟踪且可能失效的裸 UObject 指针。
- 跨帧引用优先使用 `TWeakObjectPtr`，每次使用前检查 `IsValid()`。
- Editor Mode 退出、Actor 删除或地图切换时必须取消任务。

### 验收标准

- [x] 可以生成 1、8、32 条路径。
- [x] 生成期间编辑器视口仍可响应。
- [x] Cancel 后不会留下半注册组件。
- [x] Undo 可以撤销生成，Redo 可以恢复或重新执行预期状态。

## 阶段 4：Per-Path 动态带状网格

### 目标

把每条路径转换为最简单、最容易验证的带状动态网格。

### 实施步骤

- [x] 采用底层 `GeometryFramework` / `GeometryCore`，本阶段无需启用高层 `GeometryScripting` 插件。
- [x] Runtime 模块添加 `GeometryFramework` 和 `GeometryCore` 依赖。
- [x] 创建 `UTYWaterfallMeshComponent : UDynamicMeshComponent`。
- [x] 沿每条路径按距离均匀采样。
- [x] 计算每个采样点的切线和横向向量。
- [x] 每个采样点生成左、右两个顶点。
- [x] 相邻采样点生成两个三角形。
- [x] 生成法线和 UV0，由 Dynamic Mesh 自动计算切线。
- [x] 将路径累计距离写入 UV 的 V 分量。
- [x] 支持 Ribbon Width、Mesh Sample Spacing、横向 Subdivisions 和 Base UV Scale 参数。
- [x] 实现 Clear Dynamic Mesh，并在重新生成/清除路径时同步清理网格。

### 三角形连接

```text
L0 ---- L1
|    /  |
|  /    |
R0 ---- R1

Triangle A: L0, L1, R0
Triangle B: R0, L1, R1
```

使用 Unreal 的顺时针正面绕序，使单面材质从瀑布上方可见；法线同样朝向水面上方。

### 验收标准

- 每条路径都生成连续网格。
- 网格没有明显断裂、翻面和 NaN 顶点。
- 调整宽度不会改变路径中心。
- 材质 UV 沿水流方向连续。
- 清理路径时网格同步清理。

## 阶段 5：采样数据和材质通道

### 目标

建立稳定的路径采样数据模型，为材质和 Niagara 提供输入。

### 建议采样数据

```cpp
USTRUCT()
struct FTYWaterfallSample
{
    GENERATED_BODY()

    FVector Position = FVector::ZeroVector;
    FVector Tangent = FVector::ForwardVector;
    FVector Normal = FVector::UpVector;
    FVector Velocity = FVector::ZeroVector;
    float Distance = 0.0f;
    float NormalizedDistance = 0.0f;
    float Speed = 0.0f;
    float Flow = 0.0f;
    float Impact = 0.0f;
    float Turbulence = 0.0f;
    float RandomValue = 0.0f;
};
```

### 实施步骤

- [x] 按距离重采样，不直接使用不均匀的模拟点作为顶点。
- [x] 缓存 Position、Tangent、Normal、Velocity 和 Distance。
- [x] 基于碰撞冲击和方向变化计算简化 Turbulence。
- [x] 定义固定材质通道协议。
- [x] UV0 保存归一化的横向位置和沿路径位置。
- [x] UV1 保存乘以 Base UV Scale 的横向与纵向厘米距离。
- [x] UV2 保存速度（cm/s）和累计流动时间（秒）。
- [x] UV3 保存路径固定随机种子和顶部样条归一化位置。
- [x] Vertex Color 按参考材质协议保存编码流向和湍流。
- [x] 在文档中锁定通道语义，后续不随意更改。

### 阶段 5 材质通道协议

| 通道 | X / R | Y / G | Z / B | W / A |
| --- | --- | --- | --- | --- |
| UV0 | 横向归一化坐标 × Base UV Scale.X；Splash 为径向 U | 沿路径归一化距离 × Base UV Scale.Y；Splash 为径向 V | - | - |
| UV1 | 横向厘米距离 × Base UV Scale.X；Singular 为顶部样条距离 | 沿路径厘米距离 × Base UV Scale.Y；Splash 为局部平面坐标 | - | - |
| UV2 | 速度（cm/s） | 累计 Flow × Base UV Scale.Y；Splash 不缩放 | - | - |
| UV3 | 路径固定随机种子 0 到 1 | 顶部样条归一化位置 0 到 1 | - | - |
| Vertex Color | 流向 X 从 -1..1 映射到 0..1 | 流向 Y 映射到 0..1 | 流向 Z 映射到 0..1 | 湍流 |

`FTYWaterfallSample` 是 mesh、材质和 Niagara 之间的公共数据协议。UV3.X 来自路径种子，同一路径上保持不变。UV2.Y 按参考插件的 `SampleLength / Speed` 逐点累积；Vertex Color RGB 保存流向，A 保存湍流。

此前阶段 5 的三通道协议已被四通道协议取代：原来 UV0.V 的纹理重复坐标移到 UV1.Y，UV1 和 UV2 的语义也改变。使用旧协议的材质需要同步调整 `TextureCoordinate` 通道索引与缩放。

### 验收标准

- Debug Material 能显示距离渐变。
- Debug Material 能显示速度或湍流数据。
- 同一 Seed 的随机通道稳定。
- 重采样间距变化时材质流向仍连续。

## 阶段 6：Editor Mode 最小工作流

### 目标

让用户不依赖 Actor 默认 Details 面板即可完成核心操作。

### 实施步骤

- [x] 从 Editor Mode 入口移除当前模板生成的示例 Tool。
- [x] Editor Mode 进入时识别当前选择的 `ATYWaterfallActor`。
- [x] 未选择瀑布时显示选择提示。
- [x] 添加 Generate Paths 按钮。
- [x] 添加 Generate Mesh 按钮。
- [x] 添加 Clear All 按钮。
- [x] 添加 Cancel 按钮和生成进度状态。
- [x] 生成期间禁用会破坏状态的控件。
- [x] Actor 选择变化时刷新 Toolkit。
- [x] Editor Mode 退出时取消未完成任务并释放 Actor 弱引用。

### UI 原则

- 第一版不复制参考插件的大型多 Tab UI。
- 只显示当前工作流必需的控件。
- 参数继续使用 Details View，命令放在工具栏区域。
- 所有长任务都要提供取消路径。

### 验收标准

- 选择不同瀑布 Actor 时面板正确切换。
- 删除当前 Actor 后面板不会访问失效对象。
- 生成期间按钮状态正确。
- 退出 Editor Mode 后没有残留绑定或崩溃。

## 阶段 7：Details 定制和路径可视化

### 目标

提升参数编辑、路径选择和调试效率。

### 实施步骤

- [x] 注册 `ATYWaterfallActor` Details Customization。
- [x] 注册 `UTYWaterfallSettingsComponent` Details Customization。
- [x] 已有参数按 Paths、Simulation、Mesh、Material、Performance、Debug 分组；FX、Bake 留到对应阶段。
- [x] 路径生成期间禁用 Settings 编辑，完成或取消后恢复。
- [x] 复用 Spline Component 的路径 Debug 渲染，无需额外 Path Visualizer。
- [x] 每条路径使用稳定且不同的颜色，便于辨认路径分布和交叉。
- [x] 添加显示 Dynamic Mesh 顶点和三角形线框的 Debug 开关。
- [x] 在模块 Shutdown 时对称注销全部 Details 和 Visualizer 注册。

### 验收标准

- Details 面板刷新不丢失当前对象。
- Hot Reload 或模块关闭时不留下无效注册。
- Debug 绘制有明确开关和持续时间。
- Shipping/非 Editor 构建不包含编辑器 UI 依赖。

## 阶段 8：扩展网格模式

### 目标

在 Per-Path 稳定之后扩展其他表现形式。

### 推荐顺序

- [x] Cross：在 Per Path 基础上为每条路径追加垂直相交的带状平面。
- [x] Splash：在路径终点生成带前后半径的多环水花网格。
- [x] Singular：把多条路径重映射并连接成连续水幕。

### 当前组合和参数

- Singular、Per-Path、Cross 和 Splash 分别提供独立开关；`Generate Mesh` 只追加已勾选的类型，并用完整结果替换当前 Dynamic Mesh。
- 四个开关全部关闭时会清空旧 Dynamic Mesh，不保留上一次生成结果。
- Singular 至少需要两条有效路径；路径先按顶部起点在宽度轴上的投影排序，再按归一化距离重映射到统一行数并连接相邻路径。
- Singular 的 UV0 使用横向路径比例和纵向归一化距离，UV1 使用顶部样条厘米距离和纵向厘米距离，并乘以 Base UV Scale。
- Per Path 使用 Ribbon Width；Cross 使用独立的 Cross Width，并绕路径切线旋转 90 度。
- Singular、Per Path 和 Cross 复用 Mesh Sample Spacing、Base UV Scale 和阶段 5 材质通道。
- Splash 参考 WaterfallTools 的闭合轮廓挤出：终点宽度线、前后端帽和前后半径方向组成连续外轮廓，再沿轮廓方向分环挤出。
- Splash Front Radius 和 Back Radius 分别控制水流前方、后方的延伸距离。
- Splash Radial Segments 控制前后端帽的旋转细分，Splash Rings 控制轮廓向外挤出的环数。
- Splash UV0 使用参考外轮廓的真实累计周长；UV1 使用落地平面的局部世界距离，UV2、UV3 和 Vertex Color 遵循四通道协议。

### Singular 边界

连续水幕要求相邻路径拥有兼容的采样数量和拓扑。需要处理：

- 路径长度不同；
- 路径交叉；
- 路径提前终止；
- 相邻路径点错位；
- 三角形翻转；
- UV 在路径边界不连续。

当前实现解决了路径长度、采样数量和终止位置不同造成的索引不兼容。路径身份在顶部排序后保持不变，
因此结果稳定且易于理解；但当路径在下落过程中严重交叉时，连续水幕仍可能自交。参考插件中的动态点排序
属于实验性策略，本阶段不移植，未来可作为可选的高级重映射模式增加。

### 验收标准

- 四种模式单独启用时只生成对应几何，任意组合启用时追加到同一个 Dynamic Mesh。
- 全部关闭后重新生成会清空旧网格并隐藏 Dynamic Mesh Component。
- 重新生成会整体替换旧网格，不会重复累积几何。
- 四类几何复用线框 Debug 和稳定材质数据通道，但分别使用固定材质槽与 Triangle Material ID。
- 材质槽固定为 `0 = Singular`、`1 = Per-Path`、`2 = Cross`、`3 = Splash`；关闭模式不会压缩或改变其余槽位。
- Singular 可连接长度不同的路径，至少两条有效路径时生成，正面绕序与 Per-Path 一致。
- Splash 正面朝向终点采样法线，参数变化不会产生越界或退化索引。

## 阶段 9：Niagara

### 目标

基于路径数据布置顶部、中段和底部 Niagara 效果。Audio 延期到未来阶段。

### 实施步骤

- [x] 定义统一的 `FTYWaterfallFXPointData`。
- [x] 添加顶部、底部和中段数据查询接口。
- [x] 创建 `UTYWaterfallVFXComponent : UNiagaraComponent`。
- [x] 通过 Niagara Array Data Interface 传递点数据。
- [x] Actor 持有顶部、中段、底部三个 Niagara Component。
- [x] 路径生成完成后自动刷新 FX 数据。
- [x] 路径清除后清空数组并停用 Niagara。
- [ ] Audio Component 和空间布局（延期）。

### Niagara 用户参数协议

三个 Niagara System 使用相同的用户参数；数组相同索引共同描述一个 FX 点：

| 参数 | Niagara 类型 | 语义 |
| --- | --- | --- |
| `User.TY_PositionArray` | Vector Array | 组件局部空间位置 |
| `User.TY_ForwardArray` | Vector Array | 局部空间流向 |
| `User.TY_UpArray` | Vector Array | 局部空间上方向/表面法线 |
| `User.TY_RightArray` | Vector Array | 局部空间右方向 |

Top 每条路径提供首点，Bottom 每条路径提供末点。Middle 按 Niagara Sample Spacing
重新采样并排除首尾点，避免与 Top 和 Bottom 重复。三个 Niagara System 由 Settings
中的软引用配置，生成或点击 Refresh FX 时才同步加载并赋给组件。

位置和方向在写入组件时转换并保存为组件局部空间。Niagara Emitter 需要启用 Local Space，
再按 Execution Index 从各数组读取相同索引；Spawn Count 可直接读取 Position Array 的长度。
这样 Actor 移动和旋转后无需重新上传数据。

### 性能要求

- 不在每帧重新上传不变的 Niagara 数组。
- 只在路径或 FX 设置变化时刷新。
- FX 资产使用软引用，避免插件加载时强制加载大型资源。

### 验收标准

- 顶部、底部和中段效果位置正确。
- Actor 移动和旋转后局部/世界空间转换正确。
- 清除路径不会留下悬空 FX 组件。
- 没有指定 Niagara System 时组件保持停用，但点数据仍能正确生成。

## 阶段 10：静态网格烘焙

### 目标

将动态网格保存为可打包的 Static Mesh 资产。

### 实施步骤

- [x] Editor 模块添加 Geometry Scripting 和 Asset Tools 相关依赖。
- [x] 使用阶段 8 已合并的 Dynamic Mesh 作为唯一烘焙源。
- [x] 通过 Content Browser 保存对话框选择输出目录和资产名。
- [x] 创建或覆盖 Static Mesh 前给出明确提示。
- [x] 保存四个固定材质槽名称和材质引用。
- [x] 设置 `BakedMeshComponent`。
- [x] 提供动态/烘焙网格显示切换。
- [x] 标记资产包和关卡包为 Dirty。
- [ ] 验证烘焙资产重启编辑器后仍有效。

### 验收标准

- 可以在 Content Browser 中得到 Static Mesh。
- Static Mesh 包含正确的材质槽和 UV。
- 打包游戏不依赖 Editor-only Dynamic Mesh 数据。
- 重复烘焙不会静默破坏已有资产。

## 阶段 11：测试、性能和发布

### 自动化测试候选

- [ ] 固定 Seed 生成结果一致。
- [ ] 路径数量和组件数量一致。
- [ ] 所有路径点数在合法范围内。
- [ ] 动态网格没有 NaN/Inf 顶点。
- [ ] 三角形索引不越界。
- [ ] Cancel 后任务回到 Idle。
- [ ] Actor 删除后 Builder 不继续访问对象。
- [ ] Editor 模块不会进入非 Editor Target。

### 人工测试矩阵

```text
平面自由落体
斜坡滑动
凸起和凹槽
极短 Top Spline
很宽的 Top Spline
1 / 8 / 32 / 100 条路径
很小和很大的固定时间步长
生成中取消
生成后 Undo / Redo
保存并重启编辑器
PIE
Development 打包
```

### 发布前检查

- [ ] UObject 指针均正确参与 GC。
- [ ] Delegate 在生命周期结束时解绑。
- [ ] Editor-only 代码有正确宏和模块边界。
- [ ] 没有每帧重复查找组件或 Actor。
- [ ] 没有不必要的运行时 Tick。
- [ ] 资产引用优先使用软引用。
- [ ] 模块 Startup/Shutdown 注册操作对称。
- [ ] README 包含安装和基本使用方法。
- [ ] 不包含参考插件的受版权保护资源。

## 6. 依赖规划

### Runtime 初始依赖

```text
Core
CoreUObject
Engine
```

### 动态网格阶段按需增加

```text
GeometryFramework
GeometryCore
```

只有确实调用 Geometry Script API 时再增加：

```text
GeometryScriptingCore
```

### FX 阶段按需增加

```text
Niagara
AudioMixer（仅在确有需要时）
```

### Editor 模块依赖

```text
TYWaterfallToolsRuntime
UnrealEd
LevelEditor
Slate
SlateCore
ToolMenus
PropertyEditor
EditorFramework
InteractiveToolsFramework
EditorInteractiveToolsFramework
ComponentVisualizers
AssetTools
ContentBrowser
```

依赖应按阶段增加，不要一次照搬参考插件的全部 Build.cs。

## 7. 参考源码索引

参考插件根目录：

```text
D:/UEProjectReal/VibeCoding_Test/Plugins/SHADERSO9165f204717eV6
```

| 要理解的功能 | 优先阅读文件 | 备注 |
| --- | --- | --- |
| 插件模块划分 | `Shadersource_WaterfallTool2.uplugin` | Runtime + Editor 双模块 |
| Actor 组件结构 | `Source/Shadersource_Waterfall2Runtime/Private/Actors/SH_Waterfall2.cpp` | 观察组件所有权和 Builder 协调 |
| 路径数据结构 | `Source/Shadersource_Waterfall2Runtime/Public/EditorComponents/SH_WaterfallPathComponent.h` | 模拟点、采样和缓存 |
| 单路径模拟 | `Source/Shadersource_Waterfall2Runtime/Private/EditorComponents/SH_WaterfallPathComponent.cpp` | 重力、阻力、Trace、滑动 |
| 多路径调度 | `Source/Shadersource_Waterfall2Runtime/Private/Generation/SH_WaterfallBuilder_Path.cpp` | 创建、排队、Tick、取消 |
| 动态网格 | `Source/Shadersource_Waterfall2Runtime/Private/Generation/SH_WaterfallBuilder_Mesh.cpp` | 顶点缓冲和四种模式 |
| 烘焙 | `Source/Shadersource_Waterfall2Runtime/Private/Generation/SH_WaterfallBuilder_Static.cpp` | Dynamic Mesh 到 Static Mesh |
| Editor Mode | `Source/Shadersource_Waterfall2EdMode/Private/EdMode/SH_Waterfall2EditorMode.cpp` | 进入、退出和选择变化 |
| Toolkit | `Source/Shadersource_Waterfall2EdMode/Private/EdMode/SH_Waterfall2EditorModeToolkit.cpp` | Slate 主界面和状态切换 |
| Details 定制 | `Source/Shadersource_Waterfall2EdMode/Private/Details/SH_Details_WaterfallEdit.cpp` | 参数布局和按钮 |
| 模块注册 | `Source/Shadersource_Waterfall2EdMode/Private/Shadersource_Waterfall2EdModeModule.cpp` | 菜单、命令、Details、Visualizer |
| Niagara 数据 | `Source/Shadersource_Waterfall2Runtime/Private/Components/SH_WaterfallVFXComponent.cpp` | 点数组和空间转换 |

## 8. 本项目文件索引

当前 Editor Mode 模板入口：

```text
Source/TYWaterfallTools/Private/TYWaterfallToolsModule.cpp
Source/TYWaterfallTools/Private/TYWaterfallToolsEditorMode.cpp
Source/TYWaterfallTools/Private/TYWaterfallToolsEditorModeToolkit.cpp
Source/TYWaterfallTools/Private/TYWaterfallToolsEditorModeCommands.cpp
```

模板中的 `SimpleTool` 和 `InteractiveTool` 暂时保留。阶段 6 决定它们是否仍有教学价值，再删除或替换，不在模块拆分时同时清理。

## 9. 技术决策记录

| 编号 | 决策 | 原因 | 状态 |
| --- | --- | --- | --- |
| ADR-001 | 保留 `TYWaterfallTools` 作为 Editor 模块，新增 `TYWaterfallToolsRuntime` | 减少对现有模板代码的改名和迁移 | 已决定 |
| ADR-002 | 第一种网格只实现 Per-Path Ribbon | 拓扑最简单，便于验证路径和材质数据 | 已决定 |
| ADR-003 | 生成任务分帧执行并可取消 | 防止编辑器冻结，并支持较多路径 | 已决定 |
| ADR-004 | 使用固定时间步长和 `FRandomStream` | 保证同一参数可复现 | 已决定 |
| ADR-005 | 先完成动态网格，再实现 Static Mesh Bake | 隔离几何错误和资产保存错误 | 已决定 |
| ADR-006 | 运行时 Actor 默认不 Tick | 生成是编辑器工作流，避免无意义开销 | 已决定 |
| ADR-007 | 四类网格使用独立开关并组合到同一个 Dynamic Mesh | 当前无 Advanced 分步界面；独立开关兼顾单项调参与任意组合，Singular 使用稳定的统一重采样拓扑 | 已决定 |
| ADR-008 | 阶段 9 只实现 Niagara，Audio 延期 | 先稳定 FX 数据协议和空间转换，避免同时引入声音布局与衰减资产 | 已决定 |

后续遇到会影响多个阶段的架构选择时，在此追加 ADR，而不是只把决定留在聊天记录中。

## 10. 进度日志模板

每完成一个小任务，在这里追加记录：

```text
### YYYY-MM-DD - 阶段 N / 任务名称

- 完成：
- 修改文件：
- 验证方式：
- 遗留问题：
- 下一步：
```

## 11. 下一步

阶段 10 静态网格烘焙已完成代码实现，当前等待 UE 5.8 编译与资产持久化、材质槽、UV 和显示切换验收。

### 2026-09-18 - 阶段 0 / Runtime 模块边界

- 完成：新增 `TYWaterfallToolsRuntime`，保留原模块作为 Editor 模块，并建立单向模块依赖。
- 修改文件：`.uplugin`、两个模块的 `Build.cs`、Runtime 模块入口文件。
- 验证方式：用户已在 UE 5.8 下完成 Development Editor 编译。
- 遗留问题：尚未进行非 Editor Target 打包验证，安排在发布测试阶段。
- 下一步：创建 `ATYWaterfallActor` 和阶段 1 所需默认组件。

### 2026-09-18 - 阶段 1 / Actor 骨架实现

- 完成：新增 `ATYWaterfallActor`，包含根组件、顶部样条、隐藏的烘焙网格组件和 Editor-only 终止平面。
- 修改文件：`Source/TYWaterfallToolsRuntime/Public/Actors/TYWaterfallActor.h`、`Source/TYWaterfallToolsRuntime/Private/Actors/TYWaterfallActor.cpp`。
- 验证方式：用户已在 UE 5.8 中重新生成工程文件并成功编译，确认 Actor 放置、样条编辑、保存和重开关卡均正常。
- 遗留问题：终止平面的可视化交互将在后续 Editor Mode 阶段完善。
- 下一步：进入阶段 2 的单条路径模拟。

### 2026-09-18 - 阶段 2 / 单条路径模拟实现

- 完成：新增 `UTYWaterfallPathComponent`、模拟点状态和 Actor 的生成/清除预览路径操作。
- 修改文件：`Source/TYWaterfallToolsRuntime/Public/Components/TYWaterfallPathComponent.h`、`Source/TYWaterfallToolsRuntime/Private/Components/TYWaterfallPathComponent.cpp`、Actor 头文件和实现文件。
- 验证方式：用户已在 UE 5.8 中完成编译，并验证生成、碰撞滑动、参数可复现和路径清理功能。
- 遗留问题：终止高度暂时以 Actor Z 为基准。
- 下一步：进入阶段 3 的多路径和分帧任务。

### 2026-09-20 - 阶段 3 / 多路径和分帧任务实现

- 完成：新增共享 Settings Component、明确的生成状态机、确定性多路径创建、每帧共享步数预算、取消和清理入口。
- 方向约定：Top Spline 定义瀑布宽度，路径使用其 Right Vector 垂直流出；`Reverse Flow Direction` 可翻转流向。
- 修改文件：Settings Component、Path Builder、Path Component 状态机、Waterfall Actor 和 Runtime Build.cs。
- 验证方式：UE 5.8 Development Editor 编译通过；用户已完成多路径、分帧生成、取消、清理、Undo/Redo 和垂直流向验证。
- 遗留问题：FX 尚未实现；动态网格已在阶段 4 接入同一清理入口。
- 下一步：通过阶段 3 验收后进入 Per-Path 动态带状网格。

### 2026-09-20 - 阶段 4 / Per-Path 动态带状网格

- 完成：新增 Dynamic Mesh Component 和 Mesh Builder，将全部有效路径合并为一个按距离采样的带状动态网格。
- 网格约定：UV0 为归一化表面坐标，UV1 为厘米距离坐标；宽度方向由 Top Spline 切线投影到路径法平面得到。
- 参数：Ribbon Width 控制带宽，Mesh Sample Spacing 控制纵向几何密度，Base UV Scale 控制参考材质协议的 UV 缩放；后续四类几何改为独立材质槽。
- 生命周期：重新生成、取消或清除路径时先清除派生网格；Dynamic Mesh 保留为运行时组件，路径和生成参数仍为 Editor-only 数据。
- 修改文件：Mesh Component、Mesh Builder、Settings Component、Waterfall Actor、Path Builder 和 Runtime Build.cs。
- 验证方式：UE 5.8 `UE_MCPTestEditor Win64 Development` 编译通过；编辑器视觉和交互验收待完成。
- 下一步：验证三角形正反面、UV 连续性、参数变化、清理以及 Undo/Redo，确认后提交阶段 4。

### 2026-09-20 - 阶段 5 / 采样数据和材质通道

- 完成：新增 `FTYWaterfallSample`，由路径组件根据模拟点按距离生成稳定采样缓存。
- 数据：缓存位置、切线、法线、速度、累计距离、归一化距离、速度大小、碰撞冲击、方向变化湍流和稳定随机值。
- 网格：Per-Path Ribbon 改为消费采样缓存，并写入 UV0、UV1、UV2 和 Vertex Color。
- 修改文件：`Data/TYWaterfallSample.h`、Path Component、Path Builder、Mesh Builder、Mesh Component。
- 验证方式：用户已完成编译，并使用 Debug Material 验证距离、速度、湍流、冲击和稳定随机通道正确。
- 下一步：提交阶段 5，然后进入阶段 6 的 Editor Mode 最小工作流。

### 2026-09-20 - 阶段 6 / Editor Mode 最小工作流

- 完成：Editor Mode 跟踪当前选择的瀑布 Actor，Toolkit 提供路径生成、网格生成、全部清理、取消和实时进度。
- 生命周期：Mode 和 Toolkit 只保存 Actor 弱引用；选择变化时刷新；退出 Mode 时取消仍在运行的路径任务并释放引用。
- UI：无选择时显示提示；生成期间禁用生成和清理按钮，仅允许取消；Actor 参数继续显示在内嵌 Details View。
- 模板清理：示例 Tool 不再注册或显示，源文件暂时保留，避免本阶段混入文件删除。
- 修改文件：Editor Mode、Editor Mode Toolkit、`.gitignore` 和本路线图。
- 验证方式：用户已完成编译，并验证选择切换、按钮状态、进度、取消、清理以及退出 Mode 生命周期正确。
- UE 5.8 兼容：`SHorizontalBox` 和 `SVerticalBox` 统一由 `Widgets/SBoxPanel.h` 提供。
- 下一步：提交阶段 6，然后进入阶段 7 的 Details 定制和路径可视化。

### 2026-09-20 - 阶段 7 / Details 定制和路径可视化

- 面板精简：Editor Mode 只显示 `WaterfallSettings`，不再重复 Actor 的 Transform、Rendering、Collision 和网络参数。
- Details：隐藏 Settings Component 的通用组件分类，固定瀑布参数分类顺序；隐藏 Actor 的内部组件、生成路径数组和旧 CallInEditor 按钮分类。
- 状态：路径生成期间 Settings 只读，任务结束或取消后恢复编辑。
- 路径可视化：参考原插件的 `SetDrawDebug` 方案，复用 Spline Debug 渲染；每条路径根据索引获得稳定且不同的 HSV 颜色，不常驻绘制速度、法线或状态点。
- 网格可视化：`Show Mesh Wireframe` 显示 Dynamic Mesh 三角形线框和绿色顶点。
- 生命周期：Details Customization 和 Mesh Component Visualizer 均在 Editor 模块 Startup 注册、Shutdown 对称注销。
- 修改文件：Runtime Actor、Settings、Path、Mesh Builder；Editor Toolkit、Module、Build.cs、Details 和 Visualizers。
- 默认值调整：根据实际验证将 Max Steps 设为 80、Ribbon Width 设为 50，并默认关闭 Path Debug。
- 验证方式：用户已完成编译，并验证精简面板、生成期间只读、彩色路径 Debug 和网格线框功能正确。
- 下一步：提交阶段 7，然后进入阶段 8 的扩展网格模式。

### 2026-09-20 - 阶段 8 / Per Path、Cross 和 Splash 组合网格

- 完成：Generate Mesh 单次按顺序追加 Per Path、Cross 和 Splash，并用组合结果替换 Dynamic Mesh。
- Cross：在已有 Per Path 上为每条路径追加一条绕流向旋转 90 度的带状面；不重复主平面，避免共面闪烁。
- Splash：参考 WaterfallTool 的终点水花思路，在每条路径终点按切线和表面法线生成前后非对称的多环径向网格。
- 参数：Per Path 和 Cross 分别使用 Ribbon Width 与 Cross Width；Splash 参数与共享采样、UV 参数同时显示并统一配置。
- Singular：本阶段明确延期且不进入生成组合；未来需要先解决不同路径长度、交叉、终止位置和横向排序带来的拓扑兼容问题。
- 修改文件：Settings Component、Mesh Component、Mesh Builder、Waterfall Actor、Editor Mode Toolkit 和本路线图。
- 验证方式：已完成静态差异检查；UE 5.8 编译与编辑器视觉验证由用户执行。
- 下一步：验证组合网格完整性、正面朝向、材质通道、清理、线框 Debug 和 Undo/Redo，通过后提交阶段 8。

### 2026-09-20 - 阶段 8 / Singular 与四模式独立开关

- 完成：新增 Singular 连续水幕；Singular、Per-Path、Cross、Splash 改为四个可任意组合的生成开关，默认全部开启。
- Singular：参考 WaterfallTool 的统一采样规则网格思路，将有效路径按顶部宽度方向稳定排序，并按归一化距离重映射到最大采样行数后连接相邻路径。
- 材质数据：Singular 继续写入 UV0、UV1、UV2 与 Vertex Color；法线和三角形绕序与已验证的 Per-Path 正面约定一致。
- 面板：关闭某模式时隐藏其专属参数；全部模式关闭后 Generate Mesh 清空旧网格。
- 已知限制：固定路径顺序可保证拓扑稳定，但严重交叉的路径仍可能使 Singular 自交；实验性动态排序留待未来高级选项。
- 修改文件：Settings Component、Mesh Component、Mesh Builder、Editor Mode Toolkit 和本路线图。
- 验证方式：已完成静态差异检查；UE 5.8 编译与编辑器视觉验证由用户执行。
- 下一步：分别验证四种单项模式、组合模式、全关清空、Singular 正面与不同路径长度。

### 2026-09-20 - 动态网格 / 四组 UV 修正

- 参考：`SH_WaterfallBuilder_Mesh.cpp` 定义 UV0-UV3 的逐顶点语义；`SH_WaterfallBuilder_Static.cpp` 将已有 Dynamic Mesh 合并并烘焙，不负责生成 UV。
- 完成：四种网格均创建四层 UV overlay，逐顶点写入并为每个三角形设置 UV0-UV3；Vertex Color 的数据和写入保持不变。
- 数据：路径采样增加累计流动时间，路径保存顶部样条位置及距离并由路径种子生成固定 UV 随机值；Singular 重采样时同步插值 Flow。
- 兼容性：UV0.V、UV1 和 UV2 相比旧三通道协议有语义变化，旧材质需要依照上方协议调整；本阶段不实现 Static Mesh Bake。
- 旧关卡：已有路径不含新保存的 UV3 元数据；升级后先重新 Generate Paths，再 Generate Mesh。
- 修改文件：Sample、Path Component、Mesh Component、Settings Component 和本路线图。
- 验证方式：由用户在 UE 5.8 编译，并用 Debug Material 分别读取四种模式的 UV0-UV3 与 Vertex Color。

### 2026-09-20 - 动态网格 / 四类独立材质

- 完成：移除单一 Waterfall Material 设置，增加 Singular、Per-Path、Cross、Splash 四个独立材质设置。
- 槽位：固定使用 `0 = Singular`、`1 = Per-Path`、`2 = Cross`、`3 = Splash`，不随模式开关压缩，便于材质实例和未来 Static Mesh Bake 保持稳定映射。
- 网格：启用 Dynamic Mesh 的 per-triangle Material ID attribute，每类几何生成三角形时写入对应槽位 ID。
- 面板：材质字段跟随对应生成开关显示或隐藏。
- 验证方式：由用户在 UE 5.8 编译，为四个槽设置明显不同的材质并验证单项与组合生成。

### 2026-09-20 - 动态网格 / WaterfallTools 材质兼容修正

- 原因：简化协议只能让旧 Debug Material 正常显示，不能驱动参考插件材质；Singular 和 Cross 尤其依赖 Vertex Color RGB 流向、Alpha 湍流及未经归一化的距离数据。
- 参考：直接依照 `SH_WaterfallBuilder_Mesh.cpp` 的 `FOS_PathValues`、`MeshBuffers_CalculateColour`、`MeshBuffers_CalculateUV2/UV3` 和 `BuildMeshBuffers_OnePath`。
- 数据：Vertex Color 改为 `(NormalizedVelocity + 1) * 0.5` 与 Alpha=Turbulence；UV0、UV1、UV2、UV3 改用参考插件语义，Base UV Scale 默认 `(1,1)`。
- Flow：按 `SampleLength / Speed` 从首个采样开始累计，与参考路径缓存一致。
- 局部帧：采样法线改为由流向和横向切线叉乘得到，不再把碰撞 HitNormal 当作水幕表面法线；退化方向回退到顶部样条宽度轴。
- 拓扑：Per-Path 默认增加 5 个横向内部点，Cross 默认增加 2 个横向内部点；每行均为 `Subdivisions + 2` 个顶点并完整写入四组 UV、Vertex Color 和 Material ID。
- 保留：四类材质固定槽、模式开关、Singular 统一纵向重采样、现有 Splash 参数与 Niagara 数据协议不变。
- 验证方式：重新 Generate Paths 和 Generate Mesh，分别只开启一种模式，用原 WaterfallTools 材质验证可见性、流向、动画与遮罩。

### 2026-09-20 - 阶段 9 / Niagara 数据和组件

- 完成：新增统一 FX 点数据，并为 Actor 添加 Top、Middle、Bottom 三个 Niagara Component。
- 数据：仅向 Niagara 上传局部位置和 Forward/Up/Right 三个正交方向数组。
- 分组：Top/Bottom 分别取每条路径首尾点；Middle 按 Niagara Sample Spacing 生成内部等距点。
- 生命周期：路径完成后自动刷新；Refresh FX 可在参数或资产变化后手动刷新；Clear All 和取消会清空数组并停用系统。
- 空间：组件保存局部空间数据，Actor 移动、旋转以及 PIE 重建 Niagara 实例时无需重算路径。
- 资产：Settings 使用三个 Niagara System 软引用，明确刷新时才同步加载；插件声明 Niagara 依赖。
- Audio：根据本阶段范围明确延期，未添加 Audio Component、声音资产或空间衰减参数。
- 修改文件：FX Point Data、VFX Component、Waterfall Actor、Settings、Path Builder、Runtime Build.cs、uplugin、Editor Toolkit、Details 和本路线图。
- 验证方式：已完成静态 API 与差异检查；UE 5.8 编译和 Niagara 资产视觉验证由用户执行。
- 下一步：创建符合用户参数协议的三个 Niagara System，验证自动/手动刷新、Actor 变换、清理、保存重开和 PIE。

### 2026-09-21 - Splash 参考实现修正

- 原因：旧 Splash 是终点径向圆盘，无法复现 WaterfallTools 的前后端帽、局部 UV 和流向材质表现。
- 修正：按参考 `BuildMeshBuffers_Splash` 构造闭合宽度轮廓，在端帽处保持位置并旋转挤出方向，再沿轮廓生成多环顶点。
- 材质数据：UV0.X 改为外轮廓真实累计距离，UV1 使用轮廓局部距离和挤出方向投影；UV2、UV3、Vertex Color 继续写入终点采样数据。
- 绕序：Splash 三角形调整为朝向终点采样法线的正面绕序，避免单面材质从背面观察时消失。
- 平面：Splash 不再复用瀑布墙面的采样法线，改用入水面的水平法线构造宽度轴和落水轴，确保网格与水面平行。
- 朝向：Splash 先按参考插件完成闭合轮廓，再将整个轮廓、挤出方向和局部 UV 方向绕入水面中心 Z 轴旋转 180 度，避免仅反转落水轴造成端帽交叉。
- Cross：按参考 `MeshBuffers_CalculatePositions` 直接使用完整路径采样法线作为 Cross 平面的半宽方向（不再投影到切线正交平面），并采用参考的左右顶点顺序，修正材质侧视方向。
- Cross 绕序：Cross 使用参考插件独立的 `ABC / CBD` 三角形方向和对应法线，不再复用 Per-Path 的面片绕序。
- 验证方式：未编译；由用户在 UE 5.8 中验证 Splash 材质动画、遮罩、正面可见性及与 Per-Path/Cross 的组合。

### 2026-09-21 - 编辑模式与 Kill Plane 修正

- 编辑模式：保持仅显示 Waterfall Settings 的精简参数区；Actor 和组件变换由视口 Gizmo 与编辑器标准 Details 面板负责。
- 样条编辑：模式面板增加 `Edit Top Spline`，直接选择 TopSpline 组件并交给 UE 的 Spline Visualizer 编辑。
- Kill Plane：增加 `Edit Kill Plane` 操作；组件在编辑器中可见、可选，运行时保持隐藏，网格本身不参与碰撞。
- 模拟：参考 WaterfallTools 对每一步运动线段执行无限数学平面相交测试；穿越 Kill Plane 时精确停在交点并保存最终样本，不依赖 Plane 网格碰撞。
- 默认位置：Kill Plane 初始位于 Actor 局部 Z=-1000，与默认 `Termination Height` 保持一致；用户可在模式面板中独立移动它。
- 视口交互：Editor Mode 改为继承 `UBaseLegacyWidgetEdMode`，接入 UE5.8 默认选择模式使用的 Transform Widget 与组件视口交互桥接层。
- 碰撞开关：Simulation 新增 `Enable World Collision`；关闭时跳过场景碰撞和表面滑动，但重力、阻力、步进及 Kill Plane 数学平面终止仍然生效。

### 2026-09-22 - 阶段 10 / 静态网格烘焙实现

- 完成：Editor 模块新增 Static Mesh Baker，通过 Content Browser 选择资产路径；覆盖已有资产前二次确认；从合并 Dynamic Mesh 保存四组 UV、顶点色和四个固定材质槽；烘焙后绑定 `BakedMeshComponent` 并切换为静态预览。
- 修改文件：Editor Build.cs、uplugin、Static Mesh Baker、Editor Mode Toolkit、Waterfall Actor、Settings Component 和 Mesh Builder。
- 验证方式：已完成源码和 API 静态检查，等待 UE 5.8 编译、资产重启持久化、材质与 UV 验收。
- 遗留问题：重复覆盖依赖 Unreal Editor 的强制删除流程；覆盖前会明确警告，取消不会修改原资产。
- 下一步：完成阶段 10 编辑器验收后，再决定是否在烘焙完成时提供清除动态源数据的可选项。
