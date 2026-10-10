# 断肢系统接入与使用指南

> 更新：2026-10-10。用户已反馈首轮验证通过；随后修复的蓝图模板校验误报待用户重新构建确认。
> 旧资产只在未来加载时自动转换，本轮没有写入 .uasset。

## 1. 先构建，再处理旧动画

1. 在 IDE 完整构建 UE_MCPTestEditor。新增/移动了反射类型，建议关闭编辑器后构建，再重新打开工程。
2. 打开已有断肢动画。旧通知参数会转入 DismembermentRequests，日志分类为 LogDismemberment。
3. 核对部位、方向、力度、角冲量、目标名和摘除模式；旧参数默认都是显式覆盖，不会改用新资产默认值。
4. 保存动画资产，持久化迁移。加载时迁移不会在本轮自动写回磁盘。
5. 播放现有 upperarm_r 流程，对照原测试效果。

旧的每条整网格开关与通知级整网格开关按 OR 保留效果。原 bUseWorldScale 从未起作用，迁移后为 true，保持此前始终继承模型尺度的行为。迁移后的角冲量默认仍使用世界空间。

当前反射类型仍属于 UE_MCPTest 模块，原 ApplyDismemberment 蓝图节点和 Config 字段名称保留；无需为了移动头文件改蓝图节点名称。

若蓝图或 C++ 直接调用旧 ApplyDismemberment，bUseWorldScale 现在会实际生效：原来传 false 仍继承尺度，现在会生成单位尺度。需要保持旧效果时将它设为 true；上述自动迁移仅处理旧动画通知。

## 2. 当前用户已测试的基线

以下是用户提供的信息，本轮没有重新运行验证：

| 项目 | 配置 |
| --- | --- |
| DA 配置键 | upperarm_r |
| LimbMeshType | SkeletalMesh |
| DismembermentSkeletalMeshes | “右手”骨骼网格 |
| DismembermentMeshes | 空 |
| 碰撞 | 默认 |
| BP 组件对象名 | 用户确认正确，具体字符串未提供 |
| 现有流程 | 用户已测试可用 |

因此先保持组件 LimbMeshTypeOverride = UseAssetDefault。当前静态数组为空，直接改 StaticMesh 会明确失败，需要先制作/添加静态断肢候选。

## 3. 数据资产配置

在 DA_Dismemberment 的每个部位条目填写：

- DismembermentMeshes：静态候选，需要适合模拟的简单碰撞。
- DismembermentSkeletalMeshes：骨骼候选，需要有效 Physics Asset。
- DefaultConfig：默认目标组件、摘除模式、方向、力度、角冲量与空间。
- ImpactPresets：可选的命名冲击表现，例如 Light、Strong；这些预设只改变冲击表现。

候选模型必须对齐断口 pivot。空引用会跳过；物理资源缺失会报出具体模型名。资产默认 LimbMeshType 决定 UseAssetDefault 所选数组；另一数组允许为空。

命名预设不会改变摘除模式、目标组件或生成尺度。两种模型可以有不同外观，但应分别验证 pivot、尺度与碰撞；静态类型的实际性能需要在目标设备测量。

## 4. 动作组摆新通知

1. 在动画需要断肢的帧添加 Apply Dismemberment 通知。
2. Authoring Config Data 选择角色对应的配置资产，用于部位/预设下拉与编辑器校验。
3. Dismemberment Requests 添加条目，选择 Bone Name（数据资产键）。
4. Preset Name 选 None 使用部位默认表现，或选择对应部位的命名预设。
5. 通常无需展开高级覆盖；需要动画独有表现时勾选相应覆盖组并填写 OverrideConfig。

| 覆盖组 | 生效字段 |
| --- | --- |
| Override Impact | 方向、力度、角冲量、线性空间、角冲量空间 |
| Override Removal Mode | 摘除模式 |
| Override Target Mesh | 目标组件对象名 |
| Override World Scale | 模型是否继承断口尺度 |

未勾选的覆盖组不生效，即使 OverrideConfig 中有值。勾选后 0 力度是明确的零力度，空目标名表示使用驱动网格。

预设下拉展示该资产所有部位的预设名称；选择后校验它是否存在于当前部位。Authoring Config Data 只供编辑器使用，运行时始终读取播放者 Owner 上断肢组件的资产，不会用通知引用替换角色配置。

一个通知可以填多个请求，按数组顺序执行。每条失败都有日志，批量成功数会汇总；同一通知重复填写相同部位会在编辑器校验时报错。

## 5. 角色组件配置

