# BiomePCG 样条绘制工具：首版使用说明

## 当前交付状态

已添加源码实现，包括 Runtime Actor/样条组件和独立的编辑器绘制模式。
按约定没有运行 C++ 编译、编辑器加载或交互验证；以下是编译后待验收的使用流程。
没有创建、修改或执行用户 PCG 图，也没有在当前工程中生成蓝图或写入关卡资产。

## 1. 编译与打开工具

1. 编译 `UE_MCPTestEditor / Win64 / Development`，重新打开工程。此次包含新的反射类型，建议关闭编辑器后完整编译。
2. 在顶栏选择 `TA Tools → 绘制样条植被`。
3. 也可以在编辑模式下拉菜单选择 `Biome 样条绘制`。
4. 左侧应出现工具设置面板。进入 PIE 时绘制模式自动退出。

## 2. 放置与绘制

1. 保持“目标 Actor”为空，在有碰撞的 Landscape 或 StaticMesh 表面单击左键。
2. 该次单击只放置 Actor，不生成样条。
3. 再次按住左键拖动，黄色折线显示尚未提交的控制点预览。
4. 松开左键，创建一条独立的 `UBiomePaintedSplineComponent`，平滑开关决定提交后的曲线类型。
5. 再次按住左键会创建另一条新样条，归属同一个 Actor，不与上一条连接。

“放置蓝图类”留空时，首次放置会创建或复用：

```text
/Game/PCG/BP_PCG/BP_BiomeSplinePainter
父类：ABiomeSplinePainterActor
```

工具创建的蓝图和关卡会标记为待保存，请自行保存。
默认路径已有不兼容资产时会拒绝覆盖，可以在面板指定另一个继承自 `ABiomeSplinePainterActor` 的蓝图类。

若要继续编辑已有 Actor：先选中该 Actor 再进入工具，或通过面板“目标 Actor”选择实例。
也可以选中实例后点击“使用选中的绘制 Actor”。
点击“放置新的 Actor”会清空当前目标，下一次单击放置另一个 Actor。

## 3. 接入自己的 PCG 图

可以在面板“用户 PCG 图”指定资产，再点击“将图和自动刷新设置应用到目标”。
也可以退出工具，在 Actor 的 `PCGComponent` 中直接指定 Graph。

- 面板中的图只在新建 Actor 或点击应用按钮时写入目标。
- 点击应用时，空 Graph 代表清除目标 Graph；不会自动创建 PCG 图。
- 未指定 Graph 时照样可以绘制、保存、删除样条。
- Actor 的 PCG 组件默认使用 `GenerateOnDemand`。提交后自动刷新默认开启，也提供手动刷新。
- 若用户将组件改成 `GenerateAtRuntime`，工具不抢占运行时生成调度；首版编辑器预览请使用默认 OnDemand 设置。

PCG 读取接口：

| 项目 | 值 |
| --- | --- |
| 输入来源 | 当前 Actor / Self |
| 组件类型 | USplineComponent 的派生类 `UBiomePaintedSplineComponent` |
| Component Tag | `BiomePaintedSpline` |
| 多线输出 | 需要读取全部目标 Spline，不只读取第一个 |
| 点生成规则 | 由用户图中的 Spline Sampler 等节点实现 |
| 每条线标识 | 组件上保存 `StrokeId` GUID 和可编辑 `StrokeLabel` |

`GetPaintedSplines` 也暴露为普通蓝图可调用的纯函数。
工具不约束植被间距、植物种类、Mesh 列表或图参数名称。
控制点间距只影响绘制形状，不等同于 PCG 图里的植被间距。

## 4. 编辑操作

| 操作 | 行为 |
| --- | --- |
| Esc，笔画中 | 取消当前临时笔画，不留下组件 |
| Esc，空闲时 | 退出工具 |
| 完成绘制 | 退出工具，保留已提交的内容 |
| Ctrl+Z / Ctrl+Y，空闲时 | 使用编辑器事务撤销 / 重做 |
| Ctrl+Z / Ctrl+Y，笔画中 | 取消当前临时笔画 |
| 点击样条列表中的一条线 | 蓝色高亮该条已保存样条 |
| 删除选中样条 | 删除当前目标上高亮的线，可撤销 |
| Alt+鼠标 / 右键 | 保留常规视口导航 |

拖动期间只绘制临时预览，松开后才写入持久化组件并请求 PCG 刷新。
刷新请求会短暂合并；当前组件正在生成或清理时等待其结束，不为每次鼠标移动启动生成。

结束绘制模式后，可使用普通 Spline 控制点编辑已保存的样条。
组件属性编辑、控制点编辑、Actor 移动完成及撤销会通知 PCG 更新。
用户自己的蓝图 Construction Script 不应删除或覆盖手绘实例组件。

## 5. 地表与中断处理

- 默认使用 Visibility 通道，地表必须具有对应的碰撞响应。
- 自动忽略目标 Actor 自身，并可忽略带 PCG 生成标记的 Actor/组件，避免在植被顶部继续绘制。
- 鼠标无命中时不加点；跨越距离超过设置值时结束当前有效笔画，松开左键后才能开始下一条。
- 短点击或不足最小长度的笔画会被丢弃。
- 视口失去焦点、切换地图或开始 PIE 会清理未提交的临时笔画。
- 平滑曲线只保证控制点在命中位置；控制点之间可能穿过地面，最终植被贴地应由用户 PCG 图处理。

默认控制点最小间距 100 cm，最短笔画 10 cm，最大连续跨越距离 2000 cm。
最大跨越距离的实际下限为两倍控制点最小间距。

## 6. 源码位置

```text
Plugins/BiomePCG/Source/BiomePCGRuntime/
  Public/BiomeSplinePainterActor.h
  Private/BiomeSplinePainterActor.cpp
  Public/BiomePaintedSplineComponent.h
  Private/BiomePaintedSplineComponent.cpp

Plugins/BiomePCG/Source/BiomePCGEditor/Private/SplinePainter/
  BiomeSplinePainterSettings.h
  BiomeSplinePainterEdMode.h/.cpp
  BiomeSplinePainterToolkit.h/.cpp
```

绘制界面使用插件自己的 `FEdMode` 和模式面板，不依赖引擎 Private 目录中的 PCG Draw Spline 实现。
Editor 模块新增 Runtime、PropertyEditor、ToolMenus、InputCore、EditorFramework 依赖。

## 7. 用户验收顺序

1. 无 Graph 放置 Actor；确认第一次点击没有样条。
2. 连画三次，确认一个 Actor 中有三条独立组件。
3. 验证短笔画、Esc 取消、跨空洞和视口导航。
4. 验证整笔撤销 / 重做、列表选中、删除及恢复。
5. 保存关卡和自动创建的蓝图，重开关卡检查样条。
6. 复制 Actor、编译其蓝图后检查样条实例是否保留。
7. 指定用户图，确认 Get Spline Data 读取全部三条线，逐条按距离生成植被。
8. 结束绘制后修改原生控制点，检查 PCG 刷新是否符合预期。

上述项目尚未执行；完整编译和实际场景交互结果以用户后续反馈为准。
