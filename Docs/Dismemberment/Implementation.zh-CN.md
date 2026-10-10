# 断肢系统实现细节

> 更新：2026-10-10。用户已反馈首轮验证通过；随后发现并修复蓝图组件模板校验误报，此修复待用户重新构建确认。自动化测试未由助手执行。
> 工程关联 UE 5.8，本机引擎路径 E:\UE_5.8。编译和运行验证由用户负责。

## 1. 共享类型

DismembermentTypes.h 保留 FDismembermentConfig 与 ELimbSpace 的反射名称、原字段名称及 WorldSpace/BoneSpace 枚举值。移动头文件不会改变模块中的反射类型名。

| 类型 | 用途 |
| --- | --- |
| ELimbSpace | WorldSpace、BoneSpace、CharacterSpace |
| ELimbRemovalMode | HideBone、DestroyWholeMeshComponent |
| EDismembermentLimbMeshTypeOverride | UseAssetDefault、StaticMesh、SkeletalMesh |
| FDismembermentConfig | 最终摘除与冲击参数，兼容原蓝图入口 |
| FDismembermentImpactPreset | 仅冲击参数的命名预设 |
| FDismembermentRequest | 部位/预设 + 四组显式覆盖开关 |

原 bHideWholeTargetMesh 保留为隐藏的序列化/蓝图兼容字段。直接调用时 GetRemovalMode 仍将其 true 解释为整网格销毁；新编辑面板使用 RemovalMode。

四组请求覆盖分别是 Impact、RemovalMode、TargetMesh、WorldScale。冲击覆盖包含方向、力度、角冲量以及两种空间；覆盖组内零值均有效。

## 2. 配置数据资产

FSKMDismembermentConfig 保留原两个候选数组，新增 DefaultConfig 和 ImpactPresets。

ResolveRequest 在局部副本上依次应用默认值、命名冲击预设和显式覆盖，不修改共享数据资产。无效部位/预设返回具体错误；缺少预设不会被显式冲击覆盖掩盖。

ValidForMeshType 只检查指定类型是否有有效候选。原 Valid 与随机辅助方法继续保留，随机方法仍跳过空引用。

ValidateMesh 校验：

- StaticMesh：有 BodySetup 和简单碰撞几何，不能用 ComplexAsSimple 进行模拟。
- SkeletalMesh：有 Physics Asset，其中至少一个有碰撞几何的物理体对应模型骨骼。
- 实际能否启动模拟还由生成后的组件检查，资源检查不能替代运行验证。

ValidateCollisionProfile 要求预设存在并允许物理碰撞。ValidateConfig 检查数值有限、空间/模式合法、整组件模式的目标名非空。

数据资产 IsDataValid 检查默认类型的候选、已填写的两类模型物理资源、默认参数和预设。数据资产不持有宿主网格，宿主骨骼/组件关系由组件校验。

## 3. 核心组件

### 3.1 入口

| 接口 | 行为 |
| --- | --- |
| ApplyDismemberment(BoneName, Config) | 保留原入口，使用显式参数生成断肢 |
| ApplyRegisteredDismemberment(Request) | 从资产解析默认值/预设/覆盖，再调用原入口 |
| ApplyDismembermentRequests(Requests) | 按数组顺序逐条调用，返回成功数 |
| DumpDismembermentDiagnostics() | 输出专用诊断日志，蓝图可调用 |
| GetDismembermentDiagnostics(OutErrors) | C++ 收集诊断信息 |
| GetResolvedLimbMeshType() | 返回本组件最终模型类型 |

组件的 LimbMeshTypeOverride 默认 UseAssetDefault。类型选择发生在角色实例，不更改资产的 LimbMeshType，也不进行类型回退。

组件 IsDataValid 区分模板与角色实例：蓝图 SCS 组件模板、CDO/Archetype 以及没有有效 Owner 的上下文，只检查配置、所选候选资源、物理时间和碰撞预设。模板尚无构造完成的角色组件集合，不对它执行驱动网格、目标骨骼和 Leader Pose 关系检查。有效非模板角色实例仍执行这些检查；公开 GetDismembermentDiagnostics、DumpDismembermentDiagnostics 及实际断肢入口始终保留宿主检查。

### 3.2 驱动与目标解析

指定驱动名时按组件 GetFName 查找。缓存仍是 UPROPERTY TObjectPtr；读取时检查有效性、Owner 和对象名，失效/名称变化后重新解析。

驱动名留空时：ACharacter 优先取主网格，普通 Actor 仅在一个有效骨骼网格组件时自动选择。多候选不猜测。

骨骼模式校验骨骼存在性，并拒绝 Leader Pose 跟随网格，因为直接隐藏其骨骼无法保证改变驱动姿势。

整组件模式要求独立的显式目标、可解析的驱动网格，并保护以下对象：

- 已解析的驱动网格；
- ACharacter 的真实主网格，即使驱动名指向其他组件；
- Owner 上其他骨骼网格组件依赖的 Leader Pose 来源。

### 3.3 生成顺序

