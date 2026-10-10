# 断肢系统接入与使用指南

> 面向"在当前工程中把断肢系统跑起来"的实操步骤。
> 本文档基于源码静态阅读整理，步骤中的具体数值（力度、时长）需要按项目手感调整。
> 最近核对：2026-10-10。蓝图、数据资产和动画通知的实际配置尚未读取；以下是现有代码支持的操作，不代表已在 PIE 中验证。

## 0. 用户已测试的配置基线

以下由用户于 2026-10-10 提供，未在本轮独立打开资产验证：

| 项目 | 当前配置/结果 |
| --- | --- |
| 数据资产键 | `upperarm_r` |
| `Limb Mesh Type` | `SkeletalMesh` |
| `Dismemberment Skeletal Meshes` | “右手”骨骼网格 |
| `Dismemberment Meshes` | 空 |
| 碰撞预设 | 用户说明保持默认；类声明的最终预设默认为 `BlockAllDynamic` |
| `BP_Dismemberment` 组件名 | 用户确认正确，具体字符串未提供 |
| 现有断肢流程 | 用户已经实测可用，当前进入优化阶段 |

因此当前不需要以“修正组件名”或“修正候选数组”为前提重新接入。切换到 `StaticMesh` 测试前，必须先为 `upperarm_r` 补静态候选模型；仅修改类型开关会在生成前失败。骨骼类型与静态类型共用同一份数据资产时，现有开关仍是资产级设置，当前代码没有按设备自动选择的入口。

动作组将自行摆通知，后续配置简化与质量档选择的提案见 [易用性改造讨论.zh-CN.md](易用性改造讨论.zh-CN.md) 第 9 节；这些接口尚未实现。

## 1. 接入清单（按顺序执行）

### 步骤 1：核实驱动网格的运行时名称

在角色的 `DismembermentComponent` 上，确认 `Driven Skeletal Mesh Component Name` 与主网格组件的 `GetName()` 一致。默认值是 `CharacterMesh0`，不要仅凭编辑器里显示的 `Mesh` 改名。

本机 UE 5.7 引擎源码的 `ACharacter::MeshComponentName` 为 **`CharacterMesh0`**；当前工程 `AUE_MCPTestCharacter` 继承自 `ACharacter`，没有在构造函数中重命名该网格。工程关联 UE 5.8，实际蓝图仍请通过运行时组件名称核实；可以在蓝图中从网格组件调用 `Get Object Name` 并打印。

> 若名称不匹配，`BeginPlay` 无法缓存驱动网格；未显式指定目标的调用会静默返回 `false`。显式目标仍可执行，但整网格模式的主体保护依赖驱动缓存，务必先配置正确。
>
> 驱动名只在 `BeginPlay` 解析一次；运行时改名或销毁后重建组件不会自动刷新缓存。

### 步骤 2：创建配置数据资产

在内容浏览器中右键：

```
Miscellaneous → Data Asset → DismembermentConfigData
```

命名为例如 `DA_Dismemberment_Humanoid`。

### 步骤 3：填写数据资产

1. **`Limb Mesh Type`**：先决定断肢用静态网格还是骨骼网格。这一项决定下面填哪个数组。
2. **`Dismemberment Configs`**：添加条目，骨骼模式的键应直接复制目标骨架中的骨骼名，避免拼写错误。键类型是 `FName`，通常比较不区分大小写，不应把仅大小写不同视为不同骨骼。
   - 值为 `FSKMDismembermentConfig`，其中：
     - 选 `StaticMesh` 模式就填 `Dismemberment Meshes`；
     - 选 `SkeletalMesh` 模式就填 `Dismemberment Skeletal Meshes`。
   - 一个部位可以放多个候选，运行期随机取一个（空引用会被跳过）。
3. **`Dismemberment Limb Collision Profile`**：避让结束后恢复的碰撞预设，默认 `BlockAllDynamic`，不需要等待落地。

> 关键约束：**只有出现在 `Dismemberment Configs` 里的骨骼才能被断肢。** 未登记的骨骼调用会被直接拒绝。
> 整网格模式的键只用于查候选模型，不要求对应目标骨架中的真实骨骼。`Limb Mesh Type` 对整份数据资产生效，不是每条配置独立选择。

### 步骤 4：在角色上挂载组件

把 `Dismemberment Component` 添加到角色蓝图（或 C++ 角色的构造函数中）：

- `Dismemberment Config Data` → 指向步骤 2 创建的资产。
- `Driven Skeletal Mesh Component Name` → 步骤 1 修正后的名字。
- 其余物理参数可先用默认值，见第 4 节。

### 步骤 5：触发断肢

两种方式任选：

- **动画通知（推荐）**：在动画序列的轨道上添加 `Apply Dismemberment` 通知，展开 `Apply Dismemberment Configs`，按骨骼名添加条目并设置各自的方向与力度。
- **蓝图/代码**：调用 `Apply Dismemberment`（`Bone Name` + `Dismemberment Config`）。该函数标记为 `BlueprintCallable`，且 `meta = (AutoCreateRefTerm = ...)`，在蓝图里未连接的引脚会自动生成默认结构体。

