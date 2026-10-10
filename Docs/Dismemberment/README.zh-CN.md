# 断肢系统（DetachedLib）总览

> 本文档基于对 `Source/UE_MCPTest` 下 `DetachedLib` 全部源码的静态阅读整理，未运行工程验证。
> 涉及的路径均相对于工程根目录 `D:\UEProjectReal\UE_MCPTest`。
> 最近核对：2026-10-10。已阅读四份文档、四个实现文件及配套头文件；未读取蓝图/数据资产内部配置，也未进行 PIE 验证。
> 同日用户补充了现有资产配置和测试结果，见第 8 节；这些信息来自用户确认，与本次静态检查区分记录。

## 1. 系统定位

`DetachedLib` 是一个**自包含的断肢（Dismemberment）运行时库**，作为 `UE_MCPTest` 模块（UE 5.8）的一部分编译，位于：

- 头文件：`Source/UE_MCPTest/Public/DetachedLib/`
- 实现：`Source/UE_MCPTest/Private/DetachedLib/`

它解决的问题是：**角色身体某一部分被打断后，把该部分从骨架上"摘掉"（隐藏或销毁），并在断口处掉落一个带物理模拟的断肢模型。**

设计上的三个关键特征：

1. **数据驱动**：断哪根骨骼、掉什么模型、用哪个碰撞预设，全部来自一个 `UDataAsset`，不需要为每根骨骼写代码。
2. **两种摘除模式**：单骨架用"隐藏骨骼（含子骨骼）"，模块化身体用"销毁整个网格组件"。
3. **物理避让**：断肢生成瞬间先换用会忽略 Pawn 的碰撞预设，约 0.1 秒后再恢复，避免与新断开的宿主发生挤压、弹飞或自碰撞抖动。

## 2. 文件构成

| 文件 | 类型 | 职责 |
| --- | --- | --- |
| `Public/DetachedLib/DismembermentConfigData.h`<br>`Private/DetachedLib/DismembermentConfigData.cpp` | `UDataAsset` + `USTRUCT` + `UENUM` | 配置数据资产：骨骼名 → 候选断肢模型列表、网格类型、避让结束后的碰撞预设 |
| `Public/DetachedLib/DismembermentComponent.h`<br>`Private/DetachedLib/DismembermentComponent.cpp` | `UActorComponent` | 断肢核心：校验、解析目标网格、隐藏骨骼/销毁组件、生成断肢并施加冲量 |
| `Public/DetachedLib/DismembermentLimbActor.h`<br>`Private/DetachedLib/DismembermentLimbActor.cpp` | `AActor` | 掉落的断肢本体：双网格互斥装载、物理模拟、避让时序、生命周期 |
| `Public/DetachedLib/AnimNotify_ApplyDismemberment.h`<br>`Private/DetachedLib/AnimNotify_ApplyDismemberment.cpp` | `UAnimNotify` | 动画帧触发入口：按配置批量断多根骨骼 |

`DismembermentConfigData.cpp` 只有一行 `#include`，是纯声明式类型，无运行逻辑。

## 3. 类关系

```
UAnimNotify_ApplyDismemberment            （动画通知，摆在动画轨道上）
        │  Notify() 时从 Owner 上 FindComponentByClass
        ▼
UDismembermentComponent                    （挂在角色 Actor 上）
        │  读取 ──► UDismembermentConfigData  （数据资产：骨骼名 → 候选模型）
        │  SpawnActor
        ▼
ADismembermentLimbActor                    （掉落的断肢，独立 Actor）
        │  持有互斥的两个组件
        ├─► UStaticMeshComponent   LimbMesh
        └─► USkeletalMeshComponent LimbSkeletalMesh
```

## 4. 一次断肢的完整数据流

以动画通知触发为例，调用链如下（行号见各文件）：

