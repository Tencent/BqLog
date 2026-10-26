# 游戏引擎集成指南

[← 返回首页](../README_CHS.md) | [English](./ENGINE_INTEGRATION.md)

---

## Unity / 团结引擎

### 集成方式

- **Unity**
  - 从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载 `unity_package_{version}`；
  - 解压后在 Unity Package Manager 中选择「从 tarball 安装」，指向其中的 `.tar` 文件导入；
  - 官方 Unity 暂不支持鸿蒙，如需鸿蒙支持可按需自行集成。

- **团结引擎**
  - 从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载 `tuanjie_package_{version}`；
  - 解压后同样通过 Unity Package Manager 以 tarball 方式导入；
  - 与 Unity 的主要差异是已集成鸿蒙相关支持。

### 使用方式

导入后，使用 `bq.log` C# API：

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello from Unity! value:{}", 42);
```

更多示例可参考仓库下的 `/demo/csharp` 目录。

---

## Unreal Engine

可任选以下一种方式集成。

### 支持的引擎版本

| 安装方式 | 支持的 Unreal Engine 版本 |
|---|---|
| **Fab** | UE 4.22 – 4.27 以及 UE 5.0 – 5.8（官方已提供的全部版本，每个次版本都有专用 zip） |
| **预编译版（GitHub Release）** | UE 4.25 – 4.27（`ue4` 包）、UE 5.0 – 5.8（`ue5` 包）与当前 UE6 开发版（`ue6` 包） |
| **源码版（GitHub Release）** | UE 4.25 – 4.27（`ue4` 包）、UE 5.0 – 5.8（`ue5` 包）与当前 UE6 开发版（`ue6` 包） |

> **使用 UE 4.22 / 4.23 / 4.24？** 请通过 [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf) 安装——Fab 上每个引擎次版本的专用 zip 针对这些老 UE4 的 `.uplugin` 语法差异做了适配（旧版 UBT 不识别 `UncookedOnly` 或 `LinuxAArch64` 等标识符）。GitHub Release 上的通用 `ue4` 包只针对 UE 4.25+。
>
> **UE 4.22 与 4.23 的 Mac / iOS 支持：** 由于 Fab 构建机的限制，Fab 上针对 UE 4.22 与 4.23 的 zip **未包含** Mac 与 iOS 版本。如果在这两个引擎版本下需要 Mac 或 iOS 支持，请前往 [GitHub Releases 页面](https://github.com/Tencent/BqLog/releases) 下载 **源码版** 或 **预编译版** 手动引入。
>
> **使用 UE6？** 请从 [GitHub Releases 页面](https://github.com/Tencent/BqLog/releases)手动下载 `ue6` 源码版或预编译版插件。这些包目前与 UE5 使用相同的插件实现，并已在当前 UE6 开发版中验证可用。UE6 仍在开发，后续可能需要随引擎变化调整兼容性。由于 Epic 官方的 Fab 引擎支持目前只到 UE 5.8，Fab 暂时无法分发 UE6 插件。
>
> **PS4 / PS5 / Switch？** 请使用**源码版**：预编译版不含主机平台的库。主机平台宏由插件传入，两个 base dir 也由插件设置。平台相关准备见[接入指南 — 主机平台](./INTEGRATION_GUIDE_CHS.md#主机平台)。

### 集成方式

- **Fab（推荐）**
  - 通过 Epic Fab 商城直接安装：[BqLog on Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf)；
  - 在 Epic Games Launcher / Fab 客户端中将插件添加到引擎，然后在工程中启用即可，无需手动下载解压；
  - 适合希望以最简方式安装并通过 Fab 自动获取更新的用户。

- **预编译版（Prebuilt）**
  - 从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载 `unreal_plugin_prebuilt_ue4_{version}`（UE 4.25 – 4.27）、`unreal_plugin_prebuilt_ue5_{version}`（UE 5.0 – 5.8）或 `unreal_plugin_prebuilt_ue6_{version}`（当前 UE6 开发版）；
  - 解压到游戏项目的 `Plugins` 目录下。

- **源码版（Source）**
  - 从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载 `unreal_plugin_sources_ue4_{version}`（UE 4.25 – 4.27）、`unreal_plugin_sources_ue5_{version}`（UE 5.0 – 5.8）或 `unreal_plugin_sources_ue6_{version}`（当前 UE6 开发版）；
  - 解压到游戏项目的 `Plugins` 目录下，由引擎进行二次编译。

解压后请确认插件描述文件位于以下路径。多嵌套一层 `BqLog` 目录会导致 Unreal 无法正确发现插件。

```text
<你的项目>/Plugins/BqLog/BqLog.uplugin
```

### 启用插件

在你的 `.uproject` 文件的 `Plugins` 节点下添加 BqLog：

```json
"Plugins": [
    {
        "Name": "BqLog",
        "Enabled": true
    }
]
```

> 如果 `.uproject` 已有 `Plugins` 数组，请把上述条目合并进去，不要再添加第二个 `Plugins` 键。项目 `Plugins` 目录中的插件通常会被自动发现，但仍建议显式声明 BqLog，便于版本管理和团队协作。不需要在 `.uproject` 的 `AdditionalDependencies` 中添加 BqLog。

### 添加 BqLog 模块依赖

每个需要包含 BqLog 头文件的游戏模块，都必须在自己的 `.Build.cs` 中添加 `"BqLog"`。如果只在 `.cpp` 文件中使用 BqLog，推荐添加为私有依赖：

```csharp
PrivateDependencyModuleNames.AddRange(new string[] {
    "BqLog"
});
```

如果模块的公共头文件暴露了 BqLog 类型或包含了 BqLog 头文件，则应改为公共依赖：

```csharp
PublicDependencyModuleNames.Add("BqLog");
```

之后即可在 C++ 中包含 Unreal 适配器：

```cpp
#include "BqLog.h"
```

> **不要**手动添加 include 目录，`BqLog` 模块会导出所需路径。蓝图节点模块 `BqLogBPNodes` 是编辑器专用模块，也**不需要**加入依赖。修改 `.uproject` 或 `.Build.cs` 后，如果 IDE 没有自动刷新，请重新生成项目文件，然后重新编译 Editor Target。

### 1）对 `FName` / `FString` / `FText` 的支持

在 Unreal 环境中，BqLog 内置了适配器：

- 自动支持 `FString`、`FName`、`FText` 作为 format 字符串和参数；
- 兼容 UE4、UE5 与当前 UE6 开发版。

示例：

```cpp
bq::log log_my = bq::log::create_log("AAA", config);   // config 省略