### 步骤 6：验证

播放包含该通知的动画，观察：

1. 目标部位从角色身上消失；
2. 断口处出现掉落的断肢，并受冲量飞出；
3. 默认约 5.1 秒后停止物理模拟（0.1 秒避让 + 5 秒模拟，可能悬停在空中）；
4. 约 30 秒后断肢消失。

## 2. 两种使用模式怎么选

### 2.1 骨骼模式（默认，适用于单一骨架角色）

- `bHideWholeTargetMesh = false`（默认）。
- `Target Skeletal Mesh Component Name` 留空即可，使用驱动网格。
- 指定 `BoneName`，系统调用 `HideBoneByName(BoneName, PBO_Term)`，隐藏该骨骼及子层级，并终止对应层级物理体。视觉效果依赖模型蒙皮；它不切割网格或自动生成断口封面。
  - 例：隐藏 `lowerarm_r` 会连同手掌与所有手指一起消失，实现"整条小臂断掉"。
  - 只隐藏 `hand_r` 则只断手掌。
- 断口位置取 `GetBoneTransform(BoneName)`，即**骨骼原点**。
- `UnHideBoneByName` 只能取消视觉隐藏，不能自动恢复 `PBO_Term` 终止的物理体。当前库没有完整的恢复接口。

### 2.2 整网格模式（适用于模块化身体）

- `bHideWholeTargetMesh = true`。
- **必须**填写 `Target Skeletal Mesh Component Name`，指向某个独立的模块化网格组件（如单独的小臂网格）。
- 系统对该组件执行 `DestroyComponent()`，整个网格消失，断口位置取**组件变换**。
- **硬性限制**：目标不能等于已经缓存的驱动网格，否则直接拒绝。代码没有自动识别主体/Leader Pose 关系，必须正确配置驱动缓存并只选择独立模块化组件。
- **不可逆**：组件被销毁后无法恢复，需要重建组件或重生角色。

## 3. 动画通知配置要点

`UAnimNotify_ApplyDismemberment` 的两个属性：

| 属性 | 作用 |
| --- | --- |
| `Apply Dismemberment Configs` | `TMap<骨骼名, FDismembermentConfig>`，一次可断多根骨骼 |
| `bHide Whole Target Mesh` | 通知级开关，用 `\|=` 合并进每一条配置 |

- 通知级开关与每条配置的开关是**逻辑或**关系：通知级设为 `true` 会让该通知下所有条目都走整网格模式。注意每条配置的 `Target Skeletal Mesh Component Name` 也必须各自填好，否则整网格模式会因目标名为空而失败。
- 若 `Apply Dismemberment Configs` 为空，编辑器会在资产校验时报错（`IsDataValid`）。
- 一个通知已经支持多条配置；但 `TMap` 不提供业务执行顺序，调用失败不会汇总或回滚。不要依赖父子骨骼先后次序，也不要让多个条目销毁同一组件。
- 动画通知从播放网格的 Owner 上找断肢组件；并不自动把播放网格当作断肢目标。目标仍由驱动缓存或每条配置的目标名决定。
- 骨骼模式没有重复触发保护，循环播放通知可能重复生成断肢。

## 4. 参数调优参考

### 4.1 冲击方向与力度

| 参数 | 说明 |
| --- | --- |
| `Impact Direction` | 会被**归一化**，所以只需关心方向，长度无意义 |
| `Impact Force` | 乘在归一化方向后作为速度增量（`bVelChange = true`），结果不随断肢质量变化 |
| `Angular Impulse Degrees` | 以度表示的角速度增量，直接传入世界空间，不随 `Limb Space` 转换；碰撞和物理约束也可能产生旋转 |
| `Limb Space` | 只控制线性方向：`BoneSpace`（默认）随断口骨骼朝向转换，整网格模式则随目标组件朝向转换；`WorldSpace` 直接用世界方向 |

`BoneSpace` 先通过带缩放的 `TransformVector`，然后归一化再乘力度。均匀正缩放不会放大力度；非均匀缩放可能改变方向，负缩放可能翻转方向，退化方向可能归零。`bUseWorldScale` 当前完全未被读取，切换它不会改变表现。

冲量使用 `NAME_None` 选择默认物理体，对骨骼网格是根物理体；不是对全部骨骼统一施加，也不使用命中点计算偏心扭矩。

### 4.2 物理时序

| 参数 | 默认 | 调整建议 |
| --- | --- | --- |
| `Physical Simulation Avoidance Time` | `0.1s` | 断肢仍与宿主挤压、弹飞或抖动时**调大**；设为 `0` 会退化为"下一帧恢复"，容易看到抖动 |
| `Physical Simulation Avoidance Collision Profile Name` | `Ragdoll` | 避让期间使用的预设，应选择一个与宿主不产生硬碰撞的预设 |
| `Limb Physics Simulation Time` | `5s` | 停止计时器的延迟为避让时间 + 此值；停止后不显式关闭碰撞，是否挡路取决于预设 |
| `Detached Limb Life Span` | `30s` | 断肢 Actor 存活时间；设为 `0` 表示不自动销毁（需要自行清理，否则会累积） |

