# BqLog (扁鹊日志) (V 2.0.1) ([ChangeLog](CHANGELOG.md))
[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.0.1-red.svg)](https://github.com/Tencent/BqLog/releases)

> BqLog 是一个轻量级、高性能的工业级日志系统，已在线上广泛应用于《王者荣耀》等项目。  
> **BqLog 2.0.1 版本正式发布！带来了纯血鸿蒙支持、Node.js 支持、更强的并发性能以及高强度的非对称混合加密能力。**

---

## 如果您有以下困扰，可以尝试 BqLog

- 如果您的客户端产品（尤其是游戏）希望同时满足以下「不可能三角」：
  - 方便追溯问题（日志应写尽写）
  - 性能足够好（日志要少写）
  - 节约存储空间（日志最好就别写）
- 如果您是后台服务开发者，现有日志库在**高并发场景**下性能不足，导致日志丢失或程序阻塞。
- 如果您的编程语言是 C++、Java、C#、Kotlin、TypeScript、JavaScript 之一，或者同时使用多种语言，希望有一套**统一的跨语言日志解决方案**。

---

## 支持操作系统和平台

- Windows 64-bit
- macOS
- Linux（包含嵌入式 Linux）
- iOS（包括 iPhone、iPad、Apple Vision、Apple Watch 等所有苹果设备及模拟器）
- Android
- HarmonyOS（包括手机、平板、智慧屏等所有鸿蒙设备及模拟器）
- Unix（已在 FreeBSD、NetBSD、OpenBSD、Solaris、DragonFly、OmniOS 等上通过测试）

---

## 支持编程语言及开发环境

- C++（C++11 及以上，支持 MSVC、Clang、GCC、MinGW-GCC、MinGW-Clang）
- Java / Kotlin（Android & Server）
- C#（Unity、团结引擎、.NET）
- **ArkTS / C++（HarmonyOS）**
- **JavaScript / TypeScript（Node.js，支持 CJS 与 ESM）**
- **Unreal Engine（UE4 & UE5）**

---

## 支持的硬件架构

- x86
- x86_64
- ARM32
- ARM64

---

## 支持的引入方式

- 动态库
- 静态库
- 源代码

---

## 特点

- 相比常见开源日志库有显著性能优势（详见 [Benchmark](#benchmark)），不仅适用于服务器和客户端，也非常适合移动端设备。
- 内存消耗少：在 [Benchmark](#benchmark) 用例中，10 线程、2,000 万条日志，BqLog 自身内存消耗约为 1 MB。
- 提供高性能、高压缩比的实时压缩日志格式。
- 以小于 10% 的性能损耗为代价，提供高强度的非对称混合加密日志，保护日志内容安全（可选）。
- 可在游戏引擎（`Unity`、`Unreal` 等）中正常使用，对 [Unreal 提供蓝图和常用类型的支持](#5-在-unreal-中使用-bqlog)。
- 支持 `utf8`、`utf16`、`utf32` 字符及字符串，支持 bool、float、double、各种长度与类型的整数等常用参数类型。
- 支持 `C++20` 的 `std::format` 规范（不含排序序号与时间格式化）。
- 异步日志支持 Crash 复盘机制，尽量避免日志数据丢失。
- 尺寸小巧：Android 动态库进包体积约 300 KB 左右。
- 在 Java、C#、TypeScript 上可以做到「零额外 Heap Alloc」（或极少），不会随着运行不断 new 对象。
- 仅依赖标准 C 语言库与平台 API，可在 Android 的 `ANDROID_STL = none` 模式下编译通过。
- 支持 `C++11` 及之后的标准，可在极其严格的编译选项下工作。
- 编译系统基于 `CMake`，并提供多平台编译脚本，集成简单。
- 支持自定义参数类型。
- 对代码提示非常友好。

---

## 技术文章

- **[为何 BqLog 如此快 - 高性能实时压缩日志格式](docs/文章1_为何BqLog如此快%20-%20高性能实时压缩日志格式.MD)**
- **[为何 BqLog 如此快 - 高并发环形队列](docs/文章2_为何BqLog如此快%20-%20高并发环形队列.MD)**

---

## 目录

**[从 1.x 版本升级到 2.x 版本的变化](#从-1x-版本升级到-2x-版本的变化)**  
**[将 BqLog 引入您的项目](#将-bqlog-引入您的项目)**  
**[简单的 Demo](#简单的-demo)**  
**[架构介绍](#架构介绍)**  
**[主流程 API 使用说明](#主流程-api-使用说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 创建 Log 对象](#1-创建-log-对象)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. 获取 Log 对象](#2-获取-log-对象)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. 写日志](#3-写日志)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. 其他 API](#4-其他-api)  
**[同步日志和异步日志](#同步日志和异步日志)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 异步日志的线程安全性说明](#异步日志的线程安全性说明)  
**[Appender 介绍](#appender-介绍)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. ConsoleAppender](#consoleappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. TextFileAppender](#textfileappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. （重点推荐）CompressedFileAppender](#compressedfileappender)  
**[配置说明](#配置说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 完整示例](#1-完整示例)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. 详细解释](#2-详细解释)  
**[离线解码二进制格式的 Appender](#离线解码二进制格式的-appender)**  
**[构建说明](#构建说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 库构建](#1-库构建)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Demo 构建和运行](#2-demo-构建和运行)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. 自动测试运行说明](#3-自动测试运行说明)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Benchmark 运行说明](#4-benchmark-运行说明)  
**[高级使用话题](#高级使用话题)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 无 Heap Alloc](#1-无-heap-alloc)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. 支持分类（Category）的 Log 对象](#2-支持分类category的-log-对象)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. 程序异常退出的数据保护](#3-程序异常退出的数据保护)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. 自定义参数类型](#4-自定义参数类型)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. 在 Unreal 中使用 BqLog](#5-在-unreal-中使用-bqlog)  
[&nbsp;&nbsp;&nbsp;&nbsp;6. 日志加密和解密](#6-日志加密和解密)  
**[Benchmark](#benchmark)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Benchmark 说明](#1-benchmark-说明)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. BqLog C++ Benchmark 代码](#2-bqlog-c-benchmark-代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. BqLog Java Benchmark 代码](#3-bqlog-java-benchmark-代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Log4j Benchmark 代码](#4-log4j-benchmark-代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. Benchmark 结果](#5-benchmark-结果)  
**[如何贡献代码](#如何贡献代码)**

---

## 从 1.x 版本升级到 2.x 版本的变化

1. 增加对鸿蒙系统的支持，包括 ArkTS 和 C++ 两种语言。
2. 增加对 Node.js 的支持（CJS 和 ESM）。
3. 增强跨平台兼容性、稳定性与通用性，支持更多 Unix 系统。
4. 性能平均提升约 40%。
5. Android 不再强制要求与 Java 一起使用。
6. 移除 `is_in_sandbox` 配置，改用 `base_dir_type`；对 snapshot 增加过滤配置，支持每次启动新开日志文件。详见 [配置说明](#配置说明)。
7. 支持高性能非对称混合加密，几乎无额外性能损耗，详见 [6. 日志加密和解密](#6-日志加密和解密)。
8. 提供 Unity、团结引擎、Unreal 引擎插件，方便在游戏引擎中使用；提供 ConsoleAppender 对游戏引擎编辑器日志输出重定向，提供 Unreal 蓝图支持。详见 [5. 在 Unreal 中使用 BqLog](#5-在-unreal-中使用-bqlog)。
9. 仓库不再包含二进制产物，从 2.x 版本起请从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载对应平台和语言的二进制包。
10. 单条日志长度不再受log.buffer_size限制
11. 可以精确手动设置时区
12. `raw_file`类型的appender不再维护，标记为`废弃`，请用`compressed_file`类型替代。
13. 复盘能力增加可靠性，从实验性功能变成正式能力。见[程序异常退出的数据保护](#3-程序异常退出的数据保护))。

---

## 将 BqLog 引入您的项目

> 以下示例假定您已在 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载对应版本的二进制包或源码。

### C++（动态库 / 静态库 / 源码）

- **动态库**  
  下载 `dynamic_lib_{version}` 压缩包：
  - 将 `dynamic_lib/include` 目录添加到头文件搜索路径；
  - 链接 `dynamic_lib/lib` 中对应平台的动态库文件。

- **静态库**  
  下载 `static_lib_{version}` 压缩包：
  - 将 `static_lib/include` 目录添加到头文件搜索路径；
  - 链接 `static_lib/lib` 中对应平台的静态库文件。

- **源码集成**
  - 将仓库下的 `/src` 目录加入工程源码编译；
  - 将 `/include` 目录添加到头文件搜索路径。
  - Windows + Visual Studio：请添加编译选项 `/Zc:__cplusplus`。
  - Android 模式下支持 `ANDROID_STL = none`。
  - 如需启用 Java / NAPI（Node.js / HarmonyOS ArkTS）支持，以及系统链接库与部分宏定义，请参考 `/src/CMakeLists.txt`。
  - 当存在 NAPI 环境（Node.js 或 HarmonyOS ArkTS）或需要 Java / C# 调用时，不推荐以「纯 C++ 源码」直接集成，因为需要手动处理初始化和库加载流程；更推荐使用预编译包及对应 wrapper。

### C#（Unity / 团结引擎 / .NET）

- **Unity**
  - 下载 `unity_package_{version}`；
  - 解压后在 Unity Package Manager 中选择「从 tarball 安装」，指向其中的 `.tar` 文件导入；
  - 官方 Unity 暂不支持鸿蒙，如需鸿蒙支持可按需自行集成。

- **团结引擎**
  - 下载 `tuanjie_package_{version}`；
  - 解压后同样通过 Unity Package Manager 以 tarball 方式导入；
  - 与 Unity 的主要差异是已集成鸿蒙相关支持。

- **.NET**
  - 下载对应平台的动态库包 `{os}_{arch}_libs_{version}`，引入其中动态库；
  - 将仓库下 `/wrapper/csharp/src` 源码加入工程。

### Java / Kotlin（Android / Server）

- **Android**
  - 下载 `android_libs_{version}`；
  - 可以直接引入其中的 `.aar` 包，或手动引入 `.so` + 仓库下 `/wrapper/java/src` 源码（后者方便自定义及调试）。

- **Server**
  - 下载对应平台动态库 `{os}_{arch}_libs_{version}` 并引入；
  - 再下载 `java_wrapper_{version}`，引入 jar 包或直接加入仓库下 `/wrapper/java/src` 源码。

### HarmonyOS（ArkTS / C++）

- 下载 `harmony_os_libs_{version}`；
- 引入 `har` 包，或直接引入其中的 `.so` + 仓库下 `/wrapper/typescript/src` 源码（可选）；
- 支持在 ArkTS 侧直接调用，也支持在 Native C++ 侧调用。

### Node.js

- 支持 CommonJS 与 ES Module。
- 从 Releases 下载 `nodejs_npm_{version}` 包，解压后找到其中的 `bqlog-{version}.tgz`，通过 npm 安装：

```bash
npm install ./bqlog-{version}.tgz
```

可参考仓库下 `/demo/nodejs` 目录。

### Unreal Engine

- **预编译版（Prebuilt）**
  - 从 Releases 下载 `unreal_plugin_prebuilt_{version}`；
  - 解压后根据自己的引擎版本，选择对应压缩包，解压到游戏项目的 `Plugins` 目录下。

- **源码版（Source）**
  - 从 Releases 下载 `unreal_plugin_source_{version}`；
  - 解压后根据自己的引擎版本，选择对应压缩包，解压到游戏项目的 `Plugins` 目录下，由引擎进行二次编译。

---

## 简单的 Demo

### C++

```cpp
#include <string>
#include <bq_log/bq_log.h>

int main() {
    // 配置：输出到控制台
    std::string config = R"(
        appenders_config.appender_console.type=console
        appenders_config.appender_console.levels=[all]
    )";
    auto log = bq::log::create_log("main_log", config);

    log.info("Hello BqLog 2.0! int:{}, float:{}", 123, 3.14f);
    log.force_flush(); // 强制刷新（通常用于程序退出前）

    return 0;
}
```

更多示例可参考仓库下的 `/demo/cpp` 目录。

### Typescript (Node.js, 鸿蒙ArkTS)

```javascript
import { bq } from "bqlog"; // ESM 写法
// const { bq } = require("bqlog"); // CommonJS 写法

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);

log.info("Hello from Node.js! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

更多示例可参考仓库下的 `/demo/nodejs` 目录。

### C#

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```

更多示例可参考仓库下的 `/demo/csharp` 目录。

### Java（Android / Server）

```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```

更多示例可参考仓库下的 `/demo/java` 目录。

---

## 架构介绍

![基础结构](docs/img/log_structure.png)

上图展示了 BqLog 的整体架构。图右侧为 `BqLog Core Engine`（BqLog 核心引擎），图左侧为您的业务程序与各语言 Wrapper。  
您的程序通过 BqLog 提供的 `BqLog Wrapper`（如 C++、Java、C#、TypeScript 等）来访问核心引擎。

- 由用户程序创建多个 Log 对象，如 `Log A`、`Log B`、`Log C`；
- 每个 Log 对象可挂载一个或多个 Appender，可理解为日志内容的「输出目标」（控制台 / 文本文件 / 压缩文件等）；
- Log 对象负责「接收日志 + 写入缓冲（环形队列）」；Appender 负责「实际输出到终端或文件」。

**同一进程内，不同语言的 Wrapper 可以访问同一个 Log 对象。**  
例如：Android 应用在 Java 侧创建名为 `Log A` 的 Log 对象，NDK C++ 侧可以通过 `"Log A"` 这个名字拿到同一个 Log 对象并写日志。

在极端情况下，例如一个 Unity 游戏运行在 Android 上，同时涉及 Java/Kotlin、C#、C++，它们都可以共享同一个 Log 对象，统一日志输出。

---

## 主流程 API 使用说明

> 说明：以下 API 均声明在 `bq::log`（C++）或 `bq.log`（其他语言 Wrapper）类中。  
> 为节约篇幅，这里仅列出 C++ API，其它语言 Wrapper 拥有一致的接口命名与语义。

C++ 中出现的 `bq::string` 是 BqLog 内部定义的 UTF-8 字符串类型。大多数情况下，您可以直接传入 `char*`、`std::string`、`std::string_view` 等，BqLog 会做隐式转换。

### 1. 创建 Log 对象

通过 `create_log` 静态函数创建 Log：

```cpp
/// <summary>
/// Create a log object
/// </summary>
/// <param name="log_name">
/// If the log name is an empty string, bqLog will automatically assign you a unique log name.
/// If the log name already exists, it will return the previously existing log object and
/// update its configuration (with some fields not modifiable).
/// </param>
/// <param name="config_content">Log config string</param>
/// <returns>A log object, if create failed, the is_valid() method of it will return false</returns>
static log create_log(const bq::string& log_name, const bq::string& config_content);
```

要点：

1. 无论在 C# 或 Java 中，`create_log` 的返回值都**不会为 null**。如配置有误等原因导致创建失败，可通过 `is_valid()` 判断。
2. 若 `log_name` 为空字符串，则 BqLog 会自动分配一个不重复的名称，如 `"AutoBqLog_1"`。
3. 若对已存在的同名 Log 调用 `create_log`，不会创建新对象，而是复用原对象并覆盖其配置（部分字段不可修改，例如 `buffer_size` 等，详见配置章节）。
4. 可以在全局变量或静态变量中直接通过该 API 初始化 Log 对象，无需担心静态初始化顺序（Static Initialization Order Fiasco）、析构顺序或多线程问题。

### 2. 获取 Log 对象

如果某个 Log 对象已经在其他地方创建过，可以通过名称直接获取：

```cpp
// C++ API
/// <summary>
/// Get a log object by its name
/// </summary>
/// <param name="log_name">Name of the log object you want to find</param>
/// <returns>
/// A log object. If the log object with specific name was not found,
/// the is_valid() method of it will return false.
/// </returns>
static log get_log_by_name(const bq::string& log_name);
```

同样可以在全局变量或静态变量中使用该函数初始化 Log 对象。  
需要注意：请务必保证该名称的 Log 对象已经通过 `create_log` 创建过，否则返回对象将为 `!is_valid()` 状态。

### 3. 写日志

```cpp
/// Core log functions, there are 6 log levels:
/// verbose, debug, info, warning, error, fatal
template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> verbose(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> verbose(const STR& log_format_content, const Args&... args) const;

template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> debug(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> debug(const STR& log_format_content, const Args&... args) const;

template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> info(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> info(const STR& log_format_content, const Args&... args) const;

template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> warning(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> warning(const STR& log_format_content, const Args&... args) const;

template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> error(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> error(const STR& log_format_content, const Args&... args) const;

template<typename STR>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> fatal(const STR& log_content) const;
template<typename STR, typename...Args>
bq::enable_if_t<is_bq_log_str<STR>::value, bool> fatal(const STR& log_format_content, const Args&... args) const;
```

写日志需重点关注三个方面：

#### 1. 日志等级

日志分为 6 个等级：`verbose`、`debug`、`info`、`warning`、`error`、`fatal`，与 Android 的日志等级一致，重要性依次递增。  
在控制台输出时，不同等级会用不同颜色进行区分（ConsoleAppender 下）。

![日志等级](docs/img/log_level.png)

#### 2. format 字符串（`STR` 参数）

`STR` 类似 `printf` 的第一个参数，用于描述日志格式，支持不同语言下的常见字符串类型，例如：

- Java 中的 `java.lang.String`
- C# 中的 `string`
- Unreal 中的 `FName`、`FString`、`FText` 等
- C++ 中的常见字符串形式：
  - C 风格字符串：`char*`、`char16_t*`、`char32_t*`、`wchar_t*`
  - 标准库字符串：`std::string`、`std::u8string`、`std::u16string`、`std::u32string`、`std::wstring` 等

BqLog 内部会统一将它们编码为适合存储和输出的 UTF 编码格式。

#### 3. format 参数

`STR` 后面可以跟任意数量的参数，这些参数会按 `C++20 std::format` 的规则格式化到 `{}` 位置（不支持排序序号与时间格式化）。

**强烈建议使用 format 参数方式输出日志，而不要手动拼接字符串。**  
这样可以显著提升性能，并让压缩格式得到最优效果。

当前支持的参数类型包括：

- 空指针（输出 `null`）
- 指针（输出 `0x` 开头的 16 进制地址）
- `bool`
- 单字节字符（`char`）
- 双字节字符（`char16_t`、`wchar_t`、C# 的 `char`、Java 的 `char`）
- 四字节字符（`char32_t` 或部分平台上的 `wchar_t`）
- 8/16/32/64 位整数与无符号整数
- 32 位与 64 位浮点数
- C++ 其他 POD 类型（尺寸为 1、2、4、8 字节，会按 `int8`/`int16`/`int32`/`int64` 处理）
- 上述 [STR 参数](#2-format-字符串str-参数) 中提到的所有字符串类型
- C# 和 Java 中任意对象（通过 `ToString()` 结果输出）
- 自定义参数类型（详见 [自定义参数类型](#4-自定义参数类型)）

### 4. 其他 API

更多细节请参考头文件 `bq_log/bq_log.h`，以及 Java / C# / TypeScript 等 Wrapper 中的 `bq.log` 类注释。  
这里重点介绍若干常用能力。

#### 异常退出保护

```cpp
/// <summary>
/// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk.
/// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer
/// in the event of a crash. However, this functionality does not guarantee success,
/// and only supports POSIX systems.
/// </summary>
static void enable_auto_crash_handle();
```

详细介绍见 [程序异常退出的数据保护](#3-程序异常退出的数据保护)。

#### 强制刷新缓冲

```cpp
/// <summary>
/// Synchronously flush the buffer of all log objects
/// to ensure that all data in the buffer is processed after the call.
/// </summary>
static void force_flush_all_logs();

/// <summary>
/// Synchronously flush the buffer of this log object
/// to ensure that all data in the buffer is processed after the call.
/// </summary>
void force_flush();
```

BqLog 默认使用异步日志。有时需要确保某些关键日志「立刻落盘」，可在关键路径或程序退出前调用 `force_flush` / `force_flush_all_logs()`。

#### 拦截 Console 输出

```cpp
/// <summary>
/// Register a callback that will be invoked whenever a console log message is output.
/// This can be used for an external system to monitor console log output.
/// </summary>
/// <param name="callback"></param>
static void register_console_callback(bq::type_func_ptr_console_callback callback);

/// <summary>
/// Unregister a console callback.
/// </summary>
/// <param name="callback"></param>
static void unregister_console_callback(bq::type_func_ptr_console_callback callback);
```

[ConsoleAppender](#consoleappender) 默认输出到终端（Android 为 ADB Logcat），但这不能覆盖所有情况（如自研游戏引擎、自研 IDE 等）。  
通过注册回调，可以把 console 输出转发到自研系统中。

**注意：**

1. 不要在 console callback 中再调用任何「同步刷新的 BqLog 日志函数」，否则极易造成死锁。
2. 使用 Unity / 团结引擎 / Unreal 插件时，无需手动调用此接口，插件已将 ConsoleAppender 输出自动重定向到编辑器日志窗口。

#### 主动获取 Console 输出

```cpp
/// <summary>
/// Enable or disable the console appender buffer.
/// Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke
/// callbacks from a native thread, we can enable this option.
/// This way, all console outputs will be saved in the buffer until we fetch them.
/// </summary>
static void set_console_buffer_enable(bool enable);

/// <summary>
/// Fetch and remove a log entry from the console appender buffer in a thread-safe manner.
/// If the console appender buffer is not empty, the on_console_callback function will be invoked
/// for this log entry. Please ensure not to output synchronized BQ logs within the callback function.
/// </summary>
/// <param name="on_console_callback">
/// A callback function to be invoked for the fetched log entry if the console appender buffer is not empty
/// </param>
/// <returns>
/// True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.
/// </returns>
static bool fetch_and_remove_console_buffer(bq::type_func_ptr_console_callback on_console_callback);
```

当不适合用回调方式直接从原生线程回调到虚拟机（如 C#、Java、IL2CPP 环境）时，可以改用「主动拉取」：

- `set_console_buffer_enable(true)`：启用 console 缓存；
- `fetch_and_remove_console_buffer(...)`：在业务线程中主动读取并消费缓存中的 console 日志。

**注意（IL2CPP 环境）：**

- 请确保 `on_console_callback` 为 `static unsafe` 方法；
- 并添加 `[MonoPInvokeCallback(typeof(type_console_callback))]` 属性，确保回调不会被 GC 回收。

#### 修改 Log 配置

```cpp
/// <summary>
/// Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
/// </summary>
/// <param name="config_content"></param>
/// <returns></returns>
bool reset_config(const bq::string& config_content);
```

如需在运行时调整部分配置，可调用 `reset_config`。  
部分字段（如 `buffer_size`、`thread_mode` 等）出于安全与实现复杂度考虑，不允许在运行时修改，详见配置章节对应表格。

#### 临时禁用或启用某些 Appender

```cpp
/// <summary>
/// Temporarily disable or enable a specific Appender.
/// </summary>
/// <param name="appender_name"></param>
/// <param name="enable"></param>
void set_appender_enable(const bq::string& appender_name, bool enable);
```

默认情况下，配置中声明的 Appender 全部启用。  
通过该 API，可以在运行中按需临时关闭 / 打开某些 Appender（例如临时关闭某个文件输出）。

#### 输出快照（Snapshot）

```cpp
/// <summary>
/// Works only when snapshot is configured.
/// It will decode the snapshot buffer to text.
/// </summary>
/// <param name="time_zone_config">
/// Use this to specify the time display of log text.
/// such as: "localtime", "gmt", "Z", "UTC", "UTC+8", "UTC-11", "utc+11:30"
/// </param>
/// <returns>the decoded snapshot buffer</returns>
bq::string take_snapshot(const bq::string& time_zone_config) const;
```

某些场景（如异常检测、关键事件上报）需要获取「最近一段时间」的日志快照，可通过此功能实现：

1. 在配置中开启 `snapshot`（见 [snapshot](#snapshot) 配置节），并设置缓冲大小、等级与 category 过滤；
2. 需要快照时调用 `take_snapshot()`，即可获得格式化后的最近日志字符串。  
   C++ 中返回类型为 `bq::string`，可隐式转换为 `std::string` 或 C 风格字符串。

#### 解码二进制日志文件

```cpp
namespace bq {
namespace tools {

    // This is a utility class for decoding binary log formats.
    // To use it, first create a log_decoder object,
    // then call its decode function to decode.
    // After each successful call,
    // you can use get_last_decoded_log_entry() to retrieve the decoded result.
    // Each call decodes one log entry.
    struct log_decoder {
    private:
        bq::string decode_text_;
        bq::appender_decode_result result_ = bq::appender_decode_result::success;
        uint32_t handle_ = 0;

    public:
        /// <summary>
        /// Create a log_decoder object, with each log_decoder object corresponding to a binary log file.
        /// </summary>
        /// <param name="log_file_path">
        /// the path of a binary log file, it can be a relative path or absolute path
        /// </param>
        /// <param name="priv_key">
        /// private key generated by "ssh-keygen" to decrypt encrypted log file,
        /// leave it empty when log file is not encrypted.
        /// </param>
        log_decoder(const bq::string& log_file_path, const bq::string& priv_key = "");

        ~log_decoder();

        /// <summary>
        /// Decode a log entry. Each call of this function will decode only 1 log entry.
        /// </summary>
        /// <returns>
        /// decode result, appender_decode_result::eof means the whole log file was decoded
        /// </returns>
        bq::appender_decode_result decode();

        /// <summary>
        /// get the last decode result
        /// </summary>
        bq::appender_decode_result get_last_decode_result() const;

        /// <summary>
        /// get the last decoded log entry content
        /// </summary>
        const bq::string& get_last_decoded_log_entry() const;

        /// <summary>
        /// Directly decode a log file to a text file.
        /// </summary>
        /// <param name="log_file_path"></param>
        /// <param name="output_file"></param>
        /// <param name="priv_key">
        /// private key generated by "ssh-keygen" to decrypt encrypted log file,
        /// leave it empty when log file is not encrypted.
        /// </param>
        /// <returns>success or not</returns>
        static bool decode_file(const bq::string& log_file_path,
                                const bq::string& output_file,
                                const bq::string& priv_key = "");
    };

} // namespace tools
} // namespace bq
```

该工具类用于在运行时解码二进制 Appender（如 [CompressedFileAppender](#compressedfileappender)）输出的日志文件。

- 创建 `log_decoder` 对象；
- 反复调用 `decode()`，每次解码一条日志：
  - 若返回 `bq::appender_decode_result::success`，则可以调用 `get_last_decoded_log_entry()` 获取文本；
  - 若返回 `bq::appender_decode_result::eof`，表示已解码到文件末尾；
- 如日志启用了加密，构造 `log_decoder` 或调用 `decode_file` 时需传入私钥字符串（详见后文「日志加密和解密」）。

---

## 同步日志和异步日志

BqLog 通过配置项 `log.thread_mode` 决定日志对象采用同步还是异步模式。两者的区别如下：

|                    | **同步日志（Synchronous Logging）**                                                                 | **异步日志（Asynchronous Logging）**                                                                 |
|:------------------:|------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| **行为（Behavior）**    | 调用日志函数后，日志会立刻同步输出到对应 Appender，函数返回时保证这条日志已被处理。                                  | 调用日志函数后，日志被写入缓冲区，由 worker 线程异步处理，写日志的线程很快返回。                                |
| **性能（Performance）** | 性能较低，调用线程需要阻塞等待 Appender 输出完成。                                                              | 性能较高，调用线程无需等待实际输出。                                                                 |
| **线程安全性（Thread Safety）** | 在「调用期间参数不被修改」前提下线程安全；多线程下性能受限于串行输出。                                                | 在「调用期间参数不被修改」前提下线程安全；内部通过高并发环形队列与工作线程进行调度以提升扩展性。                       |

### 异步日志的线程安全性说明

异步日志常见的顾虑是：**worker 线程真正处理日志时，参数已经失效（生命周期结束）**。例如：

```cpp
{
    const char str_array[5] = {'T', 'E', 'S', 'T', '\0'};
    const char* str_ptr = str_array;
    log_obj.info("This is test param :{}, {}", str_array, str_ptr);
}
```

`str_array` 为栈变量，离开作用域后内存已经无效。  
担心点在于：如果 worker 线程稍后才处理，会不会读到悬垂指针？

**在 BqLog 中不会发生这种情况：**

- 在 `info`（或其他日志函数）调用期间，BqLog 就会把所有参数内容完整拷贝到内部环形缓冲区；
- 一旦日志函数返回，意味着该条日志所需数据已经安全存放在 BqLog 的内部缓存中；
- 后续 worker 线程只读内部缓存，不会再访问调用方栈上的数据。

**真正可能出问题的情形是「调用期间参数内容被其他线程修改」：**

```cpp
static std::string global_str = "hello world";   // 这是一个全局变量，有多个线程在同时修改它。

void thread_a()
{
    log_obj.info("This is test param :{}", global_str);
}
```

如果在 `info` 函数的入参处理至缓冲写入的过程里，其他线程修改了 `global_str` 内容，会造成输出内容未定义（虽然 BqLog 会尽量保证不崩溃）。  
结论是：**请保证单条日志调用中传入的参数在调用期间不被修改**，与是否同步/异步无关。

---

## Appender 介绍

Appender 表示日志的最终输出目标，其概念与 Log4j 中的 Appender 基本一致。  
目前 BqLog 提供以下几种 Appender：

### ConsoleAppender

- 输出目标：控制台 / 终端；
- 在 Android 上输出到 ADB Logcat；
- 输出编码：UTF-8 文本。

### TextFileAppender

- 以 UTF-8 文本格式直接输出日志文件；
- 日志文件人类可读，适合快速排查。

### CompressedFileAppender

- 用高性能压缩格式输出日志文件；
- 是 **BqLog 推荐的默认文件输出格式**；
- 在所有 Appender 中性能最高，输出文件体积最小；
- 读取需通过 BqLog 自带解码工具或 `bq::tools::log_decoder`；
- 支持加密（基于 RSA2048 + AES256 的混合加密）。

综合对比如下：

| 名称                     | 输出目标 | 是否明文可读 | 输出性能 | 输出尺寸 | 是否支持加密 |
|--------------------------|---------|-------------|----------|----------|-------------|
| ConsoleAppender          | 控制台   | ✔           | 低       | -        | ✘           |
| TextFileAppender         | 文件     | ✔           | 低       | 大       | ✘           |
| CompressedFileAppender   | 文件     | ✘           | 高       | 小       | ✔           |

> 注：文中提及的「加密」仅指 CompressedFileAppender 支持使用「RSA2048 公钥 + AES256 对称密钥」的混合加密格式。  
> 加密格式基于 OpenSSH 风格的 `ssh-rsa` 公钥文本（PEM），私钥需由 `ssh-keygen` 生成，详见 [日志加密和解密](#7-日志加密和解密)。

---

## 配置说明

所谓「配置」，即 `create_log` 和 `reset_config` 函数中的 `config` 字符串。  
该字符串采用 **properties 文件格式**，支持 `#` 单行注释（需独立成行并以 `#` 开头）。

### 1. 完整示例

```ini
# 这个配置给 log 对象配置了 5 个 Appender，其中有两个 TextFileAppender，会输出到不同的文件。

# 第一个 Appender 名叫 appender_0，类型为 ConsoleAppender
appenders_config.appender_0.type=console
# appender_0 使用系统当地时间
appenders_config.appender_0.time_zone=localtime
# appender_0 会输出所有 6 个等级的日志（注意：不同日志等级之间不要有空格，否则解析失败）
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]

# 第二个 Appender 名叫 appender_1，类型为 TextFileAppender
appenders_config.appender_1.type=text_file
# 使用 GMT 时间（UTC0）
appenders_config.appender_1.time_zone=gmt
# 只输出 info 及以上四个等级日志，其余等级会被忽略
appenders_config.appender_1.levels=[info,warning,error,fatal]
# base_dir_type 决定相对路径的基准目录，这里为 1：
# iOS：/var/mobile/Containers/Data/Application/[APP]/Documents
# Android：[android.content.Context.getExternalFilesDir()]
# HarmonyOS：/data/storage/el2/base/cache
# 其他平台：当前工作目录
appenders_config.appender_1.base_dir_type=1
# appender_1 保存的路径为相对路径 bqLog/normal，采用滚动文件：
# 文件名形如 normal_YYYYMMDD_xxx.log，具体见后文「路径与滚动策略」。
appenders_config.appender_1.file_name=bqLog/normal
# 每个文件最大 10,000,000 字节，超过则新开文件
appenders_config.appender_1.max_file_size=10000000
# 超过 10 天的旧文件会自动清理
appenders_config.appender_1.expire_time_days=10
# 同一输出目录下，该 Appender 所有文件总大小超过 100,000,000 字节时，
# 会按日期从最早文件开始清理
appenders_config.appender_1.capacity_limit=100000000

# 第三个 Appender 名叫 appender_2，类型为 TextFileAppender
appenders_config.appender_2.type=text_file
# 输出所有等级日志
appenders_config.appender_2.levels=[all]
# base_dir_type 为 0：
# iOS：/var/mobile/Containers/Data/Application/[APP]/Library/Application Support
# Android：[android.content.Context.getFilesDir()]
# HarmonyOS：/data/storage/el2/base/files
# 其他平台：当前工作目录
appenders_config.appender_2.base_dir_type=0
# 路径为 bqLog/new_normal，文件名形如 new_normal_YYYYMMDD_xxx.log
appenders_config.appender_2.file_name=bqLog/new_normal

# 第四个 Appender 名叫 appender_3，类型为 CompressedFileAppender
appenders_config.appender_3.type=compressed_file
# 输出所有等级日志
appenders_config.appender_3.levels=[all]
# 保存路径为 ~/bqLog/compress_log，文件名形如 compress_log_YYYYMMDD_xxx.logcompr
appenders_config.appender_3.file_name=~/bqLog/compress_log
# appender_3 输出内容将使用下方 RSA2048 公钥进行混合加密
appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname

# 第五个 Appender 名叫 appender_4，类型为 ConsoleAppender
appenders_config.appender_4.type=console
# appender_4 默认禁用，后续可通过 set_appender_enable 启用
appenders_config.appender_4.enable=false
# 输出所有等级日志
appenders_config.appender_4.levels=[all]
# 仅当 Category 以 ModuleA 或 ModuleB.SystemC 开头时才输出日志，其余忽略
# （Category 的概念见后文「支持分类（Category）的 Log 对象」）
appenders_config.appender_4.categories_mask=[ModuleA,ModuleB.SystemC]

# 整体异步缓冲区大小为 65535 字节，具体含义见后文
log.buffer_size=65535
# 开启日志 Crash 复盘功能，详见「程序异常退出的数据保护」
log.recovery=true
# 仅当日志 Category 匹配以下任一通配符时才处理日志，其余忽略
log.categories_mask=[*default,ModuleA,ModuleB.SystemC]
# 使用异步日志模式（推荐）
log.thread_mode=async
# 当日志等级为 error 或 fatal 时，在每条日志后附带调用栈
log.print_stack_levels=[error,fatal]

# 启用快照功能，快照缓冲区大小为 64KB
snapshot.buffer_size=65536
# 仅记录 info 和 error 等级的日志到快照
snapshot.levels=[info,error]
# 仅当 Category 为 ModuleA.SystemA.ClassA 或以 ModuleB 开头时，才记录到快照
snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
```

### 2. 详细解释

#### `appenders_config`

`appenders_config` 是一组关于 Appender 的配置。  
`appenders_config.<name>.xxx` 中 `<name>` 即 Appender 名称，相同 `<name>` 的配置共同作用于同一个 Appender 实例。

| 名称                         | 是否必须 | 可配置值                                | 默认值             | ConsoleAppender | TextFileAppender | CompressedFileAppender |
|------------------------------|---------|-----------------------------------------|--------------------|-----------------|------------------|------------------------|
| `type`                       | ✔       | `console` / `text_file` / `compressed_file` / `raw_file` | -                  | ✔               | ✔                | ✔（加密需此类型）      |
| `enable`                     | ✘       | `true` / `false`                        | `true`             | ✔               | ✔                | ✔                      |
| `levels`                     | ✘       | 日志等级数组（`[verbose,...]` 或 `[all]`） | `[all]`            | ✔               | ✔                | ✔                      |
| `time_zone`                  | ✘       | `gmt` / `localtime` / `Z` / `UTC` / `utc+8` / `utc-2` / `utc+11:30` 等 | `localtime`（当地时间） | ✔               | ✔                | ✔（影响滚动日期）      |
| `file_name`                  | ✔（文件类） | 相对或绝对路径（不含扩展名）                | -                  | ✘               | ✔                | ✔                      |
| `base_dir_type`             | ✘       | `0` / `1`                               | `0`                | ✘               | ✔                | ✔                      |
| `max_file_size`             | ✘       | 正整数或 `0`                            | `0`（不限制）       | ✘               | ✔                | ✔                      |
| `expire_time_seconds`       | ✘       | 正整数或 `0`                            | `0`（不清理）       | ✘               | ✔                | ✔                      |
| `expire_time_days`          | ✘       | 正整数或 `0`                            | `0`（不清理）       | ✘               | ✔                | ✔                      |
| `capacity_limit`            | ✘       | 正整数或 `0`                            | `0`（不限制）       | ✘               | ✔                | ✔                      |
| `categories_mask`           | ✘       | 字符串数组（`[]`）                       | 空（不过滤）        | ✔               | ✔                | ✔                      |
| `always_create_new_file`    | ✘       | `true` / `false`                        | `false`            | ✘               | ✔                | ✔                      |
| `pub_key`                   | ✘       | RSA2048 公钥（OpenSSH `ssh-rsa` 文本）  | 空（不加密）        | ✘               | ✘                | ✔（启用混合加密）      |

##### (1) `appenders_config.xxx.type`

指定 Appender 类型：

- `console` → [ConsoleAppender](#consoleappender)
- `text_file` → [TextFileAppender](#textfileappender)
- `compressed_file` → [CompressedFileAppender](#compressedfileappender)

##### (2)`appenders_config.xxx.enable`

是否默认启用该 Appender，默认为 `true`。  
如为 `false`，Appenders 会在初始化时被创建但不实际输出，可在运行时通过 `set_appender_enable` 切换。

##### (3)`appenders_config.xxx.levels`

使用 `[]` 包裹的数组，内容为：

- 任意组合：`[verbose,debug,info,warning,error,fatal]`
- 或特殊值 `[all]`，表示所有等级均输出。


##### (4)`appenders_config.xxx.time_zone`

指定时间戳格式化使用的时区，同时也影响按日期滚动文件的「日期边界」：

- `"gmt"`、`"Z"`、`"UTC"`：使用 UTC0（格林威治时间）；
- `"localtime"`：使用系统当地时间；
- `"utc+8"`、`"utc-2"`、`"utc+11:30"` 等：明确指定偏移。

影响：

- ConsoleAppender / TextFileAppender：决定日志文本中时间字段的显示；
- TextFileAppender / CompressedFileAppender / RawFileAppender：决定文件按日期滚动的切分点（每天 0 点）。

##### (5)`appenders_config.xxx.base_dir_type`

指定当 `file_name` 为相对路径时的基准目录：

- `0`
  - Android：依次尝试
    - `android.content.Context.getFilesDir()`
    - `android.content.Context.getExternalFilesDir()`
    - `android.content.Context.getCacheDir()`
  - iOS：`/var/mobile/Containers/Data/Application/[APP]/Library/Application Support`
  - HarmonyOS：`/data/storage/el2/base/files`
  - 其他平台：进程当前工作目录
- `1`
  - Android：依次尝试
    - `android.content.Context.getExternalFilesDir()`
    - `android.content.Context.getFilesDir()`
    - `android.content.Context.getCacheDir()`
  - iOS：`/var/mobile/Containers/Data/Application/[APP]/Documents`
  - HarmonyOS：`/data/storage/el2/base/cache`
  - 其他平台：进程当前工作目录

##### (6)`appenders_config.xxx.file_name`

指定日志文件的路径与文件名前缀，示例：

```ini
appenders_config.appender_3.file_name=~/bqLog/compress_log
```

最终实际文件名会由 BqLog 按日期与滚动编号自动补全，例如：

- `compress_log_20250101_0001.logcompr`（CompressedFileAppender）
- `normal_20250101_0001.log`（TextFileAppender）

若是相对路径，则基于 `base_dir_type` 对应的目录。

##### 其他字段简要说明

- `max_file_size`：单个文件最大字节数，超过则新建文件；`0` 表示不按大小切分。
- `expire_time_seconds`：按秒数清理超时文件；`0` 关闭该功能。
- `expire_time_days`：按天清理超时文件；`0` 关闭该功能。
- `capacity_limit`：限制该 Appender 输出的文件的总大小，超过时按时间从旧文件开始删除。
- `categories_mask`：仅当日志 Category 匹配该数组中的前缀时，才会输出日志（参见 [支持分类（Category）的 Log 对象](#2-支持分类category的-log-对象)）。
- `always_create_new_file`：`true` 时，即使同一天内，每次进程重启也新开一个文件；默认 `false` 为追加写。
- `pub_key`：为 CompressedFileAppender 提供加密公钥，字符串内容应完整拷贝自 `ssh-keygen` 生成的 `.pub` 文件，且以 `ssh-rsa ` 开头。 详情见 [日志加密和解密](#6-日志加密和解密)。

---

### `log` 配置

`log.xxx` 配置作用于整个 Log 对象。

| 名称                                      | 是否必须 | 可配置值                               | 默认值                                                         | 是否可通过 `reset_config` 修改 |
|-------------------------------------------|---------|----------------------------------------|----------------------------------------------------------------|--------------------------------|
| `log.thread_mode`                         | ✘       | `sync` / `async` / `independent`       | `async`                                                        | ✘                              |
| `log.buffer_size`                         | ✘       | 32 位正整数                            | 桌面/服务器：`65536`；移动设备：`32768`                         | ✘                              |
| `log.recovery`                            | ✘       | `true` / `false`                       | `false`                                                        | ✘                              |
| `log.categories_mask`                     | ✘       | 字符串数组（`[]`）                      | 空（不过滤）                                                   | ✔                              |
| `log.print_stack_levels`                  | ✘       | 日志等级数组                           | 空（不打印调用栈）                                             | ✔                              |
| `log.buffer_policy_when_full`             | ✘       | `discard` / `block` / `expand`         | `block`                                                        | ✘                              |
| `log.high_perform_mode_freq_threshold_per_second` | ✘ | 64 位正整数                            | `1000`                                                         | ✘                              |

#### `log.thread_mode`

决定缓冲区中的数据由哪个线程处理：

- `sync`：同步日志模式。写日志的线程直接负责处理和输出日志，调用结束即完成输出；（性能低，不推荐）
- `async`（默认）：异步模式。写日志线程只写缓冲区，由全局 worker 线程统一处理所有异步 log 对象的输出；
- `independent`：独立异步模式。为该 Log 对象单独创建一个专属 worker 线程。适合单个 Log 写入量极大、希望完全与其他 Log 解耦的场景。

#### `log.buffer_size`

日志缓冲区大小（字节）。  
缓冲越大，可承受的突发写入峰值越大，但内存占用也会增加。

#### `log.recovery`

- `true`  
  当异步日志（log.thread _mode为`async`或者`independent`)遇到程序异常退出时，缓冲区中还未落盘的数据会在下次启动后重新写入日志文件。
- `false`  
  进程异常退出且未调用 `force_flush()` 时，缓冲中的日志数据将丢失。

具体行为详见 [程序异常退出的数据保护](#3-程序异常退出的数据保护)。

#### `log.categories_mask`

行为与 `appenders_config.xxx.categories_mask` 一致，但作用范围为整个 Log 对象。

- 对同步日志：在调用线程就进行过滤；
- 对异步日志：在写入缓冲时进行过滤，减少不必要的数据进出。

详见 [支持分类（Category）的 Log 对象](#2-支持分类category的-log-对象)。

#### `log.print_stack_levels`

配置方式类似 `appenders_config.xxx.levels`，用于指定哪些日志等级需要自动附带调用栈。例如：

```ini
log.print_stack_levels=[error,fatal]
```

建议仅在 Debug / 测试环境启用，避免对线上性能造成明显影响。

#### `log.buffer_policy_when_full`

当缓冲区写满后的行为：

- `discard`：丢弃新写入的日志，直到缓冲区有足够空间；
- `block`（推荐默认）：写日志的线程会阻塞等待缓冲区有空间；
- `expand`（不推荐）：缓冲区会动态扩容为原来两倍，直到可写。  
  可能显著增加内存占用，虽然 BqLog 通过良好的线程调度减少了扩容次数，但仍建议谨慎使用。

#### `log.high_perform_mode_freq_threshold_per_second`

该配置项用于控制「高性能模式」触发阈值：

- 当单个线程在一秒内记录的日志条数超过该值时，该线程将自动进入高性能模式；
- 高性能模式下，会为该线程分配更适合高频写入的内部资源；
- 当写入频率下降时，会自动退出高性能模式并释放相关资源。

默认值为 `1000`（条/秒）。  
配置为 `0` 表示关闭该功能。

为减少内存碎片，物理内存的分配通常以「若干个高速缓存」为一组进行批量申请（桌面平台为 16 个，高端移动平台通常为 2 个）。因此即使只有一个线程进入高性能模式，也会额外占用一组缓存的空间。

---

### `snapshot` 配置

`snapshot.xxx` 用于配置该 Log 对象的日志快照功能。  
适用于：检测到异常后需要上报该 Log 的「最近一段时间」日志。

| 名称                   | 是否必须 | 可配置值       | 默认值 | 是否可通过 `reset_config` 修改 |
|------------------------|---------|----------------|--------|--------------------------------|
| `snapshot.buffer_size` | ✘       | 32 位正整数    | `0`    | ✔                              |
| `snapshot.levels`      | ✘       | 日志等级数组   | `[all]`| ✔                              |
| `snapshot.categories_mask` | ✘   | 字符串数组     | 空     | ✔                              |

- `snapshot.buffer_size`：快照缓冲大小（字节），为 `0` 或未配置时，快照功能关闭；
- `snapshot.levels`：仅配置中的等级会被写入快照；若不配置则默认为 `[all]`（与前文 `levels` 默认行为略有不同）；
- `snapshot.categories_mask`：行为与 `appenders_config.xxx.categories_mask` 一致，仅匹配的 Category 会被写入快照；未配置则不过滤所有 Category。

---

## 离线解码二进制格式的 Appender

在程序之外，BqLog 提供了预编译的命令行工具用于解码压缩二进制日志文件。  
从 Releases 下载对应操作系统与架构的工具包 `{os}_{arch}_tools_{version}`，解压后可找到：

- `BqLog_LogDecoder`

用法：

```bash
./BqLog_LogDecoder 要解码的文件 [-o 输出文件] [-k 私钥文件]
```

- 未指定 `-o` 时，解码结果直接输出到标准输出；
- 如日志文件是加密格式，需要通过 `-k` 指定私钥文件路径（详见 [日志加密和解密](#7-日志加密和解密)）；
- **注意：不同版本的 BqLog 之间二进制格式可能不兼容**，请使用匹配版本的解码器。

---

## 构建说明

对于需要自行修改与编译 BqLog 的用户，所有构建脚本位于 `/build` 目录：

```text
/build
├── demo       # Demo 构建
├── lib        # Native 静态库与动态库构建
├── test       # 测试工程构建
├── tools      # 工具工程构建（包含 LogDecoder、CategoryLogGenerator 等）
├── wrapper    # 各语言 Wrapper 工程（Java / C# 等）
├── benchmark  # Benchmark 工程构建
└── plugin     # 游戏引擎插件构建（Unity / 团结引擎 / Unreal）
```

### 1. 库构建

不同平台的构建脚本位于 `/build/lib` 下。构建前请确保设置好必要环境变量：

- `ANDROID_NDK_ROOT`：Android NDK 的路径（编译 Android 库必需）；
- `JAVA_HOME`：JDK 路径（大多数脚本默认开启 Java Wrapper，如您不需要，可自行在 CMake 或脚本中去掉 `-DJAVA_SUPPORT=ON`）。

### 2. Demo 构建和运行

Demo 工程的构建脚本位于 `/build/demo`。  
注意：C# 和 Java 的 Demo 需要手动将对应平台的动态库放置到运行时可搜索到的路径中（或通过配置环境变量 / 运行参数指定）。

### 3. 自动测试运行说明

测试工程的生成脚本与「生成 + 运行」脚本位于 `/build/test`。  
建议在提交前确保所有测试用例通过。

### 4. Benchmark 运行说明

Benchmark 工程脚本位于 `/build/benchmark`。  
同样包含生成工程脚本与直接运行脚本，可用于评估不同平台和配置下的性能表现。

---

## 高级使用话题

### 1. 无 Heap Alloc

在 Java、C#、TypeScript 等运行时中，通常日志库在每条日志写入时都会产生少量 Heap 分配，随着时间推移会触发 GC 并影响性能。  
BqLog 在这些平台上通过以下方式尽力做到「零或极低 Heap Alloc」：

- **内部避免在日志路径创建临时对象与字符串**；
- **避免可变参数带来的额外数组分配**（C# 中通过多态重载规避）；
- **减少装箱/拆箱（boxing/unboxing）**：

  - 在 C# Wrapper 中，当参数个数 ≤ 12 时，不会产生装箱拆箱操作，超过 12 个参数才会退化为装箱；
  - TypeScript 通过 NAPI 直接传递参数，避免了多层装箱；
  - Java Wrapper 中采用特殊工具方法手动避免装箱，例如：

```java
// Java
// 使用 bq.utils.param.no_boxing 包裹的 primitive 参数不会产生装箱，
// 裸传的 5.3245f 则会产生装箱，触发 GC 风险上升。
import static bq.utils.param.no_boxing;

my_demo_log.info(
    my_demo_log.cat.node_2.node_5,
    "Demo Log Test Log, {}, {}",
    no_boxing(false),
    5.3245f
);
```

合理使用上述 API，可显著减少 GC 干扰，获得稳定的高性能日志行为。

---

### 2. 支持分类（Category）的 Log 对象

#### Category 概念与使用

在 Unreal 引擎中，日志有 Category（类别）概念，但原生接口对代码提示不够友好。  
在 BqLog 中，Category 用于标识「某条日志属于哪个模块 / 子系统」，并支持多级层次结构。

例如，我们定义一个业务日志对象，其 Category 树大致为：

```text
*default
├── Shop 
│   ├── Manager 
│   └── Seller 
├── Factory
│   ├── People 
│   │   ├── Manager 
│   │   └── Worker 
│   ├── Machine 
│   └── House 
└── Transport 
    ├── Vehicles
    │   ├── Driver
    │   └── Maintenance
    └── Trains
```

使用方式（C++ 示例）：

```cpp
my_category_log.info("Log0");  // Category = *default
my_category_log.info(my_category_log.cat.Shop, "Log1");  // Category = Shop
my_category_log.info(my_category_log.cat.Shop.Seller, "Log2"); // Category = Shop.Seller
my_category_log.info(my_category_log.cat.Transport.Vehicles.Driver, "Log3"); // Category = Transport.Vehicles.Driver
my_category_log.info(my_category_log.cat.Factory, "Log4"); // Category = Factory
my_category_log.info(my_category_log.cat.Factory.People, "Log5"); // Category = Factory.People
```

输出示例：

```text
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] Log0
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Shop] Log1
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Shop.Seller] Log2
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Transport.Vehicles.Driver] Log3
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Factory] Log4
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Factory.People] Log5
```

配合前文配置中的 `categories_mask`，可以在输出侧进行灵活过滤。  
结合 [Console 回调](#拦截console输出)，您可以通过 `category_idx` + 下述 API 获取 Category 名称列表：

```cpp
/// <summary>
/// get log categories count
/// </summary>
decltype(categories_name_array_)::size_type get_categories_count() const;

/// <summary>
/// get names of all categories
/// </summary>
const bq::array<bq::string>& get_categories_name_array() const;
```

这常用于在自定义 UI 中展示多维过滤器。

#### Category 日志类生成

支持 Category 的 Log 类并不是默认的 `bq::log` / `bq.log`，而是需要由工具生成的专用类。  
生成步骤如下：

1. 准备一个文本配置文件，定义所有 Category：

   **BussinessCategories.txt**

   ```text
   // 该配置文件支持用双斜杠进行注释
   Shop.Manager      // 不必单独列出 Shop，这一行会自动生成 Shop 和 Shop.Manager
   Shop.Seller
   Factory.People.Manager
   Factory.People.Worker
   Factory.Machine
   Factory.House
   Transport.Vehicles.Driver
   Transport.Vehicles.Maintenance
   Transport.Trains
   ```

2. 使用 BqLog 自带命令行工具生成对应类：  
   在 Releases 中下载 `{os}_{arch}_tools_{version}`，解压后找到：

  - `BqLog_CategoryLogGenerator`

3. 使用方式：

   ```bash
   ./BqLog_CategoryLogGenerator 要生成的类名 Category配置文件 [输出目录，默认当前目录]
   ```

   示例：

   ```bash
   ./BqLog_CategoryLogGenerator business_log /path/to/BussinessCategories.txt ./
   ```

   将在当前目录下生成 5 个文件：

  - `business_log.h`（C++ header wrapper）
  - `business_log.java`（Java wrapper）
  - `business_log.cs`（C# wrapper）
  - `business_log.ts`（TypeScript wrapper）
  - `business_log_for_UE.h`（配合 UE 工程，可在蓝图中引用 Category）

4. 在工程中引入这些文件，即可使用带 Category 的日志类。  
   例如 C++：

   ```cpp
   bq::business_log my_log = bq::business_log::create_log("MyLog", config);
   ```

   或获取已创建的同名 Log 对象：

   ```cpp
   bq::business_log my_log = bq::business_log::get_log_by_name("MyLog");
   ```

   对 `my_log.cat` 使用 `.` 补全，即可获得事先定义好的 Category 列表。  
   如不传递 Category 参数，则默认使用 `*default`。

---

### 3. 程序异常退出的数据保护

当 BqLog 使用异步模式时，如果程序非正常退出（崩溃等），可能造成缓冲中的日志尚未来得及落盘。  
BqLog 提供了两种机制尽力减少损失：

#### 1）异常信号处理机制（POSIX）

```cpp
/// <summary>
/// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk.
/// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash.
/// However, this functionality does not guarantee success, and only supports POSIX systems.
/// </summary>
static void enable_auto_crash_handle();
```

调用 `bq::log::enable_auto_crash_handle()` 后，BqLog 会在 POSIX 系统上注册若干信号处理器：

- 当进程收到如 `SIGABRT`、`SIGSEGV`、`SIGBUS` 等异常信号时，尝试在信号处理回调中强制刷新缓冲区（`force_flush_all_logs`）；
- 内部通过 `sigaction` 实现，并且在注册前会保存旧的信号处理句柄，在自身处理结束后再调用原有处理逻辑，尽量降低对宿主程序的影响。

不过需要注意：

- 该机制本质上是「紧急补救」，**不能保证 100% 成功**——如果内存本身已严重破坏，任何操作都可能失败；
- 该机制仅作用于 POSIX 平台，Windows 上不会生效。

#### 2）复盘机制（Recovery）

参考配置项 [`log.recovery`](#logrecovery)。  
当该项为 `true` 时，BqLog 在部分平台上会尝试使用平台特性，尽量保证异步缓冲区中的内容在磁盘上有临时存根；下一次启动时，可以进行「复盘」，尽量恢复未完全落盘的日志。

具体实现细节依赖操作系统能力，会在未来版本中持续增强。

---

### 4. 自定义参数类型

在前文 [format 参数](#3-format参数) 中已说明，默认支持大量常见类型。  
若需扩展自定义类型，有两种方式：

> **重要提示：**  
> 请务必在 `bq_log.h` 或生成的 Category 头文件之前先 `#include` 您的自定义类和相关函数声明。  
> 部分编译器（尤其是 Clang）在 include 顺序不正确时可能编译失败。

#### 方法一：在类中实现 `bq_log_format_str_size()` 与 `bq_log_format_str_chars()`

```cpp
// custom_bq_log_type.h
class A {
private:
    bool value_;

public:
    explicit A(bool value) : value_(value) {}

    // 返回「字符个数」而非「字节数」，返回类型必须是 size_t
    size_t bq_log_format_str_size() const {
        return value_ ? strlen("true") : strlen("false");
    }

    // 返回实际字符串首字符地址，可以是 char* / char16_t* / char32_t* / wchar_t*
    const char* bq_log_format_str_chars() const {
        return value_ ? "true" : "false";
    }
};
```

使用示例：

```cpp
#include "custom_bq_log_type.h"
#include "bq_log/bq_log.h"

void output(const bq::log& log_obj)
{
    log_obj.info("This should be Class A1:{}, A2:{}", A(true), A(false));
}
```

#### 方法二：实现全局的 `bq_log_format_str_size()` 与 `bq_log_format_str_chars()`

适用于无法改动类型定义的情况（如 Unreal 的 `FString`、`FName` 等），或希望覆盖内置类型的默认输出方式。

由于自定义类型的优先级高于内置类型，您甚至可以重定义 `int32_t` 的输出，例如：

```cpp
// custom_bq_log_type.h
#pragma once
#include <map>
#include <cinttypes>
#include <cstring>

// 覆盖 int32_t 默认输出行为
size_t bq_log_format_str_size(const int32_t& param);
const char* bq_log_format_str_chars(const int32_t& param);

// 让 std::map 能作为参数被传入
template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param);

template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param);

// 实现
template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param)
{
    if (param.empty()) {
        return strlen("empty");
    } else {
        return strlen("full");
    }
}

// 使用 UTF-16 编码
template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param)
{
    if (param.empty()) {
        return u"empty";
    } else {
        return u"full";
    }
}
```

```cpp
// custom_bq_log_type.cpp
#include "custom_bq_log_type.h"

size_t bq_log_format_str_size(const int32_t& param)
{
    if (param > 0) {
        return strlen("PLUS");
    } else if (param < 0) {
        return strlen("MINUS");
    } else {
        return strlen("ZERO");
    }
}

const char* bq_log_format_str_chars(const int32_t& param)
{
    if (param > 0) {
        return "PLUS";
    } else if (param < 0) {
        return "MINUS";
    } else {
        return "ZERO";
    }
}
```

使用示例：

```cpp
#include "custom_bq_log_type.h"   // 确保在 bq_log.h 之前
#include "bq_log/bq_log.h"

void output(const bq::log& my_category_log)
{
    std::map<int, bool> param0;
    std::map<int, bool> param1;
    param0[5] = false;

    my_category_log.info("This should be full:{}", param0);   // 输出 full
    my_category_log.info("This should be empty:{}", param1);  // 输出 empty
    my_category_log.info("This should be PLUS:{}", 5);        // 输出 PLUS
    my_category_log.info("This should be MINUS:{}", -1);      // 输出 MINUS
    my_category_log.info(param0);                             // 输出 full
}
```

---

### 5. 在 Unreal 中使用 BqLog

#### 1）对 `FName` / `FString` / `FText` 的支持

在 Unreal 环境中，BqLog 内置了适配器：

- 自动支持 `FString`、`FName`、`FText` 作为 format 字符串和参数；
- 兼容 UE4 与 UE5。

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

如果希望自定义适配行为，也可使用前文「方法二（全局函数）」的方式，自行定义 `bq_log_format_str_size` 与 `bq_log_format_str_chars`。

#### 2）将 BqLog 的输出转接到 Unreal 日志窗口

如果已按 [Unreal Engine 集成说明](#unreal-engine) 引入 Unreal 插件，BqLog 日志会自动转接到 Unreal 的 Output Log 中。  
若未使用插件，而是直接在 C++ 级别集成 BqLog，可以自行使用 console 回调进行转发：

```cpp
// 你可以根据不同的 category_idx / log_id 获取 Log 名称与 Category 名称，
// 将它们转发到不同的 UE_LOG Category 中。
static void on_bq_log(uint64_t log_id,
                      int32_t category_idx,
                      int32_t log_level,
                      const char* content,
                      int32_t length)
{
    switch (log_level)
    {
    case (int32_t)bq::log_level::verbose:
        UE_LOG(LogTemp, VeryVerbose, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    case (int32_t)bq::log_level::debug:
        UE_LOG(LogTemp, Verbose, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    case (int32_t)bq::log_level::info:
        UE_LOG(LogTemp, Log, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    case (int32_t)bq::log_level::warning:
        UE_LOG(LogTemp, Warning, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    case (int32_t)bq::log_level::error:
        UE_LOG(LogTemp, Error, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    case (int32_t)bq::log_level::fatal:
        UE_LOG(LogTemp, Fatal, TEXT("%s"), UTF8_TO_TCHAR(content));
        break;
    default:
        break;
    }
}

void CallThisOnYourGameStart()
{
    bq::log::register_console_callback(&on_bq_log);
}
```

#### 3）在蓝图中使用 BqLog

已按 [Unreal Engine 集成说明](#unreal-engine) 引入插件后，可在蓝图中直接调用 BqLog：

1. **创建日志 Data Asset**

  - 在 Unreal 工程中创建 Data Asset，类型选择 BqLog：
    - 默认 Log 类型（不带 Category）：  
      <img src="docs/img/ue_pick_data_asset_1.png" alt="默认Log创建" style="width: 455px">
    - 若按 [Category 日志类生成](#category-日志类生成) 生成了带 Category 的日志类，并将 `{category}.h` 与 `{category}_for_UE.h` 加入工程：  
      <img src="docs/img/ue_pick_data_asset_2.png" alt="Category Log创建" style="width: 455px">

2. **配置日志参数**

  - 双击打开 Data Asset，配置日志对象名与创建方式：
    - `Create New Log`：在运行时新建一个 Log 对象：  
      <img src="docs/img/ue_create_log_config_1.png" alt="配置Log参数 Create New Log" style="width: 455px">
    - `Get Log By Name`：仅获取其他地方已经创建的同名 Log：  
      <img src="docs/img/ue_create_log_config_2.png" alt="配置Log参数 Get Log By Name" style="width: 455px">

3. **在蓝图中调用日志节点**

   <img src="docs/img/ue_print_log.png" alt="蓝图调用Log" style="width: 655px">

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

### 6. 日志加密和解密

对于外发客户端（尤其是互联网游戏和 App），日志加密是重要需求。  
在 1.x 版本中，BqLog 的二进制日志中仍有大量明文。自 2.x 起，引入了完整的日志加密方案。  
该方案性能极高，几乎无感知开销，且安全性良好。

#### 1）加密算法说明

BqLog 使用 **RSA2048 + AES256** 的混合加密：

- 仅适用于 `CompressedFileAppender`；
- 使用 `ssh-keygen` 生成的 RSA2048 密钥对：
  - 公钥：OpenSSH `ssh-rsa ...` 文本（本质为 PKCS#8 公钥的 OpenSSH 表达形式）；
  - 私钥：PEM 格式，`-----BEGIN RSA PRIVATE KEY-----` 或 `-----BEGIN OPENSSH PRIVATE KEY-----` 块（兼容 PKCS#1/PKCS#8 私钥表示）；
- 日志写入时：
  - 使用公钥对随机生成的 AES256 对称密钥加密；
  - 实际日志内容通过 AES256 加密；
  - 整体编码为 BqLog 自定义的加密压缩格式。

因此，从密码学与标准格式角度看：

- **公钥**：OpenSSH `ssh-rsa` 文本，底层为 PKCS#8 兼容 RSA 公钥；
- **私钥**：PEM 编码的 RSA 私钥（PKCS#1 或 PKCS#8），BqLog 工具在解析时兼容 `ssh-keygen` 默认输出。

#### 2）配置加密

在终端执行：

```bash
ssh-keygen -t rsa -b 2048 -m PEM -N "" -f "你的密钥文件路径"
```

将生成两份文件：

- `<你的密钥文件路径>`：私钥文件，如 `id_rsa`，通常以 `-----BEGIN RSA PRIVATE KEY-----` 开头；
- `<你的密钥文件路径>.pub`：公钥文件，如 `id_rsa.pub`，以 `ssh-rsa ` 开头。

请确保：

- 公钥内容以 `ssh-rsa ` 开头；
- 私钥内容为标准 PEM 块。

在对应的 CompressedFileAppender 配置中加入：

```properties
appenders_config.{AppenderName}.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname
```

其中 value 即公钥文件中的整行内容（可去掉末尾换行，但不要破坏中间空格）。

这样，该 Appender 输出的日志内容即为使用上述 RSA2048 公钥 + AES256 对称密钥进行混合加密的格式。

#### 3）解密日志

解密加密日志时，需要私钥文件。推荐使用 BqLog 自带命令行工具 `BqLog_LogDecoder`：

- 使用方法参见 [离线解码二进制格式的 Appender](#离线解码二进制格式的appender)。

示例：

```bash
./BqLog_LogDecoder 要解码的文件 -o 输出文件 -k "./你的私钥文件路径"
```

其中：

- `要解码的文件`：压缩加密日志文件路径；
- `-o 输出文件`：可选，指定解码后的文本保存路径，不填则输出到标准输出；
- `-k "./你的私钥文件路径"`：指向 `ssh-keygen` 生成的私钥文件（PEM），支持常见 PKCS#1 / PKCS#8 形式。

---

## Benchmark

### 1. Benchmark 说明

测试环境：

- **CPU**：13th Gen Intel(R) Core(TM) i9-13900K @ 3.00 GHz
- **Memory**：128 GB
- **OS**：Windows 11

测试用例如下：

- 使用 1～10 个线程同时写日志；
- 每个线程写入 2,000,000 条日志：
  - 一种为带 4 个参数的格式化日志；
  - 一种为不带参数的纯文本日志；
- 等待所有线程结束，再调用 `force_flush_all_logs()`，统计从开始写入到所有日志落盘的总耗时。

对比对象：

- BqLog 2.x（C++ / Java，TextFileAppender 与 CompressedFileAppender）
- Log4j2（仅 TextFileAppender，**不使用滚动时 gzip 压缩**，因为其压缩是在滚动时对已存在文本文件再做 gzip，性能模型不同，无法公平对比）

### 2. BqLog C++ Benchmark 代码

```cpp
#if defined(WIN32)
#include <windows.h>
#endif
#include "bq_log/bq_log.h"
#include <stdio.h>
#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <vector>

void test_compress_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 1, 4 params=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread*> threads(thread_count);

    uint64_t start_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx] = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("idx:{}, num:{}, This test, {}, {}",
                    idx, i, 2.4232f, true);
            }
        });
    }

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }

    bq::log::force_flush_all_logs();

    uint64_t flush_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Time Cost:" << (flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_text_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 2, 4 params============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads(thread_count);

    uint64_t start_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx] = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("idx:{}, num:{}, This test, {}, {}",
                    idx, i, 2.4232f, true);
            }
        });
    }

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }

    bq::log::force_flush_all_logs();

    uint64_t flush_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Time Cost:" << (flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_compress_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 3, no param=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread*> threads(thread_count);

    uint64_t start_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx] = new std::thread([&log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("Empty Log, No Param");
            }
        });
    }

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }

    bq::log::force_flush_all_logs();

    uint64_t flush_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Time Cost:" << (flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_text_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 4, no param============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads(thread_count);

    uint64_t start_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx] = new std::thread([&log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("Empty Log, No Param");
            }
        });
    }

    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }

    bq::log::force_flush_all_logs();

    uint64_t flush_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    std::cout << "Time Cost:" << (flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

int main()
{
#ifdef BQ_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    bq::log compressed_log = bq::log::create_log("compress", R"(
        appenders_config.appender_3.type=compressed_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name=benchmark_output/compress_
        appenders_config.appender_3.capacity_limit=1
    )");

    bq::log text_log = bq::log::create_log("text", R"(
        appenders_config.appender_3.type=text_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name=benchmark_output/text_
        appenders_config.appender_3.capacity_limit=1
    )");

    std::cout << "Please input the number of threads which will write log simultaneously:" << std::endl;
    int32_t thread_count = 0;
    std::cin >> thread_count;

    // 用一条日志触发 capacity_limit，确保旧日志文件被清理
    compressed_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
    text_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
    bq::log::force_flush_all_logs();

    test_compress_multi_param(thread_count);
    test_text_multi_param(thread_count);
    test_compress_no_param(thread_count);
    test_text_no_param(thread_count);

    return 0;
}
```

### 3. BqLog Java Benchmark 代码

```java
import java.util.Scanner;

/**
 * 请在运行前确保当前平台对应的动态库已在 java.library.path 内，
 * 或在 IDE 中配置 Native Library Location 指向 (ProjectRoot)/dist 下的动态库目录。
 * 否则可能遇到 UnsatisfiedLinkError。
 */
public class benchmark_main {

    static abstract class benchmark_thread implements Runnable {
        protected int idx;
        public benchmark_thread(int idx) {
            this.idx = idx;
        }
    }

    private static void test_compress_multi_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("=========Begin Compressed File Log Test 1, 4 params=========");
        bq.log log_obj = bq.log.get_log_by_name("compress");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}",
                            bq.utils.param.no_boxing(idx),
                            bq.utils.param.no_boxing(i),
                            bq.utils.param.no_boxing(2.4232f),
                            bq.utils.param.no_boxing(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        bq.log.force_flush_all_logs();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    private static void test_text_multi_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 2, 4 params============");
        bq.log log_obj = bq.log.get_log_by_name("text");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}",
                            bq.utils.param.no_boxing(idx),
                            bq.utils.param.no_boxing(i),
                            bq.utils.param.no_boxing(2.4232f),
                            bq.utils.param.no_boxing(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        bq.log.force_flush_all_logs();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    private static void test_compress_no_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("=========Begin Compressed File Log Test 3, no param=========");
        bq.log log_obj = bq.log.get_log_by_name("compress");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        bq.log.force_flush_all_logs();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    private static void test_text_no_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 4, no param============");
        bq.log log_obj = bq.log.get_log_by_name("text");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        bq.log.force_flush_all_logs();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    public static void main(String[] args) throws Exception {
        bq.log compressed_log =  bq.log.create_log("compress", """
            appenders_config.appender_3.type=compressed_file
            appenders_config.appender_3.levels=[all]
            appenders_config.appender_3.file_name=benchmark_output/compress_
            appenders_config.appender_3.capacity_limit=1
        """);

        bq.log text_log =  bq.log.create_log("text", """
            appenders_config.appender_3.type=text_file
            appenders_config.appender_3.levels=[all]
            appenders_config.appender_3.file_name=benchmark_output/text_
            appenders_config.appender_3.capacity_limit=1
        """);

        System.out.println("Please input the number of threads which will write log simultaneously:");
        int thread_count = 0;

        try (Scanner scanner = new Scanner(System.in)) {
            thread_count = scanner.nextInt();
        } catch (Exception e) {
            e.printStackTrace();
            return;
        }

        compressed_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
        text_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
        bq.log.force_flush_all_logs();

        test_compress_multi_param(thread_count);
        test_text_multi_param(thread_count);
        test_compress_no_param(thread_count);
        test_text_no_param(thread_count);
    }

}
```

### 4. Log4j Benchmark 代码

Log4j2 部分只测试了文本输出格式，因为其 gzip 压缩是在「滚动时对已有文本文件重新 gzip 压缩」，这与 BqLog 实时压缩模式的性能模型完全不同，无法直接对标。

**依赖：**

```xml
<!-- pom.xml -->
<dependency>
  <groupId>org.apache.logging.log4j</groupId>
  <artifactId>log4j-api</artifactId>
  <version>2.23.1</version>
</dependency>
<dependency>
  <groupId>org.apache.logging.log4j</groupId>
  <artifactId>log4j-core</artifactId>
  <version>2.23.1</version>
</dependency>
<dependency>
  <groupId>com.lmax</groupId>
  <artifactId>disruptor</artifactId>
  <version>3.4.2</version>
</dependency>
```

启用 AsyncLogger：

```properties
# log4j2.component.properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

Log4j2 配置：

```xml
<!-- log4j2.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <Console name="Console" target="SYSTEM_OUT">
      <PatternLayout pattern="%d{HH:mm:ss.SSS} [%t] %-5level %logger{36} - %msg%n"/>
    </Console>

    <!-- RollingRandomAccessFile，用于演示文本输出 -->
    <RollingRandomAccessFile name="my_appender"
                             fileName="logs/compress.log"
                             filePattern="logs/compress-%d{yyyy-MM-dd}-%i.log"
                             immediateFlush="false">
      <PatternLayout>
        <Pattern>%d{yyyy-MM-dd HH:mm:ss} [%t] %-5level %logger{36} - %msg%n</Pattern>
      </PatternLayout>
      <Policies>
        <TimeBasedTriggeringPolicy interval="1" modulate="true"/>
      </Policies>
      <DefaultRolloverStrategy max="5"/>
    </RollingRandomAccessFile>

    <!-- Async Appender -->
    <Async name="Async" includeLocation="false" bufferSize="262144">
      <!-- <AppenderRef ref="Console"/> -->
      <AppenderRef ref="my_appender"/>
    </Async>
  </Appenders>

  <Loggers>
    <Root level="info">
      <AppenderRef ref="Async"/>
    </Root>
  </Loggers>
</Configuration>
```

源代码：

```java
package bq.benchmark.log4j;

import java.util.Scanner;
import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.core.async.AsyncLoggerContextSelector;

import static org.apache.logging.log4j.util.Unbox.box;

public class main {

    public static final Logger log_obj = LogManager.getLogger(main.class);

    static abstract class benchmark_thread implements Runnable {
        protected int idx;
        protected Logger log_obj;
        public benchmark_thread(int idx, Logger log_obj) {
            this.idx = idx;
            this.log_obj = log_obj;
        }
    }

    private static void test_text_multi_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 1, 4 params============");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx, log_obj) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}",
                            box(idx), box(i), box(2.4232f), box(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        org.apache.logging.log4j.core.LoggerContext context =
            (org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false);
        context.stop();
        LogManager.shutdown();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    private static void test_text_no_param(int thread_count) throws Exception {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 1, no param============");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");

        for (int idx = 0; idx < thread_count; ++idx) {
            Runnable r = new benchmark_thread(idx, log_obj) {
                @Override
                public void run() {
                    for (int i = 0; i < 2000000; ++i) {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }

        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        org.apache.logging.log4j.core.LoggerContext context =
            (org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false);
        context.stop();
        LogManager.shutdown();

        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println();
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Please input the number of threads which will write log simultaneously:");
        int thread_count = 0;

        try (Scanner scanner = new Scanner(System.in)) {
            thread_count = scanner.nextInt();
        } catch (Exception e) {
            e.printStackTrace();
            return;
        }

        System.out.println("Is Async:" + AsyncLoggerContextSelector.isSelected());

        // 这两个测试需分开运行，因为强制关闭后 LoggerContext 不再可用。
        test_text_multi_param(thread_count);
        // test_text_no_param(thread_count);
    }

}
```

### 5. Benchmark 结果

所有耗时单位为毫秒，数值越小代表性能越高。  
从结果可见：

- 在 TextFileAppender 场景下，BqLog 相比 Log4j2 有约 **3 倍** 性能优势；
- 在 CompressedFileAppender 场景下，BqLog 相比 Log4j2 文本输出有约 **10 倍以上** 性能优势；
- 若与 BqLog 1.5 版本相比，2.x 平均性能提升约 **40%**。


> 为了排版清晰，没有加入BqLog1.5的数据，可以直接和旧版本的文档的[Benchmark](https://github.com/Tencent/BqLog/blob/stable_1.5/README_CHS.md#5-benchmark%E7%BB%93%E6%9E%9C)结果对比。采取了一样的硬件和操作系统环境，相同的测试用例，相同的Log4j的结果。 从结果上看BqLog 2.x版本对比1.5版本大约有40%左右的性能提升。
#### 带 4 个参数的总耗时（毫秒）

|                         | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|-------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)    | 110    | 125    | 188    | 256    | 318    | 374    | 449    | 511    | 583    | 642     |
| BqLog Text (C++)        | 344    | 699    | 1036   | 1401   | 1889   | 2211   | 2701   | 3121   | 3393   | 3561    |
| BqLog Compress (Java)   | 129    | 141    | 215    | 292    | 359    | 421    | 507    | 568    | 640    | 702     |
| BqLog Text (Java)       | 351    | 702    | 1052   | 1399   | 1942   | 2301   | 2754   | 3229   | 3506   | 3695    |
| Log4j2 Text             | 1065   | 2583   | 4249   | 4843   | 5068   | 6195   | 6424   | 7943   | 8794   | 9254    |

<img src="docs/img/benchmark_4_params.png" alt="4个参数的结果" style="width: 100%;">

#### 不带参数的总耗时（毫秒）

一个有趣现象是，在「无参数」情况下，Log4j2 的耗时相对其「带参数」情况略低，但仍明显慢于 BqLog。

|                         | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|-------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)    | 97     | 101    | 155    | 228    | 290    | 341    | 415    | 476    | 541    | 601     |
| BqLog Text (C++)        | 153    | 351    | 468    | 699    | 916    | 1098   | 1212   | 1498   | 1733   | 1908    |
| BqLog Compress (Java)   | 109    | 111    | 178    | 240    | 321    | 378    | 449    | 525    | 592    | 670     |
| BqLog Text (Java)       | 167    | 354    | 491    | 718    | 951    | 1139   | 1278   | 1550   | 1802   | 1985    |
| Log4j2 Text             | 3204   | 6489   | 7702   | 8485   | 9640   | 10458  | 11483  | 12853  | 13995  | 14633   |

<img src="docs/img/benchmark_no_param.png" alt="不带参数的结果" style="width: 100%;">

---

## 如何贡献代码

若您希望贡献代码，请确保您的改动能通过仓库中 GitHub Actions 下的以下工作流：

- `AutoTest`
- `Build`

建议在提交前本地运行对应脚本，确保测试与构建均正常通过。
