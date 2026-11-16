# @pippocao/bqlog

> **The fastest industrial-grade logging engine** for Node.js — powered by a native C++ lock-free ring-buffer core, battle-tested at Tencent across large-scale game engines and backend services.

[![npm version](https://img.shields.io/npm/v/@pippocao/bqlog)](https://www.npmjs.com/package/@pippocao/bqlog)
[![license](https://img.shields.io/npm/l/@pippocao/bqlog)](https://www.apache.org/licenses/LICENSE-2.0)

Part of the [BqLog](https://github.com/Tencent/BqLog) project.

---

## 🚀 Why BqLog?

- ⚡ **Fastest in class** — lock-free MISO ring-buffer delivers ~80% higher throughput than comparable loggers for UTF-8, and >500% for UTF-16 environments
- 🏭 **Industrial grade** — production-proven at Tencent, designed for game engines (Unity, Unreal), mobile apps, and high-concurrency server workloads
- 🔐 **Compressed & Encrypted appender** — `compressed_file` produces the smallest log files with near-zero overhead, and supports **hybrid asymmetric encryption** (RSA + AES) for secure log storage at virtually no performance cost
- 🔄 **Sync & Async modes** — choose per-log thread model to balance latency vs throughput
- 🏷️ **Category logging** — attach categories to log entries, filter with category masks
- 🛡️ **Crash-safe recovery** — memory-mapped buffers survive process crashes; unflushed logs are recovered on restart
- 🌍 **Cross-platform native binaries** — prebuilt for Windows, macOS, Linux, FreeBSD, OpenBSD, NetBSD, DragonflyBSD, Solaris/OmniOS (x64 & ARM64)
- 📦 **ESM + CommonJS** — dual module support, works everywhere

---

## 📥 Installation

```bash
npm install @pippocao/bqlog
```

---

## ⚡ Quick Start

### ESM

```typescript
import { bq } from "@pippocao/bqlog";

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("my_log", config);

log.info("Hello BqLog! int:{}, float:{}", 123, 3.14);
bq.log.force_flush_all_logs();
```

### CommonJS

```javascript
const { bq } = require("@pippocao/bqlog");

const log = bq.log.create_log("my_log", `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`);
log.info("Hello from CJS!");
```

---

## 📋 Appender Types

| Type | Description |
|------|-------------|
| `console` | 🖥️ Output to stdout/stderr |
| `text_file` | 📄 Plain text log files, human-readable |
| `compressed_file` | 🔒 **Binary compressed — smallest size, fastest writes, optional RSA+AES encryption.** Decode with the BqLog decoder tool. |

---

## 🔐 Compressed & Encrypted Appender

The `compressed_file` appender is the **recommended choice for production**:

- 📉 **Smallest output** — proprietary binary format produces files significantly smaller than plain text or gzip
- ⚡ **Fastest writes** — compression is integrated into the write path with near-zero overhead
- 🔑 **Hybrid encryption** — optional RSA + AES encryption protects log content at rest; encryption adds virtually no performance penalty

```typescript
const config = `
    appenders_config.SecureFile.type=compressed_file
    appenders_config.SecureFile.time_zone=localtime
    appenders_config.SecureFile.levels=[all]
    appenders_config.SecureFile.file_name=logs/secure
    appenders_config.SecureFile.max_file_size=100000000
    appenders_config.SecureFile.expire_time_days=30

    # Optional: enable encryption (provide RSA public key)
    # appenders_config.SecureFile.pub_key=your_rsa_public_key_here

    log.thread_mode=async
`;
const log = bq.log.create_log("secure_log", config);
log.info("This log is compressed and optionally encrypted");
```

---

## ⚙️ Configuration Example

```typescript
const config = `
    appenders_config.ConsoleAppender.type=console
    appenders_config.ConsoleAppender.time_zone=localtime
    appenders_config.ConsoleAppender.levels=[all]

    appenders_config.FileAppender.type=compressed_file
    appenders_config.FileAppender.time_zone=localtime
    appenders_config.FileAppender.levels=[info,warning,error,fatal]
    appenders_config.FileAppender.file_name=logs/app
    appenders_config.FileAppender.max_file_size=100000000
    appenders_config.FileAppender.expire_time_days=7

    log.thread_mode=async
`;
const log = bq.log.create_log("app", config);
```

---

## 🏷️ Category Logging

Use the [BqLog Category Generator](https://github.com/Tencent/BqLog/tree/main/tools/category_log_generator) tool to generate type-safe category wrappers:

```typescript
import { my_category_log } from "./my_category_log";

const log = my_category_log.create_log("cat_log", config);
log.info(log.cat.ModuleA.SystemA, "categorized message: {}", value);
```

---

## 📊 Log Levels

| Level | Usage |
|-------|-------|
| `verbose` | Fine-grained tracing |
| `debug` | Debugging information |
| `info` | General operational messages |
| `warning` | Potential issues |
| `error` | Error conditions |
| `fatal` | Critical failures |

---

## 🌍 Supported Platforms

Prebuilt native binaries are included — **no compiler needed** at install time.

| OS | Architectures |
|----|--------------|
| 🪟 Windows | x64, ARM64 |
| 🍎 macOS | x64, ARM64 (Universal Binary) |
| 🐧 Linux | x64, ARM64, x86 (ia32) |
| 😈 FreeBSD | x64, ARM64 |
| 🐡 OpenBSD | x64, ARM64 |
| 🏁 NetBSD | x64, ARM64 |
| 🐉 DragonflyBSD | x64 |
| ☀️ Solaris / OmniOS (SunOS) | x64 |

### Debug symbols

Symbols ship beside the binary, not inside it:

| Platform | File | Location |
|----------|------|----------|
| macOS | `BqLog.node.dSYM` | `prebuilds/darwin-universal/` — one bundle covers the x64 and arm64 slices |
| Windows | `BqLog.pdb` | `prebuilds/win32-x64/`, `prebuilds/win32-arm64/` |

`prebuilds/darwin-universal/` holds symbols only and is never loaded at runtime; hand it to
`atos` or your crash pipeline to symbolicate macOS stacks.

---

## 📈 Benchmark

BqLog consistently outperforms popular logging libraries. See [full benchmark results](https://github.com/Tencent/BqLog#benchmark) in the main repository.

---

## 📚 Documentation

Full documentation and examples for all supported languages:

**C++ · Java · C# · Python · TypeScript · Unreal Engine · Unity · HarmonyOS**

👉 **[github.com/Tencent/BqLog](https://github.com/Tencent/BqLog)**

---

## 📄 License

[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) — free for commercial use.
