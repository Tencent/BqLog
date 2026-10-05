<p align="center">
  <img src="banner.jpg" alt="BqLog Banner" width="100%">
</p>

# BqLog (扁鹊日志) V 2.6.0

[English](./README.md) | **简体中文**

[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.6.0-red.svg)](https://github.com/Tencent/BqLog/releases)
[![ChangeLog](https://img.shields.io/badge/📋_更新日志-v2.6.0-orange.svg?style=flat)](CHANGELOG_CHS.md)
[![GitHub Stars](https://img.shields.io/github/stars/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/network/members)
[![GitHub Issues](https://img.shields.io/github/issues/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/issues)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20iOS%20%7C%20Android%20%7C%20HarmonyOS%20%7C%20OpenHarmony%20%7C%20Unix-lightgrey.svg?style=flat)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Java%20%7C%20C%23%20%7C%20Kotlin%20%7C%20TypeScript%20%7C%20Python%20%7C%20Go-blue.svg?style=flat)]()

> BqLog 是一个轻量级、高性能的工业级日志系统，已在线上广泛应用于《王者荣耀》等项目。

---

## 🎯 BqLog 的定位

**极限性能是 BqLog 的起点，而不是全部。** 它是一个成熟的工业级组件，为要发布上线的产品而生：

- **管道两端都要快。** 一台现代设备上，既有写日志的业务线程，也有把日志落成文件的消费线程。把一边做快、把活儿推给另一边，只是跷跷板，不是优化。BqLog 两手都抓、两手都硬：日志线程的开销和消费线程的开销一起压低，并且按真实使用场景来做基准测试，而不是只测一个热循环。
- **跑在哪里都兼容。** Windows、macOS、Linux、iOS、Android、HarmonyOS、OpenHarmony 和各类 Unix；C++、Java/Kotlin、C#、Python、TypeScript/ArkTS、Go 共用同一个内核，甚至可以共用同一个日志对象；游戏引擎、不带 STL 的移动端工具链、开着 `-Werror` 的严格构建都没问题。
- **为出事的那一天准备好。** 进程崩溃后能把崩溃前写下的日志恢复回来；磁盘写满时既不崩溃也不卡死；损坏的文件会被跳过而不是被信任；消费者跟不上时有明确的处理策略。
- **好用，而且不容易用错。** 一行创建日志、一行写日志；都是普通函数，IDE 能补全也能做类型检查；旧版本的头文件可以直接搭配新版本的库，日志文件格式跨版本可读。

---

[![Download](https://img.shields.io/badge/⬇_下载-Release_2.6.0-blue.svg?style=for-the-badge)](https://github.com/Tencent/BqLog/releases)

## 📋 v2.6.0 更新亮点

- **C++ 专属的快速模式**：`BQ_LOG_FAST_INFO(log, ...)` 等宏，日志线程上的开销约为普通模式的一半，持续写日志的线程上每条只要几纳秒，见[快速开始](#c)和 [API 参考：快速模式](docs/API_REFERENCE_CHS.md#3-写日志)。
- **普通模式更快**：`log.info` 在日志线程上执行的指令减少约 16%。和 2.5.0 比整条流水线（macOS，同一个 benchmark），文本输出快 14%～30%，压缩输出最多快约 40%。
- **更快的时钟**：在可靠的平台上，时间戳直接取自 CPU 硬件计数器，不可靠时自动退回系统时钟。

> 完整更新日志 → [CHANGELOG_CHS.md](CHANGELOG_CHS.md)

---

## 💡 如果您有以下困扰，可以尝试 BqLog

- 如果您的客户端产品（尤其是游戏）希望同时满足以下「不可能三角」：
  - 方便追溯问题（日志应写尽写）
  - 性能足够好（日志要少写）
  - 节约存储空间（日志最好就别写）
- 如果您是后台服务开发者，现有日志库在**高并发场景**下性能不足，导致日志丢失或程序阻塞。
- 如果您的编程语言是 C++、Java、C#、Kotlin、TypeScript、JavaScript、Python 之一，或者同时使用多种语言，希望有一套**统一的跨语言日志解决方案**。


---

## ✨ 特点

- 相比常见开源日志库有显著性能优势（详见 [Benchmark](#-benchmark-结果)），不仅适用于服务器和客户端，也非常适合移动端设备。
- 内存消耗少：在 Benchmark 用例中（10 线程、每线程 200 万条日志），即使同时挂着 6 个 logger，macOS 上整个 benchmark 进程的峰值也只有 8.5 MB。移动平台上，BqLog 自身一般在 1 MB 左右。
- 提供高性能、高压缩比的实时压缩日志格式。
- 以接近于0的性能损耗，提供高强度的非对称混合加密日志，保护日志内容安全（可选）。
- 可在游戏引擎（`Unity`、`Unreal` 等）中正常使用，对 Unreal 提供蓝图和常用类型的支持。
- 支持 `utf8`、`utf16`、`utf32` 字符及字符串，支持 bool、float、double、各种长度与类型的整数等常用参数类型。
- 支持 `C++20` 的 `std::format` 规范（不含排序序号与时间格式化）。
- 异步日志支持 Crash 复盘机制，尽量避免日志数据丢失。
- 在 Java、C#、TypeScript 上可以做到「零额外 Heap Alloc」（或极少），不会随着运行不断 new 对象。
- 仅依赖标准 C 语言库与平台 API，可在 Android 的 `ANDROID_STL = none` 模式下编译通过。
- 支持 `C++11` 及之后的标准，可在极其严格的编译选项下工作。
- 编译系统基于 `CMake`，并提供多平台编译脚本，集成简单。
- 支持自定义参数类型。
- 对代码提示非常友好。

---

## 🖥️ 支持的平台和语言

| 平台 | 语言 |
|------|------|
| Windows 64-bit、macOS、Linux（含嵌入式）、iOS、Android、HarmonyOS、OpenHarmony、Unix（FreeBSD、NetBSD、OpenBSD、Solaris 等） | C++（C++11+）、Java / Kotlin、C#（Unity、.NET）、ArkTS / C++（HarmonyOS 与 OpenHarmony 同一份包）、JavaScript / TypeScript（Node.js）、Python 3.7+、Go、Unreal Engine（UE4、UE5 与 UE6 开发版） |

**硬件架构**：x86、x86_64、ARM32、ARM64
**引入方式**：动态库、静态库、源代码

---

## 📖 为何 BqLog 如此快？—— 三部曲系列文章

BqLog 的快不是变魔术变出来的。这个系列从一行日志出发，配示意图，把整套设计一步步推导出来：

1. **[之一：高性能实时压缩日志格式](<docs/文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>)** —— 从一行文本推导出压缩格式：模板、VLQ、UTF-Mixed、分段与加密
2. **[之二：从环形队列到自适应数据总线](<docs/文章2_为何BqLog如此快 - 从环形队列到自适应数据总线.MD>)** —— 从 kFifo、LMAX Disruptor 到 BqLog 自适应总线：fetch_add + 回滚、MISO/SISO、无锁增删线程与崩溃复盘
3. **[之三：压缩日志执行路径优化](<docs/文章3_为何BqLog如此快 - 压缩日志执行路径优化.MD>)** —— 一次内存读取能顺便做多少事：融合拷贝哈希、模板缓存、长度回填、批量 I/O

全部插图和系列目录见 [性能设计](docs/PERFORMANCE_DESIGN_CHS.md)。

---

## 🏗️ 架构介绍

![基础结构](docs/img/log_structure.png)

您的程序通过 BqLog 提供的 `BqLog Wrapper`（C++、Java、C#、TypeScript、Python 等）来访问核心引擎。每个 Log 对象可挂载一个或多个 Appender（控制台 / 文本文件 / 压缩文件）。**同一进程内，不同语言的 Wrapper 可以访问同一个 Log 对象。**

| Appender | 输出目标 | 可读 | 性能 | 尺寸 | 加密 |
|----------|---------|------|------|------|------|
| ConsoleAppender | 控制台 | Yes | 低 | - | No |
| TextFileAppender | 文件 | Yes | 低 | 大 | No |
| CompressedFileAppender | 文件 | No | 高 | 小 | Yes |

---

## 🚀 快速上手

> 调用 API 前，请先将 BqLog 集成到您的项目：
> - **常规编程环境**（C++、Java、C#、Python、Node.js 等）→ [集成指南](docs/INTEGRATION_GUIDE_CHS.md)
> - **游戏引擎**（Unity、团结引擎、Unreal Engine）→ [游戏引擎集成指南](docs/ENGINE_INTEGRATION_CHS.md)
> - **其他问题** → [常见问题（FAQ）](docs/FAQ_CHS.md)

### C++

```cpp
#include <string>
#include <bq_log/bq_log.h>

int main() {
    std::string config = R"(
        appenders_config.appender_console.type=console
        appenders_config.appender_console.levels=[all]
    )";
    auto log = bq::log::create_log("main_log", config);
    log.info("Hello BqLog 2.0! int:{}, float:{}", 123, 3.14f);
    log.force_flush();
    return 0;
}
```

**C++ 专属的快速模式。** 在 C++ 最热的代码路径上，可以用 `BQ_LOG_FAST_*` 宏写同样的日志，调用线程的开销只有普通模式的一小部分：

```cpp
BQ_LOG_FAST_INFO(log, "Hello BqLog fast mode! int:{}, float:{}", 123, 3.14f);
```

输出和 `log.info` 完全一样。代价是：内存使用稍高一点；它是宏，IDE 代码提示体验比普通成员函数差一点；每个调用点会绑定第一次调用时的日志对象和格式串，所以同一个调用点必须固定使用同一个日志对象和同一个格式串。详见 [API 参考：快速模式](docs/API_REFERENCE_CHS.md#3-写日志)。

### Java

```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```

### C#

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```

### TypeScript (Node.js)

```typescript
import { bq } from "@pippocao/bqlog";
const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);
log.info("Hello from Node.js! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

### Python

```python
from bq.log import log
config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
"""
my_log = log.create_log("python_log", config)
my_log.info("Hello from Python! params: {}, {}", "text", 123)
log.force_flush_all_logs()
```

### Go

```go
package main

import bq "github.com/Tencent/BqLog/go/v2"

func main() {
    config := `
appenders_config.console.type=console
appenders_config.console.levels=[all]
`
    log := bq.Create_log("go_log", config, nil)
    if !log.Is_valid() {
        panic("invalid log config")
    }
    defer log.Force_flush()

    log.Info("Hello from Go! params: {}, {}", "text", 123)
}
```

### TypeScript (鸿蒙 / OpenHarmony ArkTS)

> 同一个 ohpm 上的 `bqlog` 包同时适用于 **HarmonyOS**（NEXT）和 **OpenHarmony**（4.1+ / API 11+）。安装、引用、API 完全一致。

```typescript
import { bq } from "bqlog";
const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("ohos_log", config);
log.info("Hello from HarmonyOS / OpenHarmony! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

> 各平台完整集成步骤请见 [集成指南](docs/INTEGRATION_GUIDE_CHS.md) 和 [游戏引擎集成指南](docs/ENGINE_INTEGRATION_CHS.md)。

---

## 📊 Benchmark 结果

Benchmark 同时测管道的两端：所有日志落盘的总消耗，以及日志线程自身的开销（连同它消耗的消费端 CPU 和内存），并且都针对真实使用场景。完整的测法、Windows 数据和日志线程部分见 [Benchmark](docs/BENCHMARK_CHS.md)。

**总消耗，4 个参数，macOS（Apple M4 Pro），毫秒，越小越好。** 1～10 线程，每线程 2,000,000 条，计时到全部落盘：

| | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog 压缩，快速模式（C++） | 36 | 70 | 109 | 145 | 182 | 238 | 256 | 313 | 347 | 411 |
| BqLog 压缩+加密，快速模式（C++） | 46 | 78 | 123 | 158 | 193 | 248 | 270 | 311 | 358 | 456 |
| BqLog 文本，快速模式（C++） | 178 | 334 | 480 | 623 | 805 | 1003 | 1098 | 1280 | 1471 | 1865 |
| BqLog 压缩，普通模式（C++） | 53 | 79 | 113 | 148 | 209 | 244 | 272 | 313 | 357 | 438 |
| BqLog 压缩+加密，普通模式（C++） | 45 | 84 | 120 | 159 | 209 | 246 | 285 | 326 | 374 | 457 |
| BqLog 文本，普通模式（C++） | 169 | 304 | 440 | 603 | 785 | 1010 | 1077 | 1234 | 1428 | 1736 |
| fmtlog | 270 | 534 | 800 | 1065 | 1382 | 1602 | 1885 | 2091 | 2500 | 2899 |
| quill | 361 | 725 | 1112 | 1459 | 1811 | 2221 | 2597 | 3004 | 3323 | 3953 |
| Log4j2（Java） | 740 | 1024 | 1420 | 1750 | 2313 | 2483 | 3005 | 3469 | 3785 | 3808 |
| spdlog（异步） | 573 | 1681 | 3520 | 6904 | 13843 | 25105 | 35947 | 42858 | 49091 | 56292 |
| glog | 2413 | 4135 | 6635 | 9958 | 13673 | 21834 | 28843 | 34818 | 40268 | 44857 |

- BqLog 压缩：**比 fmtlog 快 5～7 倍**，**比 quill 快 7～10 倍**，**比 Log4j2 快 9～14 倍**，比 spdlog（异步）和 glog 快一到两个数量级。
- BqLog 文本仍然**比 fmtlog 快 1.6～1.8 倍**，fmtlog 是这里最快的文本日志库。
- 快速模式和普通模式的总吞吐基本相同：快速模式在日志线程上省下的成本，不会在消费端还回去。
- 加密几乎不要钱；压缩格式比 BqLog 自己的文本输出小约 **6.7 倍**。
- 10 线程时的峰值内存：BqLog 一个进程挂 6 个 logger 只有 **8.5 MB**；quill 是 **1.4 GB**。

---

## 🔄 从 1.x 版本升级到 2.x 版本的变化

1. 增加对鸿蒙系统的支持，包括 ArkTS 和 C++ 两种语言。
2. 增加对 Node.js 的支持（CJS 和 ESM）。
3. 增强跨平台兼容性、稳定性与通用性，支持更多 Unix 系统。
4. utf8编码下性能平均提升约 80%，utf16编码环境（C#，Unreal，Unity）提升超过500%。
5. Android 不再强制要求与 Java 一起使用。
6. 移除 `is_in_sandbox` 配置，改用 `base_dir_type`；对 snapshot 增加过滤配置，支持每次启动新开日志文件。详见 [配置说明](docs/CONFIGURATION_CHS.md)。
7. 支持高性能非对称混合加密，几乎无额外性能损耗，详见 [高级用法 — 加密](docs/ADVANCED_USAGE_CHS.md#6-日志加密和解密)。
8. 提供 Unity、团结引擎、Unreal 引擎插件，方便在游戏引擎中使用；Unreal 插件支持 UE4、UE5 与当前 UE6 开发版，并提供 ConsoleAppender 日志重定向和蓝图支持。UE4/UE5 插件同时发布于 [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf)，UE6 插件目前仅通过 GitHub Releases 提供。详见 [游戏引擎集成指南](docs/ENGINE_INTEGRATION_CHS.md)。
9. 仓库不再包含二进制产物，从 2.x 版本起请从 [Releases 页面](https://github.com/Tencent/BqLog/releases)下载对应平台和语言的二进制包。
10. 单条日志长度不再受log.buffer_size限制。
11. 可以精确手动设置时区。
12. `raw_file`类型的appender已删除。配置为`type=raw_file`的appender会打印警告并被忽略，请用`compressed_file`类型替代。
13. 复盘能力增加可靠性，从实验性功能变成正式能力。见[高级用法 — 数据保护](docs/ADVANCED_USAGE_CHS.md#3-程序异常退出的数据保护)。

---

## 📑 文档导航

| 文档 | 说明 |
|------|------|
| [常见问题（FAQ）](docs/FAQ_CHS.md) | 常见问题：对比、平台与语言、发布渠道、崩溃恢复 |
| [集成指南](docs/INTEGRATION_GUIDE_CHS.md) | 所有平台完整集成步骤 + 各语言 Demo |
| [游戏引擎集成](docs/ENGINE_INTEGRATION_CHS.md) | Unity、团结引擎、Unreal Engine 插件与蓝图使用 |
| [API 参考](docs/API_REFERENCE_CHS.md) | 核心 API、同步/异步日志、Appender 介绍、构建与工具 |
| [配置说明](docs/CONFIGURATION_CHS.md) | 完整配置参考（appenders、log、snapshot） |
| [高级用法](docs/ADVANCED_USAGE_CHS.md) | 无 Heap Alloc、Category、崩溃恢复、自定义类型、加密 |
| [Benchmark](docs/BENCHMARK_CHS.md) | Benchmark 代码、测试方法和结果 |
| [性能设计三篇](docs/PERFORMANCE_DESIGN_CHS.md) | 压缩文件格式、自适应数据总线与写入路径优化 |

---

## 🤝 如何贡献代码

若您希望贡献代码，请确保您的改动能通过仓库中 GitHub Actions 下的以下工作流：

- `AutoTest`
- `Build`

建议在提交前本地运行对应脚本，确保测试与构建均正常通过。
