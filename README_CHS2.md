# BqLog (扁鹊日志) (V 2.0.1) ([ChangeLog](/CHANGELOG.md))
[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](https://github.com/Tencent/BqLog/blob/main/LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.0.1-red.svg)](https://github.com/Tencent/BqLog/releases)  

> BqLog是一个轻量级，高性能日志系统，应用于《Honor Of Kings》等项目，已经上线并良好运行。  
> **BqLog 2.0.1 版本正式发布！带来了纯血鸿蒙支持、Node.js支持、性能更强的并发模式以及高强度的混合加密方案。**

## 支持平台
- Windows 64 bit
- MacOS
- Linux (支持更多发行版及严格环境源码编译)
- iOS
- Android
- **HarmonyOS (包括HarmonyOS NEXT)**
- Unix (在FreeBSD, NetBSD, OpenBSD, Solaris, DragonFly, OmnisOS通过测试)

## 支持编程语言及环境
- C++(C++11及以上, MSVC, Clang, GCC, MinGW-GCC, MinGW-Clang, )
- Java / Kotlin (Android & Server)
- C# (Unity, .NET)
- **ArkTS / C++ (HarmonyOS)**
- **JavaScript / TypeScript (Node.js)**
- **Unreal Engine (UE4 & UE5)**

## 支持的硬件架构和环境
- X86
- X86_64
- ARMv7
- ARM64
- 手机，平板，各种嵌入式设备

## 特点
- **极致性能**：对比现有开源日志库有巨大的性能优势。2.0版本引入**高并发模式**，在高负载下自动启动高性能buffer（每线程约32K额外内存开销），整体性能较1.0版本提升40%。
- **极低内存**：内存消耗少，BqLog本身运行时内存消耗极低。
- **实时压缩**：提供高性能高压缩比的实时压缩日志格式。
- **安全加密**：新增**RSA2048 + AES256**混合加密方案，在仅增加10%性能损耗的前提下提供极高安全性，且不增加日志尺寸。
- **游戏引擎友好**：提供Unity和Unreal (源码及Prebuilt) 插件，支持将Console Appender的输出直接转接到游戏引擎的控制台。
- **多语言支持**：支持`utf8`, `utf16`, `utf32`的字符和字符串，支持常用基础类型及自定义类型。
- **标准兼容**：支持`C++20`的`format`规范。
- **数据保护**：异步日志支持Crash复盘（Recover），尽可能避免丢失数据。
- **极小体积**：Android编译后动态库仅有不到300k左右。
- **无Heap Alloc**：在Java,Javascript, Typescript和C#上可以不额外产生Heap Alloc，避免GC压力。

## 目录
**[1.X 升级到 2.0 迁移指南](#1x-升级到-20-迁移指南)**  
**[将BqLog引入您的项目](#将bqlog引入您的项目)**  
**[简单的Demo](#简单的demo)**  
**[架构介绍](#架构介绍)**  
**[主流程API使用说明](#主流程api使用说明)**  
**[配置说明 (bq_log.conf)](#配置说明)**  
**[Appender介绍](#appender介绍)**  
**[最佳实践 (Best Practices)](#最佳实践-best-practices)**  
**[高级使用话题](#高级使用话题)**  
**[离线解码工具](#离线解码二进制格式的appender)**  
**[构建说明](#构建说明)**  
**[Benchmark](#benchmark)**  

---

## 1.X 升级到 2.0 迁移指南
如果您是1.x版本的老用户，请注意以下不兼容变更：
1.  **`is_in_sandbox` 废弃**：TextFileAppender配置中的`is_in_sandbox`布尔选项已被移除，请使用新的`base_dir_type`整数选项代替。
    *   `base_dir_type=0`：对应原`is_in_sandbox=true`（沙盒/私有目录）。
    *   `base_dir_type=1`：对应原`is_in_sandbox=false`（外部/公共目录）。
2.  **`reliable_level` 废弃**：Log对象配置中的`reliable_level`已被移除，请使用新的`recover`选项。
    *   `log.recover=true`：开启崩溃恢复尝试。
    *   `log.recover=false`：关闭。
3.  **`RawFileAppender` 废弃**：我们不再推荐使用Raw格式，建议迁移至性能更强、体积更小的`CompressedFileAppender`。
4.  **Snapshot配置增强**：`snapshot`现在支持`levels`和`categories_mask`配置，升级后默认行为可能受影响，建议检查配置。
5.  **新增加密配置**：2.0版本新增了`pub_key`配置选项以支持混合加密，建议根据需求进行配置。
6.  **Android环境不再要求一定要从Java侧初始化BqLog，可以只在Native环境使用。**
---

## 将BqLog引入您的项目

### C++ (动态库/静态库/源码)
- **动态库/静态库**：下载Release中的预编译库，如果是动态库，请将`dynamic_lib/include`目录添加到头文件搜索路径。如果是静态库，请将`static_lib/include`目录添加到头文件搜索路径，并链接对应的静态库文件。
- **源码集成**：将代码仓库下的`/src`和`/include`目录拷贝进工程。并将`/include`目录添加到头文件搜索路径。
    - Windows下Visual Studio请添加编译选项 `/Zc:__cplusplus`。
    - Linux/Unix下支持更严格的编译环境。
    - Android模式下支持 `ANDROID_STL=none`。
    - 如果要定义Java和NAPI的支持，以及对于链接库和一些宏的定义，请参考 `/src/CMakeLists.txt`。

### C# (Unity / .NET)
- **Unity**：下载 `unity_package`。 解压后在Unity的package manager中选择添加tarball， 选择其中的tar文件导入。
- **.NET**：引用对应平台的动态库，并将 `/wrapper/csharp/src` 源码加入工程。

### Java / Kotlin (Android / Server)
- **Android**：引入 `.aar` 包或 `.so` + Java源码。
- **Server**：引入动态库 + Java源码。

### HarmonyOS (ArkTS / C++)
- 支持鸿蒙 `API Next` 及以上版本。
- 引入 `bqlog-harmony.har` 或集成源码。
- 支持在ArkTS侧直接调用，也支持在Native C++侧调用。

### Node.js
- 支持 CommonJS 和 ESM。
- 通过下载Release中的`nodejs_npm`包，解压后找到其中的bqlog-{version}.tgz进行 npm 安装：
  ```bash
  npm install ./bqlog-{version}.tgz
  ```

### Unreal Engine
- 插件位于 `/dist/bqlog-unreal-plugin`，支持 UE4 和 UE5。
- **Prebuilt版**：直接解压到项目 `Plugins` 目录。
- **源码版**：解压源码到 `Plugins` 目录，随项目一起编译。

---

## 简单的Demo

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
也可参考仓库下的 /demo/cpp 目录。

### Node.js
```javascript
import { bq } from "bqlog" //ESM写法
// const { bq } = require("bqlog");   //这是CJS写法

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);

log.info("Hello from Node.js! params: {}, {}", "text", 123);
        bq.log.force_flush_all_logs();
```
也可参考仓库下的 /demo/nodejs 目录。

### C#
```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```
也可参考仓库下的 /demo/csharp 目录。

### Java (Android / Server)
```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```
也可参考仓库下的 /demo/java 目录。

---

## 架构介绍
BqLog采用前后端分离架构。
- **前端 (Wrapper)**：提供多语言实现，负责极速序列化参数到RingBuffer，**无锁、几乎无堆内存分配**（其他日志库通常在此处有较大开销）。
- **后端 (Worker)**：单线程（或独立线程）消费LogBuffer，负责压缩、加密、IO写入。
- **高并发模式**：2.0版本中，当检测到高并发写入时，会自动为每个线程分配独立的高性能buffer，彻底消除线程竞争，性能提升40%。

---

## 配置说明
配置字符串采用 `properties` 格式。

### 1. 完整配置示例 (推荐)
```ini
# --- Appender 配置 ---

# 1. 控制台输出
appenders_config.console_app.type=console
appenders_config.console_app.levels=[debug,info,warning,error,fatal]

# 2. 一个叫file_app的自定义appender, 压缩文件输出 (推荐用于生产环境)
appenders_config.file_app.type=compressed_file
# 文件轮转和日志时间戳的时区设置为本地时间
appenders_config.file_app.time_zone =localtime
appenders_config.file_app.levels=[all]
appenders_config.file_app.file_name=logs/app_log
# 自动轮转：单个文件最大 10MB
appenders_config.file_app.max_file_size=10485760
# 自动清理：保留最近 7 天
appenders_config.file_app.expire_time_days=7
# 自动清理：总大小不超过 500MB
appenders_config.file_app.capacity_limit=524288000
# 目录类型：0=私有/沙盒(默认), 1=外部/公共
appenders_config.file_app.base_dir_type=0
# 加密：配置公钥即自动开启 RSA+AES 混合加密
appenders_config.file_app.pub_key=ssh-rsa AAAAB3NzaC1yc2E...

# 3. 一个叫file_text的自定义appender, 文本文件输出 (性能不如压缩格式，文件尺寸也更大，但是阅读和查询方便)
appenders_config.file_text.type=text_file
# 文件轮转和日志时间戳的时区设置为utc+8
appenders_config.file_text.time_zone =utc+8
# 只记录warning和以上的日志
appenders_config.file_text.levels=[warning,error,fatal]
# 也可以指定绝对路径
appenders_config.file_text.file_name=~/logs/app_text_log
# 自动轮转：单个文件最大 10MB
appenders_config.file_text.max_file_size=10485760
# 自动清理：保留最近 7 天
appenders_config.file_text.expire_time_days=7
# 自动清理：总大小不超过 500MB
appenders_config.file_text.capacity_limit=524288000

# --- Log 对象配置 ---

# 线程模式：async (推荐, 性能最高), sync, independent
log.thread_mode=async
# 缓冲区大小
log.buffer_size=65536
# 崩溃恢复：尝试在Crash时刷盘 (原 reliable_level)
log.recover=true
# 错误堆栈：在 Error/Fatal 级别打印调用栈，在哪个语言的wrapper调用就生成哪个语言的堆栈。
log.print_stack_levels=[error,fatal]
log.hi

# --- Snapshot (快照) 配置 ---
# 仅保留最后 64KB 的 Error 日志用于崩溃上报
snapshot.buffer_size=65536
snapshot.levels=[error,fatal]
```

### 2. 关键配置项详解

#### `base_dir_type` (取代 `is_in_sandbox`)
指定日志文件的存储基准目录，主要影响 Android/iOS 等移动平台。其他桌面和服务器平台一般代表当前程序目录。(***Mac环境下，代表`~/Library`下的目录***)
- `0`: **Internal/Private**。应用私有目录，卸载应用后随之删除。
- `1`: **External/Public**。外部存储目录，卸载后可能保留（需权限）。

#### `pub_key` (加密配置)
BqLog 2.0 支持 **RSA-2048 + AES-256** 混合加密。
- **配置方式**：直接将 OpenSSH 格式的公钥字符串填入 `pub_key`。
- **工作原理**：每次生成随机 AES Key 加密日志内容，再用 RSA 公钥加密 AES Key 存入文件头。
- **性能**：得益于 AES 的高效，开启加密仅带来约 10% 的性能损耗，且密文大小几乎不增加。

#### `log.recover` (取代 `reliable_level`)
- `true`: 开启。基于 mmap 等机制，在程序非正常退出（Crash/断电）时尽最大努力将 Buffer 中的日志刷入磁盘。
- `false`: 关闭。

#### `snapshot` (快照)
用于在内存中保留最后一段时间的日志，常用于 Crash 时通过`bq.log.take_snapshot()`获取的附加信息上报。
- `snapshot.levels`: 支持过滤特定级别的日志（如仅记录 error）。
- `snapshot.categories_mask`: 支持过滤特定模块的日志。

---

## Appender介绍

### 1. CompressedFileAppender (强烈推荐)
- **特点**：实时压缩，占用空间极小（通常为文本的 1/10到1/3），IO 开销极低，性能最强。
- **使用**：生产环境首选。需要使用解码工具查看。

### 2. TextFileAppender
- **特点**：纯文本输出，可直接阅读。
- **缺点**：IO 开销大，格式化开销大，占用空间大。
- **使用**：开发调试阶段使用。

### 3. ConsoleAppender
- **特点**：输出到控制台 (stdout, logcat, Xcode console)。
- **缺点**：格式化开销大，系统调用性能开销不可控，生产环境不推荐。
- **高级功能**：在 Unity/Unreal 中，会自动重定向到引擎的 Console 窗口。

### 4. RawFileAppender (Deprecated)
- **状态**：已废弃，不建议新项目使用。

---

## 最佳实践 (Best Practices)

1.  **生产环境务必使用 `CompressedFileAppender`**：
    它能显著降低磁盘 IO，从而减少对主线程的干扰（即使是异步日志，频繁 IO 也会抢占总线）。
2.  **使用格式化字符串**：
    
    ✅ 推荐：`log.info("Position: {}, {}", x, y);` (极快，延迟拼接，避免Heap Alloc)

    ❌ 避免：`log.info("Position: " + x + ", " + y);` (产生大量字符串拼接开销)
3.  **做好日志等级划分**：
    虽然 BqLog 极快，但每秒百万级的调用仍会消耗 CPU。建议生产环境关闭verbose/debug日志，仅保留info及以上级别。
4.  **利用 Category 管理模块**：
    使用 `log.cat.ModuleA.info(...)` 对日志进行分类，配合 `categories_mask` 可以灵活屏蔽不关注的模块日志。
5.  **Crash 捕捉配合 Force Flush**：
    在你自己捕获 Crash 的回调（如 `UncaughtExceptionHandler`或者native的`signal handle中`）中，调用 `bq::log::force_flush_all_logs()`，能进一步提高 Crash 现场日志的存活率。

---

## 高级使用话题  

### 1. 高并发模式 (Performance Mode)
BqLog 2.0 会智能检测线程并发度。当检测到高并发写入时，自动切换到**每线程独占 Buffer** 模式。
- **优势**：完全消除线程锁竞争，吞吐量大幅提升。
- **代价**：每个参与写日志的线程会多消耗约 32KB 内存（可忽略不计）。

### 2. 游戏引擎集成 (Unity / Unreal)
- **Unity**: 导入插件后，BqLog 的 ConsoleAppender 会自动输出到 Unity Console。
- **Unreal**: 插件自动拦截 ConsoleAppender 输出并转发给 `UE_LOG`。
    - 支持蓝图调用 (Blueprint Support)。
    - 支持 `FString`, `FName`, `FText` 直接作为参数打印。

### 3. 自定义参数类型
(参考 1.x 文档，API 保持兼容)

---

## 离线解码二进制格式的appender
针对 `CompressedFileAppender` 生成的日志文件，需使用解码工具还原为文本。
工具位于 `/tools/bin` 目录下：
- Windows: `BqLog_LogDecoder.exe`
- Mac/Linux: `BqLog_LogDecoder`

**基本用法**：
```bash
./BqLog_LogDecoder log_file.bq [output_file.txt]
```
**解密用法**（针对加密日志）：
需要提供 RSA 私钥文件（PEM格式）：
```bash
./BqLog_LogDecoder log_file.bq -k private_key.pem
```

---

## 构建说明
BqLog 2.0 提供了更完善的 CMake 构建系统。
- **Android**: 支持 `ANDROID_STL=none` 模式下的编译。
- **Unix/Linux**: 在 `build/lib` 下提供了标准构建脚本。
- **HarmonyOS**: 使用 DevEco Studio 打开 `build` 目录下的相关工程进行构建。

---

## Benchmark
测试环境：Intel i9-13900K, Windows 11。
对比对象：Log4j2 (Async Mode)。

| 场景 (4线程并发) | BqLog 2.0 (Compressed) | Log4j2 (Async) | 性能差异 |
| :--- | :--- | :--- | :--- |
| **4参数格式化** | **406 ms** | 4843 ms | **快 ~12 倍** |
| **无参数文本** | **467 ms** | 8485 ms | **快 ~18 倍** |

*注：BqLog 2.0 在高并发模式下，相比 1.x 版本性能进一步提升 40% 以上。*