- DismembermentConfigData：角色实际使用的数据资产。
- DrivenSkeletalMeshComponentName：主体组件的运行时对象名，默认 CharacterMesh0。不要仅凭编辑器显示的 Mesh 改名。
- 驱动名留空：ACharacter 自动使用主网格，普通 Actor 只有一个骨骼网格时自动选择；多个候选时应明确填写。
- LimbMeshTypeOverride：UseAssetDefault / StaticMesh / SkeletalMesh，由角色实例或游戏质量设置选择。
- 避让、模拟和寿命参数可先保持默认。

类型覆盖只影响本角色，不修改共享数据资产。不会自动回退另一类资源，也没有自动设备分档规则；这些策略由游戏初始化逻辑设置。

双候选数组仍是硬引用。若低端设备需要降低资产驻留内存，还需分档资产或按需加载方案，不能仅用类型开关判断内存收益。

## 6. 两种摘除模式

| 模式 | 适用与要求 |
| --- | --- |
| HideBone | 隐藏指定骨骼及子层级，终止对应物理体；直接作用于驱动姿势的网格 |
| DestroyWholeMeshComponent | 销毁独立模块网格；必须显式指定目标、成功解析驱动网格 |

整组件模式保护驱动网格、Character 主网格及其他组件依赖的 Leader Pose 网格。骨骼隐藏不能直接用在 Leader Pose 跟随网格上。

模型不自动切割，也不自动切换成“没手”资产。UnHideBoneByName 不能自动重建 PBO_Term 终止的物理体；当前没有完整接回接口。

## 7. 调整方向与物理参数

| 字段 | 说明 |
| --- | --- |
| ImpactDirection / ImpactForce | 方向归一化后乘力度，以速度增量施加 |
| LimbSpace | WorldSpace 世界方向；BoneSpace 断口局部方向；CharacterSpace 角色方向 |
| AngularImpulseDegrees | 角速度增量，以度表示 |
| AngularImpulseSpace | 独立选空间，默认 WorldSpace；局部角冲量只旋转，不带缩放 |
| bUseWorldScale | 控制掉落模型生成尺度，false 使用单位尺度 |

BoneSpace 在整网格模式使用组件变换。线性方向的非均匀缩放可能改变方向，均匀正缩放不会放大归一化后的力度。NAME_None 选择默认物理体，骨骼网格上为根物理体，不对全部骨骼分别施加命中点冲量。

默认时间：0.1 秒恢复最终碰撞，5.1 秒停止模拟，30 秒销毁。停止模拟仍保留碰撞，可能悬停或挡路；是否阻挡角色取决于预设。0 延迟使用 TimerManager 下一次处理机会，LifeSpan 独立计时。

## 8. 排查与验证

调用 Dump Dismemberment Diagnostics，或在 Output Log 过滤 LogDismemberment。

蓝图编译时，断肢组件模板没有完整的运行时 Owner/网格集合。组件校验现已在模板上下文跳过宿主查找，继续校验配置与资源；实际角色实例和断肢调用仍检查驱动网格、目标骨骼及主网格保护。若此前编译 BP_Dismemberment 出现 Driven mesh could not be resolved / Target skeletal mesh component/asset could not be resolved，构建修复代码后重新编译蓝图。若在 PIE 中仍出现同类日志，应检查实际组件对象名和网格配置。

| 问题 | 排查 |
| --- | --- |
| 部位/预设选不到 | 给通知指定 Authoring Config Data；确认键已配置 |
| Invalid part/preset | 运行时角色资产与通知选择不匹配 |
| 无静态候选 | 为对应部位补 DismembermentMeshes，或选择 SkeletalMesh |
| 模拟无法启动 | 检查 StaticMesh 简单碰撞、Physics Asset、预设与组件模拟状态；宿主会保留 |
| 主体销毁被拒绝 | 整组件模式选到了主网格/Leader Pose 来源，或驱动尚未正确解析 |
| 部位位置不对 | 检查实际宿主组件、骨骼原点、候选模型 pivot 与尺度 |
| 旧通知没有预设下拉 | 旧通知仍可运行；选择 Authoring Config Data 后再转用默认/预设 |
| 调了覆盖值没变化 | 确认对应覆盖组已经勾选 |

IDE 构建后可以在编辑器 Automation 窗口运行 UE_MCPTest.Dismemberment 测试组。PhysicsPathsAndHostProtection 使用现有 DA 和临时 Cube 验证两类路径；随后仍需用正式模型做视觉与手感验收。

助手未执行上述测试，用户已反馈首轮验证通过。当前待确认蓝图模板校验修复：IDE 构建后重新编译 BP_Dismemberment，并确认既有断肢流程仍可用。