```
1. 动画播放到通知帧
   UAnimNotify_ApplyDismemberment::Notify()
        ├─ 校验 MeshComp / Owner 有效
        ├─ Owner->FindComponentByClass<UDismembermentComponent>()
        └─ 遍历 ApplyDismembermentConfigs（TMap<骨骼名, FDismembermentConfig>）
              每条配置的 bHideWholeTargetMesh |= 通知级开关
              ↓
2. UDismembermentComponent::ApplyDismemberment(BoneName, Config)
        ├─ 校验 Owner / World
        ├─ 解析 TargetMesh（默认缓存的驱动网格，或按名即时查找）
        ├─ 校验 bHideWholeTargetMesh 的合法目标（禁止作用于驱动网格）
        ├─ 校验目标网格有 SkeletalMeshAsset、配置资产有效
        ├─ 骨骼模式下校验 BoneName 存在于目标骨架
        ├─ 从 DismembermentConfigData 查该骨骼的 FSKMDismembermentConfig 并校验有效
        ├─ 按 LimbMeshType 随机取出一个有效网格资源
        ├─ 计算断口世界变换（组件变换 或 骨骼变换）
        ├─ SpawnActor<ADismembermentLimbActor>(世界变换)
        ├─ 隐藏骨骼 HideBoneByName(PBO_Term) 或 DestroyComponent()
        ├─ SetLifeSpan(DetachedLimbLifeSpan)
        ├─ 按 LimbSpace 把冲击方向转到世界空间
        └─ 依次调用断肢的 4 个方法：
             ReInitializeLimb()         装载网格、设碰撞预设、记录激活组件
             SetPhysicalSimulationAvoidance()  写入避让时间与避让预设
             SetLimbPhysicsSimulationTime()    写入物理模拟时长
             AddImpulse()               开物理、唤醒刚体、施加线/角冲量
              ↓
3. ADismembermentLimbActor 自主运行
        避让阶段 → 恢复碰撞 → 物理模拟 → 停止模拟 → LifeSpan 到期销毁
```

## 5. 两种断肢模式

| | 骨骼模式（默认） | 整网格模式 |
| --- | --- | --- |
| 开关 | `bHideWholeTargetMesh = false` | `bHideWholeTargetMesh = true` |
| 作用对象 | 驱动网格（或显式指定的网格）上的**某根骨骼** | **必须显式指定**的模块化网格组件 |
| 摘除手段 | `HideBoneByName(BoneName, PBO_Term)`，隐藏该骨骼及子层级，并终止对应层级的物理体 | `DestroyComponent()`，整个组件消失 |
| 前置校验 | 骨骼必须存在于目标骨架 | 目标名不得为空，且不得是驱动网格本身 |
| 断口取点 | `GetBoneTransform(BoneName)` | `GetComponentTransform()` |
| 典型用途 | 单一骨架角色的断手、断脚 | 模块化身体（独立手臂/腿部网格）整块掉落 |
| 可逆性 | 可取消视觉隐藏；`PBO_Term` 终止的物理体不会随取消隐藏自动恢复 | 必须重建组件或重生 Actor |

关于"驱动网格"：`UDismembermentComponent::DrivenSkeletalMeshComponentName` 指定主体网格组件名，在 `BeginPlay` 时解析一次并缓存。整网格模式被硬性禁止作用于它（见 `DismembermentComponent.cpp` 约 56–61 行），这是防止误删主体网格的保护。

这项保护依赖缓存正确建立：源码只比较 `TargetMesh == DrivenSkeletalMeshComponent`，没有自动识别 `ACharacter::GetMesh()` 或 Leader Pose 关系。驱动网格未解析成功时，显式目标仍可继续执行；不能据此认为任意主体网格都被自动保护。整网格模式下 `BoneName` 只用作数据资产查找键，不检查它是否存在于目标骨架。

## 6. 关键可调参数

