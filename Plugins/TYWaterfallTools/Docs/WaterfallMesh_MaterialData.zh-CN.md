# 瀑布网格材质数据协议

本文档说明 `TYWaterfallMeshComponent` 写入 Dynamic Mesh，并由第十阶段烘焙到 Static Mesh 的 UV 和 Vertex Color 数据。材质应使用 `TextureCoordinate`、`VertexColor` 和必要的缩放/偏移节点读取这些数据。

## 总体约定

- 网格最多包含 4 个 UV Channel：`UV0`、`UV1`、`UV2`、`UV3`。
- 四种网格模式都会写入四组 UV 和 Vertex Color：Singular、Per-Path、Cross、Splash。
- 烘焙 Static Mesh 时，这些 overlay 会复制到 Static Mesh 的 UV Channel 0-3；Vertex Color 会复制到 Static Mesh 的顶点色。
- 所有浮点值都以原始值写入，不会自动归一化速度、距离或时间。材质中需要根据项目尺度自行乘除。
- `Base UV Scale` 会影响 Singular、Per-Path、Cross 的大部分距离/平面坐标：X 通常控制横向或路径标识的平铺，Y 通常控制纵向距离或流动时间。Splash 的 UV0/UV1 不使用该缩放；UV2 的 Speed 和 UV3 始终不乘缩放。

## UV 通道总表

| Channel | Per-Path / Cross | Singular | Splash |
| --- | --- | --- | --- |
| UV0 | X=带状面横向 0-1；Y=路径归一化距离 0-1 | X=路径宽度方向 0-1；Y=水幕归一化距离 0-1 | X=Splash 闭合外轮廓累计周长 0-1；Y=从外轮廓到内环的径向衰减 1-0 |
| UV1 | X=带状面局部横向坐标，单位 cm；Y=路径累计距离，单位 cm | X=源路径在 TopSpline 上的距离，单位 cm；Y=路径累计距离，单位 cm | X/Y=Splash 径向位移在局部横向/入水方向上的投影，单位 cm |
| UV2 | X=速度 Speed，单位 cm/s；Y=累计流动时间 Flow，单位 s（Y 乘 `Base UV Scale.Y`） | 同左 | 使用终点样本的原始 Speed/Flow，不乘 `Base UV Scale` |
| UV3 | X=稳定的每路径随机值 UVSeed 0-1；Y=路径在 TopSpline 上的归一化位置 0-1 | 同左 | 使用终点路径的 UVSeed/TopSplinePosition |

## UV0 详细说明

### Per-Path 和 Cross

这两类都是沿路径生成的带状面：

- `UV0.R` 从带子的一侧到另一侧变化，通常用于横向水纹、边缘遮罩或宽度渐变。
- `UV0.G` 从瀑布顶部到路径末端变化，通常用于纵向渐变、下落遮罩或采样动画。
- `UV0` 基本范围是 0-1，但 `Base UV Scale` 会改变平铺范围。
- `Ribbon Width` 和 `Cross Width` 是路径顶部宽度；`Bottom Width Scale` 按 UV0.G/归一化路径距离线性改变实际宽度。UV0 的 0-1 横向范围不会随几何宽度变化。

Per-Path 和 Cross 的几何朝向不同，但 UV0 的语义相同。

### Singular

Singular 将多条路径连接成一张连续水幕：

- `UV0.R` 是排序后的路径宽度位置，从左到右归一化。
- `UV0.G` 是共享水幕上的纵向归一化距离。

### Splash

Splash 是闭合轮廓沿径向挤出的面片：

- `UV0.R` 沿闭合轮廓累计距离，首尾方向连续。
- `UV0.G` 在外轮廓为 1，在中心/内环方向逐渐变为 0。

## UV1 详细说明

UV1 主要提供不会因为面片横向重复而丢失的世界尺度数据。

