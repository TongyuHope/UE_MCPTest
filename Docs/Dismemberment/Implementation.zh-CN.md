# 断肢系统实现细节

> 逐类实现要点与代码观察。行号对应 2026-10-10 阅读时的源码，路径相对工程根目录。
> 本文档为静态阅读整理，未编译、未运行验证。
> 最近核对：2026-10-10。引擎行为辅助核对使用本机 `D:\UE_5.7\Engine\Source`；工程关联 UE 5.8，本次未定位到对应引擎源码，相关细节仍需在工程实际引擎版本中验证。

## 1. 类型清单

### 1.1 枚举与结构体

| 类型 | 所在文件 | 说明 |
| --- | --- | --- |
| `ELimbSpace` | `DismembermentComponent.h:15` | 冲击方向坐标系：`WorldSpace` / `BoneSpace` |
| `FDismembermentConfig` | `DismembermentComponent.h:23` | 单次断肢的冲击参数 |
| `EDismembermentLimbMeshType` | `DismembermentConfigData.h:15` | 断肢使用 `StaticMesh` 还是 `SkeletalMesh` |
| `FSKMDismembermentConfig` | `DismembermentConfigData.h:26` | 单根骨骼的候选模型列表 |

### 1.2 类

| 类 | 父类 | 生成宏 |
| --- | --- | --- |
| `UDismembermentConfigData` | `UDataAsset` | `BlueprintType` |
| `UDismembermentComponent` | `UActorComponent` | `ClassGroup=(Custom)`, `BlueprintSpawnableComponent` |
| `ADismembermentLimbActor` | `AActor` | 无特殊标记 |
| `UAnimNotify_ApplyDismemberment` | `UAnimNotify` | `DisplayName = "Apply Dismemberment"` |

## 2. `UDismembermentConfigData`（数据资产）

纯数据类，`DismembermentConfigData.cpp` 仅有一行 `#include`。

关键成员（`DismembermentConfigData.h`）：

- `DismembermentConfigs`（:97）：`TMap<FName, FSKMDismembermentConfig>`，**骨骼名 → 配置**。运行期以骨骼名为键查找，未登记的骨骼一律无法断肢。
- `LimbMeshType`（:100）：默认 `StaticMesh`。
- `DismembermentLimbCollisionProfile`（:104）：默认 `BlockAllDynamic`，构造时用 `FCollisionProfileName(TEXT(...))` 初始化。

`FSKMDismembermentConfig` 的三个方法：

- `Valid()`（:36）：两个数组中**任意一个**存在有效资源即返回真。注意它不区分 `LimbMeshType`，所以"配置有效"不等于"能生成成功"。
- `GetRandomDismembermentSkeletalMesh()`（:49）/ `GetRandomDismembermentMesh()`（:67）：先遍历统计有效项数量 `ValidCount`，再 `FMath::RandRange(0, ValidCount-1)` 取第 N 个有效项。这个两段式写法是刻意的——避免了"先随机下标再判空、判空失败就放弃"的常见缺陷，数组中间有空引用时仍能稳定取出资源。

## 3. `UDismembermentComponent`（核心组件）

### 3.1 构造与 `BeginPlay`

- 构造函数：`PrimaryComponentTick.bCanEverTick = false`，组件不参与 Tick。
- `BeginPlay()`（`.cpp:13`）：遍历 Owner 上所有 `USkeletalMeshComponent`，按 `GetName()` 与 `DrivenSkeletalMeshComponentName` 字符串比对，命中即缓存到 `DrivenSkeletalMeshComponent` 并跳出循环。注释指出驱动网格"只需在开始游戏时解析一次"。

缓存是 `UPROPERTY() TObjectPtr<USkeletalMeshComponent>`，受 GC 跟踪，并非裸指针或弱引用。缓存只在 `BeginPlay` 建立一次；销毁并重建组件后不会自动查找新组件。仅在同一组件上替换 SkeletalMesh 资产不会改变组件引用。运行时修改 `DrivenSkeletalMeshComponentName` 也不会自动刷新缓存。

### 3.2 `ApplyDismemberment()` 逐步解析（`.cpp:31`）

**第一步：基础校验（:34–38）**

```cpp
if (!IsValid(GetOwner()) || !IsValid(GetWorld())) return false;
if (!IsValid(DrivenSkeletalMeshComponent) && DismembermentConfig.TargetSkeletalMeshComponentName.IsNone())
    return false;
```

即：驱动网格未解析出来时，只有显式指定了目标网格名才能继续。

**第二步：解析目标网格（:40–55）**