### 6.1 组件级（`UDismembermentComponent`）

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `DrivenSkeletalMeshComponentName` | `CharacterMesh0` | 按组件 `GetName()` 查找；不要把编辑器显示名 `Mesh` 当作运行时对象名 |
| `DismembermentConfigData` | 空 | 配置数据资产，为空则 `ApplyDismemberment` 直接失败 |
| `PhysicalSimulationAvoidanceTime` | `0.1` 秒 | 使用临时避让碰撞预设的时长；为 0 时退化为"下一帧"处理 |
| `PhysicalSimulationAvoidanceCollisionProfileName` | `Ragdoll` | 避让期间使用的碰撞预设 |
| `LimbPhysicsSimulationTime` | `5.0` 秒 | 避让结束后的物理模拟时长 |
| `DetachedLimbLifeSpan` | `30.0` 秒 | 断肢 Actor 存活时间；为 0 表示不自动销毁 |

### 6.2 每条断肢配置（`FDismembermentConfig`）

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `TargetSkeletalMeshComponentName` | 空 | 为空时使用缓存的驱动网格；非空则按名查找目标网格 |
| `bHideWholeTargetMesh` | `false` | 是否走整网格销毁模式 |
| `ImpactDirection` | `(0,0,0)` | 冲击方向，会先归一化再乘以力度 |
| `ImpactForce` | `0.0` | 冲击力度；为 0 时方向无效，只保留角冲量 |
| `AngularImpulseDegrees` | `(0,0,0)` | 角速度增量，以度为单位传入；不随 `LimbSpace` 转换 |
| `LimbSpace` | `BoneSpace` | 只控制线性冲击方向；整网格模式下使用目标组件的局部空间 |
| `bUseWorldScale` | `true` | 结构体字段（见下文"注意事项"） |

### 6.3 数据资产级（`UDismembermentConfigData`）

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `DismembermentConfigs` | 空 | `TMap<骨骼名, FSKMDismembermentConfig>`，只有列在这里的骨骼才允许断肢 |
| `LimbMeshType` | `StaticMesh` | 决定读取候选静态网格数组还是候选骨骼网格数组 |
| `DismembermentLimbCollisionProfile` | `BlockAllDynamic` | 避让阶段结束后断肢使用的最终碰撞预设，不依赖落地事件 |

### 6.4 候选模型（`FSKMDismembermentConfig`）

一根骨骼可配多个候选模型，断肢时随机取一个：

- `DismembermentMeshes`：`TArray<UStaticMesh*>`
- `DismembermentSkeletalMeshes`：`TArray<USkeletalMesh*>`

两个数组都会被 `Valid()` 统计，但**运行期只读取 `LimbMeshType` 指定的那一个**。随机实现会先统计有效项数量，再取第 N 个有效项，因此数组中间存在空引用也不会随机到空指针。

## 7. 物理时序

`ADismembermentLimbActor::StartLimbPhysicsSimulation()` 编排了断肢的完整物理生命周期：

```
t = 0                     DisableLimbCollision()   切换到避让预设（默认 Ragdoll）
                          SetSimulatePhysics(true)
t = 0                     WakeAllRigidBodies() + AddImpulse + AddAngularImpulseInDegrees
   │
   │  PhysicalSimulationAvoidanceTime（默认 0.1s）
   ▼
t = 0.1s                  EnableLimbCollision()    恢复 DismembermentLimbCollisionProfile
   │
   │  LimbPhysicsSimulationTime（默认 5s）
   ▼
t = 5.1s                  StopLimbPhysicsSimulation()  SetSimulatePhysics(false)，断肢在空中冻结
   │
   │  剩余时间直至 LifeSpan（默认 30s，由组件 SetLifeSpan 设定）
   ▼
t = 30s                   Actor 被引擎销毁
```

两个计时器都从启动物理时注册：恢复预设的延迟是避让时间，停止模拟的延迟是避让时间加物理时间。时长由 setter 钳制为非负；延迟为 0 时用 `SetTimerForNextTick`，实际触发取决于 TimerManager 的更新。LifeSpan 独立计时，较短时可以先销毁 Actor。

