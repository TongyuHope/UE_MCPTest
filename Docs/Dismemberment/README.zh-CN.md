# 断肢系统（DetachedLib）总览

> 更新：2026-10-12。用户已反馈首轮验证通过，并确认单人玩家距离清理效果已实现。自动化测试未由助手执行。
> 路径相对工程根目录。资产配置与已有测试结果来自用户说明；本轮没有保存或修改 .uasset。

## 1. 系统定位

DetachedLib 通过隐藏宿主骨骼或销毁独立的模块化网格组件，生成一个独立的物理断肢 Actor。它使用预制候选模型，不进行运行时网格切割、断口封面生成或身体模型自动切换。

运行链路：

~~~
动画通知 / 蓝图注册请求
    → 角色 DismembermentComponent
    → 数据资产默认参数 + 表现预设 + 显式覆盖
    → 类型、目标、碰撞和物理资源校验
    → 生成断肢，启动物理
    → 隐藏骨骼 / 销毁独立网格
    → 恢复碰撞预设 → 停止模拟
    → 按时间或持续超出玩家距离清理（与物理时序独立）
~~~

原有 ApplyDismemberment(BoneName, Config) 蓝图/C++ 入口保留，直接使用传入参数。

## 2. 文件与职责

| 文件组 | 职责 |
| --- | --- |
| DismembermentTypes.h | 空间、摘除模式、类型覆盖、最终参数、表现预设和请求结构 |
| DismembermentConfigData.h/.cpp | 候选模型、部位默认参数、命名表现预设、参数解析和资产校验 |
| DismembermentComponent.h/.cpp | 主体保护、目标解析、模型类型选择、生成流程、诊断与批量请求 |
| AnimNotify_ApplyDismemberment.h/.cpp | 部位/预设选择、旧通知迁移、按顺序触发请求、编辑器校验 |
| DismembermentLimbActor.h/.cpp | 双网格互斥使用、物理启动、碰撞时序、清理 |
| DismembermentLog.h | 专用 LogDismemberment 日志分类 |
| Private/DetachedLib/Tests/DismembermentTests.cpp | 五项编辑器自动化测试，尚未运行 |

头文件位于 Source/UE_MCPTest/Public/DetachedLib，实现位于 Private/DetachedLib。反射类型名称与模块保持不变；FDismembermentConfig 和 ELimbSpace 移至共享头文件，已有包含 DismembermentComponent.h 的调用方仍可使用它们。

## 3. 当前已实现的配置流程

数据资产每部位保留两类候选数组，并新增 DefaultConfig 与 ImpactPresets。预设只包含线性和角冲击参数，避免误改宿主目标。

注册请求按以下优先级解析：

1. 部位 DefaultConfig；
2. 若选了 PresetName，覆盖冲击参数；
3. 请求中勾选的显式覆盖组。

零力度、零角冲量与空目标名均可作为显式覆盖值。预设键不存在时仍返回失败，不因请求覆盖了力度而忽略无效预设。

组件新增 LimbMeshTypeOverride：

| 值 | 行为 |
| --- | --- |
| UseAssetDefault | 使用数据资产 LimbMeshType，保持已有默认选择 |
| StaticMesh | 本角色读取静态候选数组 |
| SkeletalMesh | 本角色读取骨骼候选数组 |

覆盖不修改共享资产，也不自动判断设备性能。所选数组缺资源时明确失败，不自动回退另一类型。两类数组仍是硬引用，两种网格组件仍同时创建；资产驻留内存优化需要另行设计。

## 4. 摘除模式与保护

| 模式 | 取点 | 处理 |
| --- | --- | --- |
| HideBone | 目标骨骼世界变换 | HideBoneByName + PBO_Term |
| DestroyWholeMeshComponent | 目标组件世界变换 | DestroyComponent |

骨骼模式检查骨骼存在性，并拒绝直接隐藏 Leader Pose 跟随网格。整网格模式必须显式指定独立目标且成功解析驱动网格，不能销毁驱动网格、ACharacter 主网格或其他骨骼网格的 Leader Pose 来源。

驱动名称默认 CharacterMesh0；留空时优先取 ACharacter::GetMesh，普通 Actor 则只在恰好一个骨骼网格组件时自动选择。缓存失效或名称变更时会重新解析。

PBO_Term 终止的是物理体。取消视觉隐藏不会自动恢复这些物理体；销毁的组件需要重建。当前不提供完整接回、恢复状态机或对象池。

## 5. 空间与物理时序

ELimbSpace 保留 WorldSpace/BoneSpace 原有枚举值，并新增 CharacterSpace。线性方向与 AngularImpulseSpace 分别选择空间，旧数据的角冲量默认保持世界空间。

BoneSpace 使用断口变换，整网格模式则是组件变换。线性方向仍先带缩放变换再归一化；均匀正缩放不会增大力度，非均匀缩放可能改变方向。角冲量只旋转，不带缩放。

bUseWorldScale 现在控制生成模型是否继承断口尺度，不控制方向变换；旧通知迁移会设为 true，保留此前始终继承尺度的行为。

默认名义时序：0.1 秒避让后恢复最终预设，5.1 秒停止物理，30 秒销毁。0 延迟使用 TimerManager 下一次处理机会。LifeSpan 独立计时，停模拟不显式关闭碰撞。

DetachedLimbCleanupMode 默认 Time；选择 Distance 后由循环 Timer 按单人玩家当前 Pawn 与掉落模型的三维距离判定，连续超距后销毁。默认距离 3000 cm、检查间隔 1 s、持续超距 3 s。DetachedLimbLifeSpan 在两种模式中都是最长寿命，设为 0 关闭时间上限；返回范围、玩家缺失或更换 Pawn 会重置超距计时。配置步骤见 Usage 7.1。

生成流程会先校验模型的物理资源和碰撞预设，再确认模拟启动，成功后才摘除宿主；物理无法启动时销毁新断肢并保留宿主。

## 6. 用户确认的基线

用户于 2026-10-10 提供的回答：

- 没手_Skeleton 保留手部骨骼，相关骨骼命名一致。
- DA_Dismemberment 登记 upperarm_r，骨骼候选为“右手”，静态数组为空，类型为 SkeletalMesh，碰撞默认。
- BP 的组件名正确，原有流程已由用户测试可用；当前仍是个人测试阶段。
- 动作组自行摆通知；静态与骨骼两条路径都需要，允许表现不同。
- 不存在同一部位多次断肢的业务情形，本轮没有提出接回需求。

代码首轮修改的 Git 基线为 7f9b8f1。当前代码变更尚未自动提交或 push。

## 7. 待验证与能力边界

- 用户在 IDE 中完整构建 UE_MCPTestEditor，检查旧动画通知自动迁移后的效果。
- 当前 DA 尚无静态候选，须补资源后验证静态路径。
- 没有宿主姿势/材质覆盖/速度复制、落地检测、网络 RPC 或专用复制逻辑。
- 请求数组保持填写顺序，批量返回成功数量；没有事务回滚或跨通知重复断肢状态记录。
- 自动化测试包括真实模型的物理路径测试，但尚未执行。

## 8. 文档入口

- [Usage.zh-CN.md](Usage.zh-CN.md)：配置、迁移与动作组操作步骤。
- [Implementation.zh-CN.md](Implementation.zh-CN.md)：当前接口与代码行为。
- [易用性改造讨论.zh-CN.md](易用性改造讨论.zh-CN.md)：需求回答、原始备选方案与实施状态。