1. 验证 Owner/World、部位键、参数、目标、配置资产和所选候选类型。
2. 验证时长及其总和有限且非负、碰撞预设存在且允许物理。
3. 随机选择一个所选类型的有效资源并检查物理资源。
4. 保存断口变换，按 bUseWorldScale 决定模型生成尺度。
5. 计算线性和角冲击世界向量，生成并初始化断肢。
6. 切换避让预设、启动模拟；失败则销毁新 Actor，保留宿主。
7. 成功后才隐藏骨骼或销毁目标组件，并设置断肢 LifeSpan。

结果 true 表示流程已成功启动；视觉对齐、姿态、碰撞手感仍需运行检查。PBO_Term 终止物理体，不能靠 UnHideBoneByName 完整恢复。

### 3.4 冲击语义

线性方向在 BoneSpace 使用 CutTransform.TransformVector 后归一化，CharacterSpace 用宿主 Actor 朝向，WorldSpace 直接使用。

AngularImpulseSpace 独立于 LimbSpace。局部角冲量只旋转，不受缩放影响，默认 WorldSpace 兼容旧数据。两个冲量仍使用 bVelChange=true，质量不改变输入速度增量。

bUseWorldScale 控制生成模型尺度；false 使用单位尺度。局部线性方向变换仍采用原断口尺度，避免将模型尺度开关混同为方向选项。

## 4. 动画通知与迁移

新通知暴露：

- AuthoringConfigData：编辑器选择/校验资产，WITH_EDITORONLY_DATA，Cook 时剔除。
- DismembermentRequests：有顺序的请求数组；每条选择部位与表现预设，高级覆盖折叠。

部位选项来自 AuthoringConfigData 的配置键；预设选项是所有部位预设名的去重集合，包含 None。最终仍检查预设是否属于所选部位。运行时资产始终来自 Owner 的断肢组件。

PostLoad 调用 MigrateLegacyConfigurations，Notify 也为 C++ 手工设置的旧 map 提供迁移入口：

1. 按骨骼名排序旧 TMap 键，建立稳定请求顺序；旧 map 本身没有业务顺序保证。
2. 每条旧参数转换为四组显式覆盖。
3. 每条旧布尔 OR 通知级旧布尔映射为新 RemovalMode，清除兼容布尔。
4. 将旧 bUseWorldScale 设为 true：旧代码忽略该开关，始终继承生成尺度。
5. 清空旧 map 和通知级布尔，保证重复迁移不重复追加。
6. 若同部位已有新请求，以显式新请求为准并打印警告。

旧序列化字段仍以原名称保存声明，没有使用会跳过旧属性读取的 DeprecatedProperty 标记。旧动画加载后在内存中迁移，需手工保存动画资产才能持久化；本轮未重存任何资产。

没有 AuthoringConfigData 的旧通知仍可运行，编辑器校验仅警告下拉/资产交叉检查不可用。通知检测空请求、重复部位、无效参数以及有资产上下文时的无效部位/预设；无 Actor 上下文时不伪造宿主检查。

## 5. 掉落 Actor

仍保留 SceneRoot + 静态/骨骼双组件及 ActiveLimbComponent。

ReInitializeLimb 清理两个物理计时器及旧 LifeSpan，停模拟并清速度，将可能脱离根节点的组件重新挂接，重置相对变换，清除未使用类型的资源引用。它为复用改善了清理，但没有对象池管理。

AddImpulse/StartLimbPhysicsSimulation 返回 bool，检查 active component、World 和模拟是否实际启动。专用日志替代 LogTemp。EndPlay 清理计时器。

默认停止模拟仍保留碰撞；没有落地检测或新收尾策略，避免改变当前测试效果。

## 6. 自动化测试与待验证项

测试源位于 Private/DetachedLib/Tests，仅在 WITH_DEV_AUTOMATION_TESTS 且 WITH_EDITOR 时编译。编辑器测试组：UE_MCPTest.Dismemberment。

| 测试 | 覆盖 |
| --- | --- |
| RequestResolution | 默认/预设/覆盖优先级、零值、无效键、共享资产不变 |
| MeshTypeIsolation | 两实例类型隔离、缺类型资源、空引用、缺 Physics Asset |
| LegacyNotificationMigration | 序列化旧字段、OR 语义、参数保留、尺度兼容、幂等性 |
| AuthoringOptionsAndValidation | 部位/预设选项、整组件目标规则、无效碰撞预设 |
| BlueprintTemplateValidation | 无 Owner 的组件模板不误报网格缺失，同时保留参数、候选资源和碰撞预设校验 |
| PhysicsPathsAndHostProtection | 加载实际 DA，使用临时物理 World 测试静态/骨骼路径及主网格保护 |

最后一项依赖工程现有 DA_Dismemberment、upperarm_r 骨骼候选和 Engine BasicShapes Cube，不保存这些资源。静态 Cube 仅用于功能测试，不能代替正式静态断肢资源。

用户已反馈首轮验证通过，未明确自动化测试执行范围。蓝图模板校验修复及新增回归测试未编译或执行，需用户 IDE 构建后重新编译 BP_Dismemberment；上述测试可在 Automation 窗口运行。