- Per-Path/Cross 的 `UV1.R` 是局部横向坐标，中心附近约为 0，两侧为正负值。
- 因为 UV1.R 使用实际横向距离，`Bottom Width Scale` 大于 1 时，其绝对值会从顶部到底部逐渐增大。
- Per-Path/Cross 的 `UV1.G` 是从路径起点累积的距离，单位为 Unreal cm。
- Singular 的 `UV1.R` 是对应源路径在 TopSpline 上的距离，便于不同宽度位置使用不同相位。
- Singular 的 `UV1.G` 仍然是路径累计距离。
- Splash 的 UV1 是终点处径向挤出的局部横向/前后方向投影，适合控制冲击圈、泡沫扩散和方向性噪波。

## UV2：速度和流动时间

Per-Path、Cross 和 Singular 的 UV2 定义如下：

- `UV2.R = Speed`：当前采样点速度，单位为 cm/s。
- `UV2.G = Flow * Base UV Scale.Y`：从路径起点累积的流动时间，原始单位为秒。

Splash 使用终点样本的 `Speed` 和 `Flow`，但不乘 `Base UV Scale`，因为 Splash 是一个终点局部面片而不是沿路径铺开的带状面。

常见用法：

- 使用 `Flow * AnimationSpeed + Time` 作为噪波或流动纹理的纵向相位。
- 使用 `Speed` 控制泡沫强度、拉伸程度或高速度区域的亮度。
- Speed 不是 0-1 值，材质中应使用 `Divide`、`Map Range` 或 `Saturate` 自行归一化。

## UV3：稳定路径标识

- `UV3.R = UVSeed`：每条路径的稳定随机值，范围为 0-1。同一 Seed 和路径索引重新生成时保持稳定。
- `UV3.G = TopSplinePosition`：路径出生点在 TopSpline 上的归一化位置，范围为 0-1。
- `UV3.B/A` 当前未使用，保持 0。

常见用法：使用 UV3.R 为每条水流生成不同的噪波相位，使用 UV3.G 让材质从瀑布左侧到右侧产生渐变。

## Vertex Color：方向和湍流

代码中的 `MakeWaterfallVertexColor` 将一个单位方向向量编码到 RGB：

```text
EncodedRGB = (Direction + 1) * 0.5
Direction  = EncodedRGB * 2 - 1
```

因此材质中应这样解码：

- `VertexColor.RGB * 2 - 1` 得到有符号方向向量。
- Per-Path/Singular/Cross 使用路径采样点的 `Velocity` 方向。
- Splash 使用 Splash 轮廓各点的径向挤出方向；因此 Splash 的方向会沿冲击面变化。
- `VertexColor.A = Turbulence`，当前范围为 0-1，由路径方向变化和碰撞影响计算得到。

建议在材质中使用 `Normalize` 重新归一化解码后的 RGB 方向，避免压缩、插值或材质运算造成长度偏差。

## 材质制作建议

1. 先用 `UV0.G` 驱动一张纵向渐变，确认路径方向和 UV 方向正确。
2. 再用 `UV2.G` 驱动 Panner 或噪波相位，确认流动速度符合预期。
3. 用 `VertexColor.A` 控制泡沫、破碎或碰撞扰动强度。
4. 用 `VertexColor.RGB * 2 - 1` 生成方向偏移或流向高光。
5. 最后根据 Per-Path、Cross、Singular、Splash 的材质槽分别调节，因为 UV0/UV1 的几何坐标含义并不完全相同。

## 代码位置

- 属性创建和三角形绑定：`Source/TYWaterfallToolsRuntime/Private/Components/TYWaterfallMeshComponent.cpp`
- Per-Path/Cross/Singular/Splash 写入：同一文件中的 `AppendSingular`、`AppendRibbon`、`AppendSplashReference`
- Static Mesh 烘焙：`Source/TYWaterfallTools/Private/Generation/TYWaterfallStaticMeshBaker.cpp`