默认取缓存的驱动网格；若 `TargetSkeletalMeshComponentName` 非空，则**放弃缓存、按名即时遍历查找**。这是为模块化网格准备的——每个模块是独立的 `USkeletalMeshComponent`，需要按名定位。

**第三步：整网格模式的合法目标校验（:56–61）**

```cpp
if (DismembermentConfig.bHideWholeTargetMesh &&
    (DismembermentConfig.TargetSkeletalMeshComponentName.IsNone() || TargetMesh == DrivenSkeletalMeshComponent))
{
    return false;
}
```

硬性约束：整网格模式**必须显式指定目标名**，且目标**不能等于缓存的驱动网格**。注释说明此模式面向跟随/模块化网格，但代码没有检查实际 Leader Pose 关系。驱动缓存未建立时，指针比较不能保护未识别的主体网格，因此必须先正确配置驱动网格。

**第四步：资源与配置校验（:63–94）**

依次校验：目标网格有 `SkeletalMeshAsset` → `DismembermentConfigData` 有效 → （骨骼模式下）`GetBoneIndex(BoneName) != INDEX_NONE` → 该骨骼在 `DismembermentConfigs` 中且 `Valid()` → 按 `LimbMeshType` 随机取出的资源有效。

整网格模式不校验骨骼存在性，`BoneName` 在此仅作为配置键。`LimbMeshType` 是数据资产级开关，同一资产中的所有条目共用它。

注意顺序：`DismembermentConfigData` 为空的校验（:68）排在骨骼存在性校验（:73）之前，而目标网格资源校验（:63）排在更前。任何一步失败都返回 `false`，**且不产生任何日志**。

**第五步：计算断口世界变换（:96–99）**

```cpp
const FTransform DismembermentWorldTransform = DismembermentConfig.bHideWholeTargetMesh
    ? TargetMesh->GetComponentTransform()
    : TargetMesh->GetBoneTransform(BoneName);
```

注释强调：对于独立的模块化网格，必须在销毁组件**之前**保存其世界变换。

**第六步：生成断肢（:101–106）**

```cpp
ADismembermentLimbActor* LimbActor = GetWorld()->SpawnActor<ADismembermentLimbActor>(
    ADismembermentLimbActor::StaticClass(), DismembermentWorldTransform);
```

直接把断口变换作为 Spawn 变换传入（会触发 Actor 的对齐与生成期初始化）。

**第七步：摘除宿主部分（:108–116）**

- 整网格模式：`TargetMesh->DestroyComponent()`，同时再次重复第三步的注释警告（"本模式面向独立的模块化肢体网格，不要用在 Leader Pose 网格上"）。
- 骨骼模式：`TargetMesh->HideBoneByName(BoneName, PBO_Term)`。隐藏影响该骨骼及子层级；`PBO_Term` 是物理体处理选项，表示终止对应层级的物理体，不是“终止姿势求值”。本机 UE 5.7 的 `USkeletalMeshComponent::HideBone` 调用 `TermBodiesBelow`，枚举注释明确提示取消隐藏不会重新初始化被终止的物理体。视觉隐藏效果还取决于模型蒙皮与 Leader Pose 等设置。

**第八步：生命周期与冲量（:118–143）**

先 `SetLifeSpan(DetachedLimbLifeSpan)`，再按 `LimbSpace` 求最终方向：

- `WorldSpace`：直接使用 `ImpactDirection`。
- `BoneSpace`：`DismembermentWorldTransform.TransformVector(ImpactDirection)`，随后在 `AddImpulse` 中归一化。均匀正缩放不会放大最终力度；非均匀缩放可能改变方向，负缩放可能翻转方向，退化向量可能归零。整网格模式用的是组件变换，因此此时 `BoneSpace` 实际表示目标组件局部空间。
- `default: checkNoEntry()`：枚举只有两个值，此分支不可达。

`AngularImpulseDegrees` 直接传入断肢 Actor，没有经过 `LimbSpace` 转换。`bUseWorldScale` 没有读取点，不能控制 Actor 生成缩放或冲量变换。

最后按固定顺序调用断肢的四个方法（顺序有语义，见 4.2 节）：

```cpp
LimbActor->ReInitializeLimb(...);                    // 装载网格、设碰撞预设、置 ActiveLimbComponent
LimbActor->SetPhysicalSimulationAvoidance(...);      // 写入避让参数
LimbActor->SetLimbPhysicsSimulationTime(...);        // 写入物理时长
LimbActor->AddImpulse(...);                          // 真正开物理并施加冲量
```

## 4. `ADismembermentLimbActor`（掉落的断肢）