FString fstring_1 = TEXT("这是一个测试的FString{}");
FString fstring_2 = TEXT("这也是一个测试的FString");
log_my.error(fstring_1, fstring_2);

FText text1 = FText::FromString(TEXT("这是一个FText!"));
FName name1 = FName(TEXT("这是一个FName"));
log_my.error(fstring_1, text1);
log_my.error(fstring_1, name1);
```

如果希望自定义适配行为，也可以使用「方式2（全局函数）」自行定义 `bq_log_format_str_size` 和 `bq_log_format_str_chars`（详见 [高级用法](./ADVANCED_USAGE_CHS.md)）。

### 2）将 BqLog 的输出转接到 Unreal 日志窗口

Unreal 插件会自动将所有 BqLog 输出转接到 Unreal 的 Output Log，无需额外配置。

> **注意：** 如果未使用插件而是直接在 C++ 级别集成 BqLog，可以通过 `bq::log::register_console_callback` 手动将日志转发到 `UE_LOG`。详见 [高级用法](./ADVANCED_USAGE_CHS.md)。

### 3）在蓝图中使用 BqLog

已引入插件后，可在蓝图中直接调用 BqLog：

1. **创建日志 Data Asset**
   - 在 Unreal 工程中创建 Data Asset，类型选择 BqLog：
     - 默认 Log 类型（不带 Category）：
       <img src="img/ue_pick_data_asset_1.png" alt="默认Log创建" style="width: 455px">
     - 若生成了带 Category 的日志类（详见 [高级用法 — Category](./ADVANCED_USAGE_CHS.md)），并将 `{category}.h` 与 `{category}_for_UE.h` 加入工程：
       <img src="img/ue_pick_data_asset_2.png" alt="Category Log创建" style="width: 455px">

2. **配置日志参数**
   - 双击打开 Data Asset，配置日志对象名与创建方式：
     - `Create New Log`：在运行时新建一个 Log 对象：
       <img src="img/ue_create_log_config_1.png" alt="配置Log参数 Create New Log" style="width: 455px">
     - `Get Log By Name`：仅获取其他地方已经创建的同名 Log：
       <img src="img/ue_create_log_config_2.png" alt="配置Log参数 Get Log By Name" style="width: 455px">

3. **在蓝图中调用日志节点**

   <img src="img/ue_print_log.png" alt="蓝图调用Log" style="width: 655px">

   - 区域 1：添加日志参数；
   - 区域 2：新增的日志参数节点，可通过右键菜单（Remove ArgX）删除；
   - 区域 3：选择日志对象（即刚才创建的 Data Asset）；
   - 区域 4：仅当日志对象带 Category 时显示，可选择日志 Category。

4. **测试**
   - 运行蓝图，如配置正确且有 ConsoleAppender 输出，可在 Log 窗口看到类似输出：

     ```text
     LogBqLog: Display: [Bussiness_Log_Obj] UTC+7 2025-11-27 14:49:19.381[tid-27732 ] [I] [Factory.People.Manager] Test Log Arg0:String Arg, Arg1:TRUE, Arg2:1.000000,0.000000,0.000000|2.999996,0.00[...]
     ```

---

## 接下来

- [集成指南](./INTEGRATION_GUIDE_CHS.md) — 所有平台和语言的集成步骤
- [API 参考](./API_REFERENCE_CHS.md) — 创建日志、写日志等核心 API
- [高级用法](./ADVANCED_USAGE_CHS.md) — Category、自定义类型、加密等
