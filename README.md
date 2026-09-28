<p align="center">
  <img src="banner.jpg" alt="BqLog Banner" width="100%">
</p>

# BqLog (BianQue Log) V 2.5.0

**English** | [简体中文](./README_CHS.md)

[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.5.0-red.svg)](https://github.com/Tencent/BqLog/releases)
[![ChangeLog](https://img.shields.io/badge/📋_ChangeLog-v2.5.0-orange.svg?style=flat)](CHANGELOG.md)
[![GitHub Stars](https://img.shields.io/github/stars/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/network/members)
[![GitHub Issues](https://img.shields.io/github/issues/Tencent/BqLog?style=flat&logo=github)](https://github.com/Tencent/BqLog/issues)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20iOS%20%7C%20Android%20%7C%20HarmonyOS%20%7C%20OpenHarmony%20%7C%20Unix-lightgrey.svg?style=flat)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Java%20%7C%20C%23%20%7C%20Kotlin%20%7C%20TypeScript%20%7C%20Python%20%7C%20Go-blue.svg?style=flat)]()

> BqLog is a lightweight, high-performance, industrial-grade logging system that has been widely used in online projects such as "Honor of Kings".
> **BqLog 2.x is officially released!** With native `HarmonyOS NEXT`, `Python` and `Node.js` support, and asymmetric hybrid encryption.
>
> 🚀 In [Benchmarks](#-benchmark-results), BqLog's compressed log mode is **7–16x faster than fmtlog**, **7–18x faster than quill**, **6–65x faster than spdlog (async)**, **9–22x faster than Log4j2**, and **47–252x faster than glog**; plain text mode also outperforms all of them.

---

[![Download](https://img.shields.io/badge/⬇_Download-Release_2.5.0-blue.svg?style=for-the-badge)](https://github.com/Tencent/BqLog/releases)

## 📋 What's New in v2.5.0

- **Performance**: Improved log-writing performance and reduced unnecessary worker wakeups and Java argument allocations.
- **Go support**: Added a Go wrapper with automatic argument conversion, category support, and source-module distribution.

> Full changelog → [CHANGELOG.md](CHANGELOG.md)

---

## 💡 If you have the following pain points, try BqLog

- If your client product (especially games) wants to satisfy this "impossible triangle" at the same time:
  - Easy troubleshooting (log as much as possible)
  - Good performance (log as little as possible)
  - Save storage space (better not log at all)
- If you are a backend service developer and your current logging library cannot handle **high-concurrency scenarios**, causing log loss or application stalls.
- If your programming language is one of C++, Java, C#, Kotlin, TypeScript, JavaScript, Python, or you use multiple languages at the same time and want a **unified cross-language logging solution**.

---

## ✨ Highlights

- Significant performance advantage over common open-source logging libraries (see [Benchmark](#-benchmark-results)); suitable for server, client, and mobile.
- Low memory usage: in the Benchmark case (10 threads, 2,000,000 log entries each), BqLog uses only about 2-3 MB of memory even with three appenders active simultaneously. And in mobile platform scenarios, it's generally around 1 MB.
- Provides a high-performance, high-compression real-time compressed log format.
- Supports strong hybrid encryption (asymmetric + symmetric) for log content protection with nearly zero performance overhead (optional).
- Works well inside game engines (`Unity`, `Unreal`, etc.), with UE Blueprint and builtin data type support.
- Supports `utf8`, `utf16`, `utf32` characters and strings, as well as bool, float, double, and integer types of various sizes.
- Supports `C++20` `std::format` style format strings (without positional index and time formatting).
- Asynchronous logging supports crash recovery and tries to avoid data loss.
- On Java, C#, TypeScript wrappers, it can achieve "zero extra heap alloc" (or very close), avoiding continuous object allocations.
- Depends only on the standard C library and platform APIs; can be compiled with Android `ANDROID_STL = none`.
- Supports `C++11` and later standards and works under very strict compiler options.
- Build system is based on `CMake` and provides multi-platform scripts, easy to integrate.
- Supports custom parameter types.
- Very friendly for code completion and IDE hints.

---

## 🖥️ Supported platforms & languages

| Platforms | Languages |
|-----------|-----------|
| Windows 64-bit, macOS, Linux (incl. embedded), iOS, Android, HarmonyOS, OpenHarmony, Unix (FreeBSD, NetBSD, OpenBSD, Solaris, etc.) | C++ (C++11+), Java / Kotlin, C# (Unity, .NET), ArkTS / C++ (HarmonyOS & OpenHarmony share the same package), JavaScript / TypeScript (Node.js), Python 3.7+, Go, Unreal Engine (UE4, UE5 & UE6 development builds) |

**Hardware architectures**: x86, x86_64, ARM32, ARM64
**Integration methods**: Dynamic library, Static library, Source code

---

## 📖 Why is BqLog so fast? — a three-part series

BqLog's speed is not magic. This series derives the whole design step by step, with diagrams:

1. **[Part 1: High-Performance Realtime Compressed Log Format](<docs/Article 1_Why is BqLog so fast - High Performance Realtime Compressed Log Format.MD>)** — from one line of text to a compressed format: templates, VLQ, UTF-Mixed, segments and encryption
2. **[Part 2: From Ring Buffer to Adaptive Data Bus](<docs/Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>)** — from kFifo and LMAX Disruptor to BqLog's adaptive bus: fetch_add + rollback, MISO/SISO, lock-free thread management and crash recovery
3. **[Part 3: Optimizing the Compressed Log Pipeline](<docs/Article 3_Why is BqLog so fast - Optimizing the Compressed Log Pipeline.MD>)** — how much work can share one memory load: fused copy+hash, template caches, length backfill, batched I/O

All diagrams and the series index live in [Performance Design](docs/PERFORMANCE_DESIGN.md).

---

## 🏗️ Architecture

![Structure](docs/img/log_structure.png)

Your program accesses the core engine through `BqLog Wrapper` (C++, Java, C#, TypeScript, Python, etc.). Each Log object can mount one or more Appenders (Console / Text File / Compressed File). **Within the same process, Wrappers of different languages can access the same Log object.**

| Appender | Output | Readable | Performance | Size | Encryption |
|----------|--------|----------|-------------|------|------------|
| ConsoleAppender | Console | Yes | Low | - | No |
| TextFileAppender | File | Yes | Low | Large | No |
| CompressedFileAppender | File | No | High | Small | Yes |

---

## 🚀 Quick Start

> Before calling any API, you need to integrate BqLog into your project first:
> - **Standard environments** (C++, Java, C#, Python, Node.js, etc.) → [Integration Guide](docs/INTEGRATION_GUIDE.md)
> - **Game engines** (Unity, Tuanjie Engine, Unreal Engine) → [Game Engine Integration Guide](docs/ENGINE_INTEGRATION.md)
> - **Anything else** → [FAQ](docs/FAQ.md)

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

### TypeScript (HarmonyOS / OpenHarmony ArkTS)

> The same `bqlog` package on ohpm works for both **HarmonyOS** (NEXT) and **OpenHarmony** (4.1+ / API 11+). Same install, same import, same code.

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

> For full integration steps for all platforms, see [Integration Guide](docs/INTEGRATION_GUIDE.md) and [Game Engine Integration Guide](docs/ENGINE_INTEGRATION.md).

---

## 📊 Benchmark results

Test: 1–10 threads, each writing 2,000,000 log entries. Environment: PC, AMD Ryzen 9 9950X (16 cores / 32 threads), 96 GB, Windows 11 Pro; Java tests on JBR 21.0.9.

Comparison: BqLog (Text / Compress / Compress+Encrypt) vs spdlog 1.17.0 (async), glog 0.7.1, fmtlog, quill 11.1.0, Log4j2 2.23.1.

The BqLog figures below measure the C++ API. Other language wrappers generally
add runtime and cross-language call overhead; their performance depends on the
language and workload and should be measured separately.

#### Throughput — Total Time Cost with 4 parameters (ms)

|                              | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|------------------------------|----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|------------|
| BqLog Compress (C++)         | 95       | 144       | 210       | 210       | 226       | 267       | 362       | 395       | 439       | 507        |
| BqLog Compress+Encrypt (C++) | 102      | 166       | 167       | 190       | 236       | 308       | 350       | 391       | 453       | 493        |
| BqLog Text (C++)             | 258      | 513       | 777       | 1054      | 1324      | 1587      | 1891      | 2143      | 2465      | 2811       |
| fmtlog                       | 672      | 1219      | 1766      | 2428      | 3024      | 3923      | 4612      | 5935      | 6293      | 7934       |
| quill                        | 639      | 1429      | 2232      | 3082      | 3915      | 4726      | 5609      | 6246      | 6957      | 7812       |
| Log4j2 (Java)                | 873      | 1484      | 2087      | 2727      | 3738      | 4541      | 4889      | 6127      | 9475      | 7192       |
| spdlog (async)               | 560      | 1649      | 3402      | 5737      | 9069      | 13827     | 21494     | 24518     | 28463     | 32939      |
| glog                         | 4485     | 8548      | 14875     | 21387     | 28295     | 36060     | 45742     | 62368     | 102370    | 127550     |

#### Peak Memory Usage (MB)

|                                          | 1 Thread | 4 Threads | 10 Threads |
|------------------------------------------|----------|-----------|------------|
| BqLog (all 3 appenders in one process)   | 12.7     | 13.3      | 14.7       |
| spdlog (async)                           | 14.4     | 14.4      | 14.7       |
| glog                                     | 11.9     | 12.1      | 12.4       |
| fmtlog                                   | 17.1     | 20.2      | 23.3       |
| quill                                    | 282.1    | 1062.7    | 2714.9     |
| Log4j2 (Java)                            | 1537.8   | 6884.3    | 4631.5     |

#### Output File Size (1 thread, 4M log entries)

| Library | Format | Size | Compression Ratio |
|---------|--------|------|-------------------|
| BqLog Compress | Binary | 45 MB | **6.3x** smaller |
| BqLog Compress+Encrypt | Encrypted | 45 MB | **6.3x** smaller |
| BqLog Text | Text | 283 MB | — |
| spdlog (async) | Text | 285 MB | — |
| quill | Text | 247 MB | — |
| fmtlog | Text | 270 MB | — |
| glog | Text | 314 MB | — |
| Log4j2 | Text | 410 MB (2M entries) | — |

- BqLog Compress: **7–16x faster** than fmtlog, **7–18x** than quill, **6–65x** than spdlog (async), **9–22x** than Log4j2, **47–252x** than glog
- Encryption adds **near-zero overhead**
- Compressed format is **6.3x smaller** than text
- BqLog uses only **12.7–14.7 MB** memory with all three appenders active

> For full benchmark code, methodology, and feature comparison, see [Benchmark](docs/BENCHMARK.md).

---

## 🔄 Changes from 1.x to 2.x

1. Added HarmonyOS support, including ArkTS and C++.
2. Added Node.js support (CJS and ESM).
3. Improved cross-platform compatibility, stability and generality; supports more Unix systems.
4. Average performance improved by ~80% for UTF-8, and by >500% for UTF-16 environments (C#, Unreal, Unity).
5. Android no longer must be used together with Java.
6. Removed the `is_in_sandbox` config and replaced it with `base_dir_type`; added filters for snapshots and support for opening a new log file on each startup. See [Configuration](docs/CONFIGURATION.md).
7. Added high-performance hybrid asymmetric encryption, ***almost zero overhead***; see [Advanced Usage — Encryption](docs/ADVANCED_USAGE.md#6-log-encryption-and-decryption).
8. Provides Unity, Tuanjie Engine, and Unreal Engine plugins, making it easy to use in game engines; the Unreal plugin supports UE4, UE5, and current UE6 development builds, with ConsoleAppender redirection and Blueprint support. UE4/UE5 packages are also published on [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf), while UE6 packages are available from GitHub Releases only. See [Game Engine Integration Guide](docs/ENGINE_INTEGRATION.md).
9. The repository no longer ships binaries. From 2.x on, please download platform- and language-specific packages from the [Releases page](https://github.com/Tencent/BqLog/releases).
10. The size of a single log entry is not limited by `log.buffer_size` anymore;
11. The timezone can be specified manually.
12. The `raw_file` appender is deprecated and no longer maintained in 2.x; please use the `compressed_file` appender instead.
13. The Recovery feature's reliability has been improved and it has been promoted from experimental (beta) to stable (release). see [Advanced Usage — Data Protection](docs/ADVANCED_USAGE.md#3-data-protection-on-abnormal-exit).

---

## 📑 Documentation

| Document | Description |
|----------|-------------|
| [FAQ](docs/FAQ.md) | Common questions: comparisons, platforms, distribution channels, crash recovery |
| [Integration Guide](docs/INTEGRATION_GUIDE.md) | Full integration steps for all platforms + all language demos |
| [Game Engine Integration](docs/ENGINE_INTEGRATION.md) | Unity, Tuanjie Engine, Unreal Engine plugins and Blueprint usage |
| [API Reference](docs/API_REFERENCE.md) | Core APIs, sync/async logging, Appender overview, build & tools |
| [Configuration](docs/CONFIGURATION.md) | Full configuration reference (appenders, log, snapshot) |
| [Advanced Usage](docs/ADVANCED_USAGE.md) | No Heap Alloc, Category, crash recovery, custom types, encryption |
| [Benchmark](docs/BENCHMARK.md) | Benchmark code, methodology, and results |
| [Performance Design](docs/PERFORMANCE_DESIGN.md) | Compressed file format, adaptive data bus, and write-path optimizations |

---

## 🤝 How to contribute

If you want to contribute code, please make sure your changes can pass the following workflows under GitHub Actions in the repository:

- `AutoTest`
- `Build`

It is recommended to run corresponding scripts locally before submitting to ensure both testing and building pass normally.