### 4.1 组件布局（`.cpp:11`）

```
SceneRoot (USceneComponent, RootComponent)
├── LimbMesh         (UStaticMeshComponent)     默认隐藏、无碰撞
└── LimbSkeletalMesh (USkeletalMeshComponent)   默认隐藏、无碰撞
```

两个网格组件都直接挂在根节点并使用**单位相对变换**，因此 Actor 变换即为断肢的定位依据。构造函数同时关闭了两者的可见性与碰撞。

### 4.2 `ReInitializeLimb()`（`.cpp:27`）

执行序列：

1. 清掉 `ReInitializeTimerHandle` 与 `PhysicalSimulationAvoidanceTimerHandle`（:31–35），支持重复初始化时撤销上一轮的物理计时器。这是复用准备，不等于已实现对象池；本函数没有清理 Actor 的 LifeSpan，也没有归还池或重新挂接物理脱离组件的逻辑。
2. `SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics)`（:36）：用 `TeleportPhysics` 瞬移，不让物理插值拉扯。
3. 记录 `CurrentCollisionProfileName`，把 `ActiveLimbComponent` 置空（:37–38）。
4. **两个网格组件都彻底复位**（:40–51）：隐藏 + `NoCollision` + `SetSimulatePhysics(false)`。注释指出这是为保证重复初始化不会继续参与碰撞或模拟。
5. 按类型点亮其中一个（:52–73）：
   - `StaticMesh` 分支：`SetStaticMesh` → 单位相对变换 → 可见 → 设碰撞预设 → `QueryAndPhysics` → 关物理模拟 → `ActiveLimbComponent = LimbMesh`。
   - `SkeletalMesh` 分支：同理，但多一步 `SetSimulatePhysics(false)` 置于设碰撞预设之前，`ActiveLimbComponent = LimbSkeletalMesh`。

`ActiveLimbComponent`（头文件 :48–49）是这套设计的关键：注释写明"当前实际承载断肢资产的组件，避免用 Visibility 推断物理目标"。后续所有物理与碰撞操作都作用于它，而不是猜哪个网格可见。

**调用顺序的语义**：`ReInitializeLimb` 先应用最终预设并显式开启 `QueryAndPhysics`，`AddImpulse` 内部的 `StartLimbPhysicsSimulation()` 随即切到避让预设，计时器再恢复最终预设。首次初始化若先调用 `AddImpulse`，`ActiveLimbComponent` 为空，物理启动会被跳过。重复初始化也必须遵循这套顺序。

### 4.3 `AddImpulse()`（`.cpp:78`）

```cpp
const FVector Impulse = ImpulseDirection.GetSafeNormal() * ImpactForce;
StartLimbPhysicsSimulation();
ActiveLimbComponent->WakeAllRigidBodies();
ActiveLimbComponent->AddImpulse(Impulse, NAME_None, true);
ActiveLimbComponent->AddAngularImpulseInDegrees(AngularImpulseDegrees, NAME_None, true);
```

- 方向先归一化再乘力度，所以 `ImpactDirection` 的长度不影响结果，只影响方向。
- `NAME_None` 指定默认物理体；对 SkeletalMesh，API 文档说明是根物理体，并非对所有骨骼施加冲量。骨骼名参数用于选择物理体，不是指定冲击位置。当前没有使用 `AddImpulseAtLocation`，所以不按命中点产生偏心扭矩；碰撞与物理约束仍可能引起旋转。
- 两个冲量的 `bVelChange` 参数均为 `true`，即按**速度增量**施加，结果不随质量变化。
- `WakeAllRigidBodies()` 在 `SetSimulatePhysics(true)` 之后调用，确保休眠中的刚体被唤醒。若 `ActiveLimbComponent` 无效，打一条 `LogTemp` 警告 `"LimbMesh is not valid!"`——注意这条日志对骨骼网格分支也复用了同样的文案。

### 4.4 物理时序编排（`.cpp:108`）

`StartLimbPhysicsSimulation()` 是时序中枢：

```cpp
DisableLimbCollision();                                  // 切到避让预设
if (PhysicalSimulationAvoidanceTime > 0.0f)
    SetTimer(..., EnableLimbCollision, PhysicalSimulationAvoidanceTime, false);
else
    SetTimerForNextTick(..., EnableLimbCollision);       // 退化路径

ActiveLimbComponent->SetSimulatePhysics(true);

const float StopPhysicsDelay = LimbPhysicsSimulationTime + PhysicalSimulationAvoidanceTime;
if (StopPhysicsDelay > 0.0f)
    SetTimer(ReInitializeTimerHandle, ..., StopLimbPhysicsSimulation, StopPhysicsDelay, false);
else
    SetTimerForNextTick(..., StopLimbPhysicsSimulation);
```

