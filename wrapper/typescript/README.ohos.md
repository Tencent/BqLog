# BqLog

> **工业级高性能日志引擎** -- 基于原生 C++ 无锁环形缓冲区核心，经过腾讯大规模游戏引擎和APP的生产验证，现已支持 HarmonyOS (ArkTS)。

[![license](https://img.shields.io/badge/license-Apache--2.0-blue)](https://www.apache.org/licenses/LICENSE-2.0)

项目主页：[github.com/Tencent/BqLog](https://github.com/Tencent/BqLog)


---
## 💡 如果您有以下困扰，可以尝试 BqLog

- 如果您的客户端产品（尤其是游戏）希望同时满足以下「不可能三角」：
    - 方便追溯问题（日志应写尽写）
    - 性能足够好（日志要少写）
    - 节约存储空间（日志最好就别写）
- 如果您是后台服务开发者，现有日志库在**高并发场景**下性能不足，导致日志丢失或程序阻塞。
- 如果您的编程语言是 C++、Java、C#、Kotlin、TypeScript、JavaScript、Python 之一，或者同时使用多种语言，希望有一套**统一的跨语言日志解决方案**。

---

## 特性

- **极致性能** -- 工业级无锁队列，对UTF16环境极致优化
- **工业级品质** -- 腾讯生产环境上亿用户验证，适用于游戏引擎（Unity、Unreal）、移动应用及高并发服务端
- **压缩加密 Appender** -- `compressed_file` 生成最小体积的日志文件，几乎零开销，支持 RSA + AES 混合非对称加密
- **同步与异步模式** -- 按需选择线程模型，平衡延迟与吞吐
- **分类日志** -- 为日志条目附加分类标签，支持分类掩码过滤
- **崩溃恢复** -- 内存映射缓冲区可在进程崩溃后存活，未刷写的日志在重启时自动恢复
- **跨平台** -- 支持 Windows、macOS、Linux、Android、iOS、HarmonyOS 等多平台

---

## 安装

```
ohpm install bqlog
```

---

## 快速开始

```typescript
import { bq } from "bqlog";

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("my_log", config);

log.info("Hello BqLog! int:{}, float:{}", 123, 3.14);
bq.log.force_flush_all_logs();
```

---

## Appender 类型

| 类型 | 说明 |
|------|------|
| `console` | 输出到 stdout/stderr |
| `text_file` | 纯文本日志文件，可直接阅读 |
| `compressed_file` | **二进制压缩 -- 体积最小、写入最快，可选 RSA+AES 加密。** 使用 BqLog 解码工具解码 |

---

## 压缩加密 Appender

`compressed_file` 是 **生产环境推荐选择**：

- **最小输出** -- 专有二进制格式，文件体积远小于纯文本或 gzip
- **最快写入** -- 压缩集成在写入路径中，几乎无额外开销
- **混合加密** -- 可选 RSA + AES 加密保护静态日志内容，加密几乎不影响性能

```typescript
const config = `
    appenders_config.SecureFile.type=compressed_file
    appenders_config.SecureFile.time_zone=localtime
    appenders_config.SecureFile.levels=[all]
    appenders_config.SecureFile.file_name=logs/secure
    appenders_config.SecureFile.max_file_size=100000000
    appenders_config.SecureFile.expire_time_days=30

    # 可选：启用加密（提供 RSA 公钥）
    # appenders_config.SecureFile.pub_key=your_rsa_public_key_here

    log.thread_mode=async
`;
const log = bq.log.create_log("secure_log", config);
log.info("此日志已压缩，可选加密");
```

---

## 配置示例

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

## 分类日志

使用 [BqLog Category Generator](https://github.com/Tencent/BqLog/tree/main/tools/category_log_generator) 工具生成类型安全的分类日志封装：

```typescript
import { my_category_log } from "./my_category_log";

const log = my_category_log.create_log("cat_log", config);
log.info(log.cat.ModuleA.SystemA, "分类消息: {}", value);
```

---

## 日志级别

| 级别 | 用途 |
|------|------|
| `verbose` | 细粒度追踪 |
| `debug` | 调试信息 |
| `info` | 常规运行消息 |
| `warning` | 潜在问题 |
| `error` | 错误状况 |
| `fatal` | 严重故障 |

---

## 基准测试

BqLog 的性能持续领先于主流日志库。详细基准测试结果请参阅主仓库：[完整测试数据](https://github.com/Tencent/BqLog#benchmark)。

---

## 完整文档

支持所有语言的完整文档和示例：

**C++ | Java | C# | Python | TypeScript | Go | Unreal Engine | Unity | HarmonyOS**

👉 **[github.com/Tencent/BqLog](https://github.com/Tencent/BqLog)**

---

## 许可证

[Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) -- 可免费商用。