"物理避让"的意图：断肢刚生成时若立刻使用正常碰撞体，会与被隐藏或销毁的宿主骨骼在同一位置发生重叠，导致挤压、弹飞或抖动。先切到 `Ragdoll`（默认忽略 Pawn）可让断肢在最初几帧"穿过"宿主，随后再恢复正常碰撞。

## 8. 当前工程中的接入状态

搜索 `Source` 目录，除 `DetachedLib` 自身以外，没有其他 C++ 类型引用它。`Content/Environment/LimbCharacter/` 已存在 `BP_Dismemberment.uasset`、`DA_Dismemberment.uasset` 和相关网格、动画资产；仅凭源码和文件名无法判断蓝图是否已经接入。现有入口是动画通知 `Apply Dismemberment` 和蓝图/C++ 调用 `ApplyDismemberment`。

**用户已确认的测试基线（2026-10-10）**：

- `没手_Skeleton` 保留手部骨骼，相关骨架的骨骼命名一致。
- `DA_Dismemberment` 登记 `upperarm_r`；`Dismemberment Skeletal Meshes` 使用“右手”骨骼网格，`Dismemberment Meshes` 为空；`Limb Mesh Type = SkeletalMesh`，碰撞预设保持默认。此配置与当前代码的资源选择路径一致。
- `BP_Dismemberment` 使用的组件名称正确，现有方法已经由用户测试可用；当前目标是优化已有功能。
- 系统仍在个人测试阶段，尚未用于正式项目，资产迁移成本较低；后续由动作组自行放置通知。
- 静态与骨骼两种断肢都需要保留，允许表现不同；静态版本拟用于低端设备。性能差异仍需实测，当前没有静态候选资产的配置。
- 用户没有同一部位多次断肢的使用情形。本轮需求未提出接回或完整恢复功能。

这些结论来自用户提供的 `C:\Users\Administrator\Desktop\新建文本文档.txt`；本次没有独立读取资产或重做运行测试。

**Git 基线（本地检查）**：仓库地址为 [TongyuHope/UE_MCPTest](https://github.com/TongyuHope/UE_MCPTest.git)，当前分支 `main` 跟踪 `origin/main`。本轮文档修改前工作区干净，HEAD 与本地远程跟踪引用均为 `0d3a084`。用户说明已在另一个对话 push；本次未 fetch 或访问远端重新核验，也未执行提交/push。

`UE_MCPTest.Build.cs` 已包含 `Core`、`CoreUObject`、`Engine` 等依赖；当前库使用这些模块的能力，头文件用 `DetachedLib/xxx.h` 形式包含，经 `Public/` 根目录解析。本次没有编译，不能把依赖检查视为编译通过。

接入前请阅读 [Usage.zh-CN.md](Usage.zh-CN.md) 中的“接入清单”，先核实运行时组件名、配置资产与候选模型类型。

### 当前能力边界

- 系统生成独立的候选断肢模型，没有运行时切割网格、生成断口封面或自动切换“没手”模型的逻辑。
- 骨骼断肢没有已断状态记录或重复调用保护；同一骨骼可再次生成掉落物。通知批量配置使用 `TMap`，没有规定执行顺序，也没有事务回滚。
- 骨骼网格断肢只装载新资产，没有从宿主复制当前姿势、材质覆盖、动画蓝图或速度。
- 物理是否真正生效依赖候选模型的碰撞/Physics Asset。`ApplyDismemberment` 返回 `true` 表示流程已执行，不代表物理表现已通过验证。
- 当前库没有专门的复制/RPC、恢复断肢、对象池或落地检测实现。

## 9. 相关文档

- [Implementation.zh-CN.md](Implementation.zh-CN.md) —— 逐类实现细节与代码要点
- [Usage.zh-CN.md](Usage.zh-CN.md) —— 接入步骤、配置示例与排查清单
- [易用性改造讨论.zh-CN.md](易用性改造讨论.zh-CN.md) —— 尚未实施的改造建议与待核实资产信息
