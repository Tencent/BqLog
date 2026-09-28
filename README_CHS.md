<p align="center">
  <img src="banner.jpg" alt="BqLog Banner" width="100%">
</p>

# BqLog (扁鹊日志) V 2.5.0

[English](./README.md) | **简体中文**

[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.5.0-red.svg)](https://github.com/Tencent/BqLog/releases)
[![ChangeLog](https://img.shields.io/badge/📋_更新日志-v2.5.0-orange.svg?style=flat)](CHANGELOG_CHS.md)
[![GitHub Stars](https://img.shields.io/github/stars/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/network/members)
[![GitHub Issues](https://img.shields.io/github/issues/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/issues)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20iOS%20%7C%20Android%20%7C%20HarmonyOS%20%7C%20OpenHarmony%20%7C%20Unix-lightgrey.svg?style=flat)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Java%20%7C%20C%23%20%7C%20Kotlin%20%7C%20TypeScript%20%7C%20Python%20%7C%20Go-blue.svg?style=flat)]()

> BqLog 是一个轻量级、高性能的工业级日志系统，已在线上广泛应用于《王者荣耀》等项目。
> **BqLog 2.x 正式发布！** 新增`纯血鸿蒙`、`Python` 与 `Node.js` 支持，并带来非对称混合加密能力。
>
> 🚀 在 [Benchmark](#-benchmark-结果) 中，BqLog 压缩日志模式比 fmtlog 快 **7-16 倍**、quill 快 **7-18 倍**、spdlog（异步）快 **6-65 倍**、Log4j2 快 **9-22 倍**、glog 快 **47-252 倍**；纯文本模式同样全面领先。

---

[![Download](https://img.shields.io/badge/⬇_下载-Release_2.5.0-blue.svg?style=for-the-badge)](https://github.com/Tencent/BqLog/releases)

## 📋 v2.5.0 更新亮点

- **性能提升**：优化日志写入，减少无效的工作线程唤醒和 Java 参数分配。
- **Go 语言支持**：新增 Go wrapper，支持自动参数转换、category 和源码模块分发。

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
- 内存消耗少：在 Benchmark 用例中（10 线程、每线程 200 万条日志），即使同时挂载三种 appender，BqLog 内存消耗也仅约 2-3 MB。移动平台场景一般在1 MB左右。
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

测试：1-10 线程，每线程写 2,000,000 条日志。环境：PC，AMD Ryzen 9 9950X（16 核 / 32 线程），96 GB，Windows 11 专业版；Java 测试使用 JBR 21.0.9。

对比：BqLog（Text / Compress / Compress+Encrypt）vs spdlog 1.17.0（异步）、glog 0.7.1、fmtlog、quill 11.1.0、Log4j2 2.23.1。

下表 BqLog 数据来自 C++ 接口。其他语言的 wrapper 通常会增加运行时和
跨语言调用开销，性能随语言和负载而变化，请以对应语言的实测结果为准。

#### 吞吐量 — 带 4 个参数的总耗时（毫秒）

|                              | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|------------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)         | 95     | 144    | 210    | 210    | 226    | 267    | 362    | 395    | 439    | 507     |
| BqLog Compress+Encrypt (C++) | 102    | 166    | 167    | 190    | 236    | 308    | 350    | 391    | 453    | 493     |
| BqLog Text (C++)             | 258    | 513    | 777    | 1054   | 1324   | 1587   | 1891   | 2143   | 2465   | 2811    |
| fmtlog                       | 672    | 1219   | 1766   | 2428   | 3024   | 3923   | 4612   | 5935   | 6293   | 7934    |
| quill                        | 639    | 1429   | 2232   | 3082   | 3915   | 4726   | 5609   | 6246   | 6957   | 7812    |
| Log4j2 (Java)                | 873    | 1484   | 2087   | 2727   | 3738   | 4541   | 4889   | 6127   | 9475   | 7192    |
| spdlog（异步）               | 560    | 1649   | 3402   | 5737   | 9069   | 13827  | 21494  | 24518  | 28463  | 32939   |
| glog                         | 4485   | 8548   | 14875  | 21387  | 28295  | 36060  | 45742  | 62368  | 102370 | 127550  |

#### 峰值内存占用（MB）

|                                          | 1 线程 | 4 线程 | 10 线程 |
|------------------------------------------|--------|--------|---------|
| BqLog（单进程内同时挂载 3 种 appender）  | 12.7   | 13.3   | 14.7    |
| spdlog（异步）                           | 14.4   | 14.4   | 14.7    |
| glog                                     | 11.9   | 12.1   | 12.4    |
| fmtlog                                   | 17.1   | 20.2   | 23.3    |
| quill                                    | 282.1  | 1062.7 | 2714.9  |
| Log4j2 (Java)                            | 1537.8 | 6884.3 | 4631.5  |

#### 日志文件大小（1 线程，400 万条日志）

| 库 | 格式 | 大小 | 压缩比 |
|----|------|------|--------|
| BqLog Compress | 二进制 | 45 MB | 比文本 **小 6.3 倍** |
| BqLog Compress+Encrypt | 加密 | 45 MB | 比文本 **小 6.3 倍** |
| BqLog Text | 文本 | 283 MB | — |
| spdlog（异步） | 文本 | 285 MB | — |
| quill | 文本 | 247 MB | — |
| fmtlog | 文本 | 270 MB | — |
| glog | 文本 | 314 MB | — |
| Log4j2 | 文本 | 410 MB（200 万条） | — |

- BqLog Compress 比 fmtlog 快 **7-16 倍**，比 quill 快 **7-18 倍**，比 spdlog（异步）快 **6-65 倍**，比 Log4j2 快 **9-22 倍**，比 glog 快 **47-252 倍**
- 加密几乎 **零额外开销**
- 压缩格式比文本 **小 6.3 倍**
- BqLog 同时挂载三种 appender 时也仅使用 **12.7-14.7 MB** 内存

> 完整 Benchmark 代码、方法论和功能对比请见 [Benchmark](docs/BENCHMARK_CHS.md)。

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
12. `raw_file`类型的appender不再维护，标记为`废弃`，请用`compressed_file`类型替代。
13. 复盘能力增加可靠性，从实验性功能变成正式能力。见[高级用法 — 数据保护](docs/ADVANCED_USAGE_CHS.md#3-程序异常退出的数据保护)。

---

## 📑 文档导航

| 文档 | 说明 |
|------|------|
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
