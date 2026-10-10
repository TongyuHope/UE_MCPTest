# UE_MCPTest 代码与插件存档

本仓库保存 UE 5.8 测试项目中持续维护的 C++ 代码、插件及开发文档。
仓库目录对应本机 `D:/UEProjectReal/UE_MCPTest`，Git 只跟踪约定范围内的文件。

## 保存范围

- `Source/UE_MCPTest.Target.cs`、`Source/UE_MCPTestEditor.Target.cs`。
- `Source/UE_MCPTest/UE_MCPTest.Build.cs`。
- `Source/UE_MCPTest/Private` 和 `Public` 中的代码，目前包含 `DetachedLib` 断肢系统。
- `Plugins/WorldPartitionRVTBake` 的源码、脚本、插件描述、图标、使用说明及测试记录。
- `Plugins/TYWaterfallTools` 的当前源码、插件描述、配置、图标和开发文档。
- 根目录 `Docs` 下所有 Markdown 文档，包括今后新增的文档。

`Content`、地图和材质等资源、编译产物、缓存、IDE 文件、Variant 示例目录，以及
模块根目录中未列出的零散 C++ 文件均不保存。其他项目插件不在本仓库范围内。

## 恢复与编译

此仓库是代码存档，不包含 `.uproject`、完整项目配置、完整游戏模块及测试资产。
单独克隆后不能直接当作完整 Unreal 工程打开。

1. 准备 UE 5.8 的 C++ 宿主项目。
2. 将所需插件目录复制到宿主项目的 `Plugins`，启用插件并重新编译。
3. `DetachedLib` 代码可放回已有 `UE_MCPTest` 模块的对应 `Private/Public` 目录。
   放入其他模块时需调整 `UE_MCPTEST_API` 导出宏及模块依赖。
4. Target 与 Build.cs 保留为原项目配置参考；在新宿主项目中需按实际模块名称、
   源码范围和引擎依赖调整。原模块的入口 `.cpp/.h` 与 Variant 示例代码不在本仓库内。
5. 材质、Niagara、蓝图、地图及其他 `Content` 资产需从本地项目或独立资源备份恢复。

瀑布插件依赖 Niagara、GeometryScripting 和 EditorScriptingUtilities；RVT 烘焙插件
依赖 PCG。插件脚本中的项目/引擎路径需按本机安装位置配置。

## 文档

- [断肢系统说明](Docs/Dismemberment/README.zh-CN.md)
- [烘焙插件中文使用手册](Docs/WorldPartitionRVTBake/README.zh-CN.md)
- [RVT 烘焙及选择性导出](Plugins/WorldPartitionRVTBake/README.md)
- [RVT 地图导出测试记录](Plugins/WorldPartitionRVTBake/TEST_RESULTS.md)
- [瀑布插件材质数据说明](Plugins/TYWaterfallTools/Docs/WaterfallMesh_MaterialData.zh-CN.md)

## 版本管理

瀑布插件以当前文件快照纳入本仓库。旧仓库的提交历史不导入，也不作为子模块依赖。
原 Git 元数据在本地迁移时另行备份。

在项目根目录使用 `git status` 查看改动，再使用 `git add Source Plugins Docs README.md`
提交约定范围内的文件。`.gitignore` 排除的本地目录可继续正常使用。
