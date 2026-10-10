# 选择性导出测试记录

测试日期：2026-10-10。引擎：本机 UE 5.8，Win64 Development。

- 项目：`D:/UEProjectReal/UE_MCPTest/UE_MCPTest.uproject`
- 源地图：`/Game/Level/BakeTest`
- 目标地图：`/Game/Level/BakeTest_RVTBake`
- PCG 标签：`BakeTest`
- RVT：`RuntimeVirtualTextureVolume_UAID_047C160156AB250903_2076114856`

插件编译通过。描述预览从 4,449 个 Actor 中选出 69 个：65 个地形 Actor
（Landscape 和 StreamingProxy）、1 个 PCG、1 个 RVT、WorldDataLayers 和 PCGWorldActor。

实际导出返回 0，目标地图和外部 Actor 共 70 个文件、7,830,461 字节（约 7.5 MiB）。
重新加载目标地图后仍恰好有 69 个 Actor。验证脚本对比了 Actor 变换、地形组件
网格位置与包围盒、地形材质与编辑层设置、PCG 图与生成配置、RVT/SVT 引用与
低 Mip 设置；全部一致。源地图和外部包共 4,450 个文件的 SHA-256 均未变化。

再次向同名地图导出被拒绝，目标 70 个文件哈希未变化。
在导出地图运行原 RVT 构建器 `-ValidateOnly` 返回 0，识别到 8192×8192、
32×32 个 tile、512 MiB 像素缓冲，原 SVT Builder 引用保留。

本轮没有执行实际 PCG 生成和 SVT 重烘焙，也没有比较地形纹理的逐像素内容。
材质、网格、PCG 图、RVT/SVT 资源仍共享；测试重烘焙前应按需要配置单独的
SVT Builder。该测试结果不代表已解决 ZDG 中 PCG 动态输入缺失的问题。

测试项目内记录：

- `Saved/RVTBakeExport/BakeTest_RVTBake.json`
- `Saved/RVTBakeExport/BakeTest_RVTBake-validation.json`
- `Saved/RVTBakeExport-test.log`
- `Saved/RVTBakeExport-validation.log`
- `Saved/RVTBakeExport-overwrite-check.log`
- `Saved/RVTBakeExport-rvt-validation.log`