要点：

- 避让时间为 0 时使用 `SetTimerForNextTick`，安排在 TimerManager 的下一次处理机会执行，不应承诺固定在当帧末或下一渲染帧发生。
- 两个 setter 均用 `FMath::Max(0.0f, ...)` 钳制时长。`StopPhysicsDelay` 是避让时间与物理时间之和，会随避让时长增加。两个计时器在同一次启动中注册，停止计时不是等恢复预设回调执行后才开始。
- `SetTimerForNextTick` 的返回值同样被赋给对应的 `FTimerHandle`，因此仍可被后续 `ClearTimer` 撤销。
- 物理计时器注册在 World 的 TimerManager 上，并绑定 Actor 成员函数。LifeSpan 独立计时，可能早于或晚于物理计时器到期，不能认为它总是先触发。当前类没有自定义 `EndPlay` 清理，销毁时的对象绑定计时器处理依赖引擎生命周期机制。

### 4.5 三个回调（`.cpp:147`、`:159`、`:171`）

- `StopLimbPhysicsSimulation()`：`SetSimulatePhysics(false)`，**没有显式关闭碰撞，也没有调用休眠**。网格可能停在空中或落地位置；是否阻挡角色取决于当前预设与双方通道响应，不能一概称为静态障碍。Actor LifeSpan 到期后销毁。
- `EnableLimbCollision()`：`SetCollisionProfileName(CurrentCollisionProfileName)`，恢复数据资产里的最终预设。
- `DisableLimbCollision()`：`SetCollisionProfileName(PhysicalSimulationAvoidanceCollisionProfileName)`，切到避让预设。

三者都用 `IsValid(ActiveLimbComponent)` 包裹，且都声明为 `const`。

注意避让与恢复都只改**碰撞预设**，不动 `SetCollisionEnabled`。因此实际碰撞行为取决于两个预设各自的 `CollisionEnabled` 设置；若避让预设本身是 `NoCollision`，避让阶段就等同于完全穿透。

物理模拟也依赖有效碰撞体/Physics Asset 和允许物理碰撞的预设。函数名 `DisableLimbCollision` 实际只切换预设，并不保证关闭全部碰撞。

## 5. `UAnimNotify_ApplyDismemberment`（动画触发）

- 构造函数（`.cpp:10`）：`NotifyColor = FColor(197, 23, 46)`（暗红），仅在 `WITH_EDITORONLY_DATA` 下编译。
- `Notify()`（`.cpp:17`）：校验 `MeshComp` 与 `MeshComp->GetOwner()` → `FindComponentByClass<UDismembermentComponent>()` → 遍历 `ApplyDismembermentConfigs`，把通知级的 `bHideWholeTargetMesh` 用 `|=` 合并进每条配置，然后逐条调用 `ApplyDismemberment`。
- 一个通知即可一次性断多根骨骼，键是骨骼名。
- `ApplyDismembermentConfigs` 使用 `meta = (ForceInlineRow)`，在细节面板里以紧凑行展示 `TMap`。
- 编辑器校验 `IsDataValid()`（`.cpp:42`）：配置为空时报错并返回 `Invalid`，这是该文件里唯一的错误提示机制。
- `GetNotifyName_Implementation()` 返回 `"Apply Dismemberment"`。

通知忽略的是 `ApplyDismemberment()` 的布尔返回值；`Notify()` 本身返回 `void`。单条失败不会中断其他配置。`TMap` 遍历没有业务顺序保证；若多个条目引用同一个整网格目标，第一次销毁后后续条目可能失败。当前批处理没有汇总结果或失败回滚。

## 6. 实现层面的已知问题与观察

按影响程度排序。前四条属于"会导致功能不符预期"，后六条属于代码质量与边界问题。

### 6.1 影响功能

1. **驱动网格按运行时名称查找，失败无提示（高）**
   `DrivenSkeletalMeshComponentName` 默认 `"CharacterMesh0"`（`DismembermentComponent.h:69`）。本机 UE 5.7 的 `Character.cpp` 中，`ACharacter::MeshComponentName` 就是 `CharacterMesh0`；`Mesh` 是成员名/常见编辑器显示名，不应直接替代对象名。本工程角色继承 `ACharacter`，构造函数未重命名其网格；没有依据判定默认值必然错误。实际蓝图和 UE 5.8 中请以组件 `GetName()` 为准。未解析到驱动网格且未显式指定目标时，调用会静默返回 `false`。