实际的时间轴是：

```
断肢生成 ──0.1s──► 恢复碰撞 ──5s──► 停止物理模拟（可能悬停） ──剩余时间──► 销毁
                                    总计 30s
```

这是默认参数下的名义时间轴。时长 setter 会钳制负值；0 延迟使用 TimerManager 的下一次处理机会。LifeSpan 独立计时，如果短于物理计时器延迟，Actor 会先销毁。

### 4.3 碰撞预设

工程 `Config/DefaultEngine.ini` 中**没有定义任何自定义碰撞预设**，因此组件默认引用的 `Ragdoll` 与数据资产默认引用的 `BlockAllDynamic` 都来自引擎内置配置。具体通道响应请在 `项目设置 → 碰撞` 中确认。若项目使用了自定义碰撞体系，建议在这里替换成自己的预设。

## 5. 制作候选模型时的约定

1. **对齐 pivot**：骨骼模式以骨骼原点定位断肢，而两个网格组件使用单位相对变换。因此**模型的 pivot 必须放在断口处**，否则掉落物会明显偏移。这是最常见的"看起来不对"的原因。
2. **一个部位多个候选**：可以配置多个候选模型做随机变化，数组中的空引用会被安全跳过。
3. **注意缩放与物理资源**：模型继承断口世界变换；非均匀缩放还可能改变局部冲击方向。静态网格需要适合模拟的碰撞体，骨骼网格需要有效 Physics Asset；代码没有验证物理是否实际启动。
4. **静态网格与骨骼网格的选择**：
   - 静态网格：更省，适合形状固定的断肢。
   - 骨骼网格：激活 `Limb Skeletal Mesh`，可使用 Physics Asset 的骨骼物理；当前代码不复制宿主姿势或动画蓝图，不能假定保持断开瞬间的姿势。
5. **宿主与掉落物是独立资产**：系统不会自动继承宿主材质覆盖、速度，也不会自动切换到“没手”模型。这些需要另行接入。

## 6. 排查清单

现象 → 可能原因（按可能性排序）：

| 现象 | 排查方向 |
| --- | --- |
| 完全没反应，无日志 | 检查通知是否触发、Owner 是否有断肢组件、目标组件对象名、数据资产和键是否匹配；候选数组与 `Limb Mesh Type` 不一致也会在生成前返回失败 |
| 部位没消失但生成了断肢 | 骨骼不存在会在生成前失败，不能解释此现象；检查是否选错目标组件、模型蒙皮、Leader Pose/跟随关系或其他逻辑重置了隐藏状态 |
| 断肢生成了但模型为空/不可见 | 类型/数组不一致会在生成前失败；检查实际模型几何、材质、缩放、生成位置，以及生成后是否被其他逻辑修改 |
| 断肢位置明显偏移 | 候选模型的 pivot 不在断口处 |
| 断肢一生成就被弹飞/抖动 | 调大 `Physical Simulation Avoidance Time`；确认避让预设确实与宿主不硬碰 |
| 断肢掉落但没有线性飞出 | `Impact Force` 为 0 或最终方向为零；角冲量只控制旋转，不能代替线性冲量 |
| 断肢不模拟物理 | 检查候选模型碰撞体/Physics Asset、避让预设是否允许物理，以及引擎 Output Log；调用返回 `true` 不代表模拟成功 |
| 断肢挡路 | 停止模拟没有显式关闭碰撞；检查预设是否阻挡 Pawn，需要时缩短 `Detached Limb Life Span` |
| 整网格模式调用无效 | `Target Skeletal Mesh Component Name` 未填，或误填成了驱动网格名（会被硬性拒绝） |
| 想复原角色 | 整网格模式需要重建组件；骨骼模式可取消视觉隐藏，但终止的物理体需要另外重建/初始化 |

## 7. 扩展建议

若需要在现有实现上继续演进，以下几点与现有代码结构契合度较高：

1. **补日志**：`ApplyDismemberment` 的每个 `return false` 都无日志，是排查困难的主要来源。建议加一个专用日志分类并在各失败点输出原因。
2. **校验候选数组一致性**：让 `FSKMDismembermentConfig::Valid()` 或 `IsDataValid` 感知 `LimbMeshType`，在编辑器阶段就阻止"填错数组"。
3. **明确 `bUseWorldScale` 的语义后实现或删除**：目前完全未使用；须区分模型生成缩放与局部方向变换，避免把它误解为力度倍率。
4. **对象池**：当前只做了部分复位，每次仍新建 Actor。接入池化须补 LifeSpan、速度、物理组件挂接/变换和归还流程。
5. **生命周期收尾**：停止模拟不显式关闭碰撞。若不希望挡路，可在回调中关闭碰撞或切换为不阻挡 Pawn 的预设。
6. **状态与恢复**：增加重复断肢保护，并分别设计视觉隐藏、终止物理体和销毁组件的恢复流程。