2. **`bUseWorldScale` 是未使用字段（中）**
   `FDismembermentConfig::bUseWorldScale`（`DismembermentComponent.h:46–47`）在整个 `DetachedLib` 中没有读取点。生成始终使用断口世界变换，局部方向始终用 `TransformVector` 后归一化，该开关目前不起作用。后续实现前须明确它控制模型生成缩放还是方向变换。

3. **候选数组与 `LimbMeshType` 不一致时静默失败（中）**
   `FSKMDismembermentConfig` 允许同时填 `DismembermentMeshes` 与 `DismembermentSkeletalMeshes`，但运行期只读 `LimbMeshType` 指定的那个。只填了另一个数组时 `Valid()` 仍返回真，直到随机取模返回空指针才在校验处失败（`DismembermentComponent.cpp:88`、`:93`），同样没有日志。建议在 `IsDataValid` 或 `Valid()` 中加入一致性检查。

4. **断肢位置取决于模型 pivot 与骨骼原点的对齐（中）**
   骨骼模式以 `GetBoneTransform(BoneName)` 作为 Actor 变换，两个网格组件自身是单位相对变换。因此断肢的最终位置完全由**候选模型的 pivot**决定：若模型 pivot 不在断口处，掉落物会明显偏移。这不是 bug，但需要在制作候选模型时约定好 pivot 位置。

### 6.2 代码质量与边界

5. **互斥网格分支对另一组件的解引用未判空（低）**
   StaticMesh 分支 `:59` 直接使用 `LimbSkeletalMesh`，SkeletalMesh 分支 `:71` 直接使用 `LimbMesh`。两个组件均由构造函数创建，常规路径风险低，但分支只检查了本分支的组件。

6. **`ReInitializeTimerHandle` 命名具有误导性（低）**
   该句柄实际承载的是“停止物理模拟”的定时器（`:132`），与“重新初始化”无关。当前源码无法证明其命名历史。

7. **`StopLimbPhysicsSimulation` 是"空中冻结"而非"休眠"（低）**
   停模拟没有显式修改碰撞；当前预设可能阻挡角色，持续到 `LifeSpan` 到期。实际预设由资产配置决定，并不固定为 `BlockAllDynamic`。

8. **整网格模式不可逆（低）**
   `DestroyComponent()` 后需要重建组件或重生 Actor；骨骼模式可以取消视觉隐藏，但不能用 `UnHideBoneByName` 自动恢复 `PBO_Term` 终止的物理体。当前库没有提供完整恢复 API。

9. **对象池能力当前无调用者（低）**
   `ReInitializeLimb` 支持部分状态复位，但组件每次都新建 Actor。若接池化，还须处理 LifeSpan、物理速度、组件重新挂接与变换、资源残留和归还流程；当前复位不能视为完整对象池实现。

10. **`LogTemp` 日志文案与实际组件不一致（很低）**
    `AddImpulse`、`StartLimbPhysicsSimulation` 和三个回调共五处统一打印 `"LimbMesh is not valid!"`，在骨骼网格分支下文案不准确；同时使用 `LogTemp` 而非专用日志分类。

### 6.3 尚未实现的行为

- 没有记录已断骨骼，同一骨骼重复调用可能重复掉落；父子骨骼批量断肢还会受隐藏后的变换影响。
- SkeletalMesh 断肢没有宿主姿势复制、动画蓝图设置或材质覆盖继承，不能假设保持被斩断瞬间的姿态。
- 没有命中点冲量、宿主速度继承、运行时网格切割、断口封面生成或身体网格切换。
- 没有落地检测、网络复制/RPC 或服务器权威断肢逻辑。
- `ApplyDismemberment` 不验证物理是否实际启动；生成 Actor 后才隐藏宿主，后续物理异常没有回滚路径。

## 7. 与引擎版本的注意点

工程为 UE 5.8（`UE_MCPTest.uproject` 的 `EngineAssociation`），渲染侧启用了 DX12、Lumen、虚拟阴影贴图、光线追踪与 Substrate（见 `Config/DefaultEngine.ini`）。断肢是动态生成的物理 Actor，会受这些特性的常规影响（例如 Substrate 下候选模型的材质需要相应支持）。

`Config/DefaultEngine.ini` 中**没有定义任何自定义碰撞预设**，因此组件默认引用的 `Ragdoll` 与数据资产默认引用的 `BlockAllDynamic` 都来自引擎内置配置。关于这两个预设的具体通道响应（例如 `Ragdoll` 默认忽略 `Pawn` 的说法），请以编辑器 `项目设置 → 碰撞` 中显示的实际值为准。
