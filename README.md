# BqLog (BianQue Log) (V 2.0.1) ([ChangeLog](CHANGELOG.md))
[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.0.1-red.svg)](https://github.com/Tencent/BqLog/releases)

> BqLog is a lightweight, high-performance, industrial-grade logging system that has been widely used in online projects such as "Honor of Kings".
> **BqLog 2.0.1 is officially released! It brings native HarmonyOS support, Node.js support, stronger concurrency performance, and high-strength hybrid asymmetric encryption for logs.**

> [简体中文文档](./README_CHS.md)

---

## If you have the following pain points, try BqLog

- If your client product (especially games) wants to satisfy this "impossible triangle" at the same time:
  - Easy troubleshooting (log as much as possible)
  - Good performance (log as little as possible)
  - Save storage space (better not log at all)
- If you are a backend service developer and your current logging library cannot handle **high-concurrency scenarios**, causing log loss or application stalls.
- If your programming language is one of C++, Java, C#, Kotlin, TypeScript, JavaScript, or you use multiple languages at the same time and want a **unified cross-language logging solution**.

---

## Supported operating systems and platforms

- Windows 64-bit
- macOS
- Linux (including embedded Linux)
- iOS (iPhone, iPad, Apple Vision, Apple Watch and other Apple devices and simulators)
- Android
- HarmonyOS (phones, tablets, smart TV, and all HarmonyOS devices and simulators)
- Unix (tested on FreeBSD, NetBSD, OpenBSD, Solaris, DragonFly, OmniOS, etc.)

---

## Supported languages and environments

- C++ (C++11 and later, supports MSVC, Clang, GCC, MinGW-GCC, MinGW-Clang)
- Java / Kotlin (Android & Server)
- C# (Unity, Tuanjie Engine, .NET)
- **ArkTS / C++ (HarmonyOS)**
- **JavaScript / TypeScript (Node.js, supports CJS and ESM)**
- **Unreal Engine (UE4 & UE5)**

---

## Supported hardware architectures

- x86
- x86_64
- ARM32
- ARM64

---

## Ways to integrate

- Dynamic library
- Static library
- Source code

---

## Highlights

- Significant performance advantage over common open-source logging libraries (see [Benchmark](#benchmark)); suitable for server, client, and mobile.
- Low memory usage: in the [Benchmark](#benchmark) case (10 threads, 20,000,000 log entries), BqLog itself uses about 1 MB of memory.
- Provides a high-performance, high-compression real-time compressed log format.
- With less than 10% performance overhead, supports strong hybrid asymmetric encryption of logs to protect log content (optional).
- Works well inside game engines (`Unity`, `Unreal`, etc.), with [Supporting of UE builtin data types and Blueprint](#5-using-bqlog-in-unreal).
- Supports `utf8`, `utf16`, `utf32` characters and strings, as well as bool, float, double, and integer types of various sizes.
- Supports `C++20` `std::format` style format strings (without positional index and time formatting).
- Asynchronous logging supports crash recovery and tries to avoid data loss.
- Small binary size: Android dynamic library adds about 300 KB to the final package.
- On Java, C#, TypeScript wrappers, it can achieve "zero extra heap alloc" (or very close), avoiding continuous object allocations.
- Depends only on the standard C library and platform APIs; can be compiled with Android `ANDROID_STL = none`.
- Supports `C++11` and later standards and works under very strict compiler options.
- Build system is based on `CMake` and provides multi-platform scripts, easy to integrate.
- Supports custom parameter types.
- Very friendly for code completion and IDE hints.

---

## Technical articles

- **[Why BqLog is so fast - High Performance Realtime Compressed Log Format](docs/Article%201_Why%20is%20BqLog%20so%20fast%20-%20High%20Performance%20Realtime%20Compressed%20Log%20Format.MD)**
- **[Why BqLog is so fast - High Concurrency Ring Buffer](docs/Article%202_Why%20is%20BqLog%20so%20fast%20-%20High%20Concurrency%20Ring%20Buffer.MD)**

---

## Table of contents

**[Changes from 1.x to 2.x](#changes-from-1x-to-2x)**  
**[Integrating BqLog into your project](#integrating-bqlog-into-your-project)**  
**[Simple demo](#simple-demo)**  
**[Architecture](#architecture)**  
**[Main-flow API usage](#main-flow-api-usage)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Create a Log object](#1-create-a-log-object)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Get a Log object](#2-get-a-log-object)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. Write logs](#3-write-logs)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Other APIs](#4-other-apis)  
**[Synchronous vs asynchronous logging](#synchronous-vs-asynchronous-logging)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Thread-safety notes for asynchronous logging](#thread-safety-notes-for-asynchronous-logging)  
**[Appender overview](#appender-overview)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. ConsoleAppender](#consoleappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. TextFileAppender](#textfileappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. (Recommended) CompressedFileAppender](#compressedfileappender)  
**[Configuration](#configuration)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Full example](#1-full-example)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Detailed explanation](#2-detailed-explanation)  
**[Offline decoder for binary Appenders](#offline-decoder-for-binary-appenders)**  
**[Build](#build)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Library build](#1-library-build)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Demo build & run](#2-demo-build-and-run)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. Automated tests](#3-automated-tests)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Benchmark](#4-benchmark-build-run)  
**[Advanced topics](#advanced-topics)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. No Heap Alloc](#1-no-heap-alloc)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Log objects with Category support](#2-log-objects-with-category-support)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. Data protection on abnormal exit](#3-data-protection-on-abnormal-exit)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Custom parameter types](#4-custom-parameter-types)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. Using BqLog in Unreal](#5-using-bqlog-in-unreal)  
[&nbsp;&nbsp;&nbsp;&nbsp;6. Log encryption and decryption](#6-log-encryption-and-decryption)  
**[Benchmark](#benchmark)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Benchmark description](#1-benchmark-description)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. BqLog C++ Benchmark code](#2-bqlog-c-benchmark-code)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. BqLog Java Benchmark code](#3-bqlog-java-benchmark-code)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Log4j Benchmark code](#4-log4j-benchmark-code)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. Benchmark results](#5-benchmark-results)  
**[How to contribute](#how-to-contribute)**  

---

## Changes from 1.x to 2.x

1. Added HarmonyOS support, including ArkTS and C++.
2. Added Node.js support (CJS and ESM).
3. Improved cross-platform compatibility, stability and generality; supports more Unix systems.
4. Average performance improved by about 40%.
5. Android no longer must be used together with Java.
6. Removed the `is_in_sandbox` config and replaced it with `base_dir_type`; added filters for snapshots and support for opening a new log file on each startup. See [Configuration](#configuration).
7. Added high-performance hybrid asymmetric encryption; see [6. Log encryption and decryption](#6-log-encryption-and-decryption).
8. Provides Unity, Tuanjie Engine, and Unreal Engine plugins, making it easy to use in game engines; provides ConsoleAppender redirection to game-engine editors and Blueprint support for Unreal. See [5. Using BqLog in Unreal](#5-using-bqlog-in-unreal).
9. The repository no longer ships binaries. From 2.x on, please download platform- and language-specific packages from the [Releases page](https://github.com/Tencent/BqLog/releases).
10. The size of a single log entry is not limited by `log.buffer_size` anymore;
11. Timezone can be manually specified;

---

## Integrating BqLog into your project

> The examples below assume you have already downloaded the corresponding binary package or source code from the [Releases page](https://github.com/Tencent/BqLog/releases).

### C++ (dynamic / static / source)

- **Dynamic library**
  Download `dynamic_lib_{version}` archive:
  - Add `dynamic_lib/include` directory to your header search path;
  - Link against the dynamic library file in `dynamic_lib/lib` for your platform.

- **Static library**
  Download `static_lib_{version}` archive:
  - Add `static_lib/include` directory to your header search path;
  - Link against the static library file in `static_lib/lib` for your platform.

- **Source integration**
  - Add the `/src` directory under the repo into your project sources compilation;
  - Add `/include` directory to your header search path.
  - Windows + Visual Studio: please add compile option `/Zc:__cplusplus`.
  - Android supports `ANDROID_STL = none`.
  - If you need to enable Java / NAPI (Node.js / HarmonyOS ArkTS) support, as well as system link libraries and some macro definitions, please refer to `/src/CMakeLists.txt`.
  - When NAPI environment (Node.js or HarmonyOS ArkTS) is present, or when Java / C# needs to call, it is not recommended to integrate directly as "pure C++ source", because initialization and library loading processes need to be handled manually; using prebuilt packages and corresponding wrappers is more recommended.

### C# (Unity / Tuanjie Engine / .NET)

- **Unity**
  - Download `unity_package_{version}`;
  - Unzip and in Unity Package Manager select "Install from tarball", pointing to the `.tar` file inside to import;
  - Official Unity does not support HarmonyOS yet; if you need HarmonyOS support, you can integrate it yourself as needed.

- **Tuanjie Engine**
  - Download `tuanjie_package_{version}`;
  - Unzip and import via Unity Package Manager as tarball similarly;
  - The main difference from Unity is that HarmonyOS related support is already integrated.

- **.NET**
  - Download the dynamic library package `{os}_{arch}_libs_{version}` for the corresponding platform, and import the dynamic libraries within;
  - Add the `/wrapper/csharp/src` source code under the repository to your project.

### Java / Kotlin (Android / Server)

- **Android**
  - Download `android_libs_{version}`;
  - You can directly import the `.aar` package within, or manually import `.so` + `/wrapper/java/src` source code under the repository (the latter is convenient for customization and debugging).

- **Server**
  - Download the dynamic library `{os}_{arch}_libs_{version}` for the corresponding platform and import it;
  - Then download `java_wrapper_{version}`, import the jar package or directly add `/wrapper/java/src` source code under the repository.

### HarmonyOS (ArkTS / C++)

- Download `harmony_os_libs_{version}`;
- Import the `har` package, or directly import `.so` + `/wrapper/typescript/src` source code under the repository (optional);
- Supports direct calling from ArkTS side, also supports calling from Native C++ side.

### Node.js

- Supports CommonJS and ES Modules.
- From Releases download `nodejs_npm_{version}` package, unzip to find `bqlog-{version}.tgz` inside, install via npm:

```bash
npm install ./bqlog-{version}.tgz
```

Refer to `/demo/nodejs` directory under the repository.

### Unreal Engine

- **Prebuilt**
  - Download `unreal_plugin_prebuilt_{version}` from Releases;
  - Unzip and according to your engine version, select the corresponding compressed package, unzip to the `Plugins` directory of your game project.

- **Source**
  - Download `unreal_plugin_source_{version}` from Releases;
  - Unzip and according to your engine version, select the corresponding compressed package, unzip to the `Plugins` directory of your game project, to be recompiled by the engine.

---

## Simple Demo

### C++

```cpp
#include <string>
#include <bq_log/bq_log.h>

int main() {
    // Config: output to console
    std::string config = R"(
        appenders_config.appender_console.type=console
        appenders_config.appender_console.levels=[all]
    )";
    auto log = bq::log::create_log("main_log", config);

    log.info("Hello BqLog 2.0! int:{}, float:{}", 123, 3.14f);
    log.force_flush(); // Force flush (usually used before program exit)

    return 0;
}
```

For more examples, refer to `/demo/cpp` directory.

### Typescript (Node.js, ArkTS on HarmonyOS)

```typescript
import { bq } from "bqlog"; // ESM style
// const { bq } = require("bqlog"); // CommonJS style

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);

log.info("Hello from Node.js! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

For more examples, refer to `/demo/nodejs` directory.

### C#

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```

For more examples, refer to `/demo/csharp` directory.

### Java (Android / Server)

```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```

For more examples, refer to `/demo/java` directory.

---

## Architecture

![Structure](docs/img/log_structure.png)

The figure above shows the overall architecture of BqLog. The right side of the figure is `BqLog Core Engine`, and the left side is your business application and various language Wrappers.
Your program accesses the core engine through `BqLog Wrapper` provided by BqLog (such as C++, Java, C#, TypeScript, etc.).

- The user program creates multiple Log objects, such as `Log A`, `Log B`, `Log C`;
- Each Log object can mount one or more Appenders, which can be understood as the "output destination" of log content (Console / Text File / Compressed File, etc.);
- The Log object is responsible for "receiving logs + writing to buffer (ring queue)"; the Appender is responsible for "actual output to terminal or file".

**Within the same process, Wrappers of different languages can access the same Log object.**
For example: An Android application creates a Log object named `Log A` on the Java side, and the NDK C++ side can get the same Log object via the name `"Log A"` and write logs.

In extreme cases, such as a Unity game running on Android, involving Java/Kotlin, C#, and C++ simultaneously, they can all share the same Log object to unify log output.

---

## Main-flow API usage

> Note: The following APIs are all declared in `bq::log` (C++) or `bq.log` (other language Wrappers) class.
> To save space, only C++ APIs are listed here; other language Wrappers have consistent interface naming and semantics.

`bq::string` appearing in C++ is a UTF-8 string type defined internally by BqLog. In most cases, you can directly pass `char*`, `std::string`, `std::string_view`, etc., and BqLog will perform implicit conversion.

### 1. Create a Log object

Create Log through `create_log` static function:

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

Key points:

1. Whether in C# or Java, the return value of `create_log` will **never be null**. If creation fails due to incorrect configuration, etc., it can be judged via `is_valid()`.
2. If `log_name` is an empty string, BqLog will automatically assign a unique name, such as `"AutoBqLog_1"`.
3. If `create_log` is called for an existing Log with the same name, it will not create a new object, but reuse the original object and overwrite its configuration (some fields cannot be modified, such as `buffer_size`, see Configuration chapter).
4. You can initialize Log objects directly via this API in global variables or static variables without worrying about Static Initialization Order Fiasco, destruction order, or multi-threading issues.

### 2. Get a Log object

If a Log object has already been created elsewhere, it can be retrieved directly by name:

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

You can also use this function to initialize Log objects in global variables or static variables.
Note: Please ensure that the Log object with this name has already been created via `create_log`, otherwise the returned object will be in `!is_valid()` state.

### 3. Write logs

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

Writing logs requires focusing on three aspects:

#### 1. Log Level

Logs are divided into 6 levels: `verbose`, `debug`, `info`, `warning`, `error`, `fatal`, consistent with Android log levels, with increasing importance.
When outputting to the console, different levels will be distinguished by different colors (under ConsoleAppender).

![Log Level](docs/img/log_level.png)

#### 2. format string (`STR` parameter)

`STR` is similar to the first parameter of `printf`, used to describe the log format, supporting common string types under different languages, for example:

- `java.lang.String` in Java
- `string` in C#
- `FName`, `FString`, `FText`, etc. in Unreal
- Common string forms in C++:
  - C-style strings: `char*`, `char16_t*`, `char32_t*`, `wchar_t*`
  - Standard library strings: `std::string`, `std::u8string`, `std::u16string`, `std::u32string`, `std::wstring`, etc.

BqLog will unify them into UTF encoding suitable for storage and output internally.

#### 3. format parameters

`STR` can be followed by any number of parameters, which will be formatted into `{}` positions according to `C++20 std::format` rules (positional index and time formatting are not supported).

**It is strongly recommended to use format parameter way to output logs instead of manually splicing strings.**
This can significantly improve performance and allow the compressed format to achieve optimal results.

Currently supported parameter types include:

- Null pointer (outputs `null`)
- Pointer (outputs 16-hex address starting with `0x`)
- `bool`
- Single-byte character (`char`)
- Double-byte character (`char16_t`, `wchar_t`, C# `char`, Java `char`)
- Four-byte character (`char32_t` or `wchar_t` on some platforms)
- 8/16/32/64-bit integers and unsigned integers
- 32-bit and 64-bit floating point numbers
- Other C++ POD types (sizes of 1, 2, 4, 8 bytes, treated as `int8`/`int16`/`int32`/`int64`)
- All string types mentioned in [STR parameter](#2-format-string-str-parameter) above
- Any object in C# and Java (output via `ToString()` result)
- Custom parameter types (see [Custom parameter types](#4-custom-parameter-types))

### 4. Other APIs

For more details, please refer to the header file `bq_log/bq_log.h`, and `bq.log` class comments in Java / C# / TypeScript wrappers.
Here we focus on introducing several common capabilities.

#### Data protection on abnormal exit

```cpp
/// <summary>
/// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk.
/// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer
/// in the event of a crash. However, this functionality does not guarantee success,
/// and only supports POSIX systems.
/// </summary>
static void enable_auto_crash_handle();
```

Detailed introduction see [Data protection on abnormal exit](#3-data-protection-on-abnormal-exit).

#### Force flush buffer

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

BqLog uses asynchronous logging by default. Sometimes it is necessary to ensure that certain key logs are "immediately written to disk", `force_flush` / `force_flush_all_logs()` can be called in key paths or before program exit.

#### Intercept Console Output

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

[ConsoleAppender](#consoleappender) outputs to terminal (ADB Logcat for Android) by default, but this cannot cover all situations (such as self-developed game engines, self-developed IDEs, etc.).
By registering a callback, console output can be forwarded to self-developed systems.

**Note:**

1. Do not call any "synchronous flushing BqLog log functions" in console callback, otherwise it is extremely easy to cause deadlock.
2. When using Unity / Tuanjie Engine / Unreal plugins, there is no need to manually call this interface, the plugin has automatically redirected ConsoleAppender output to the editor log window.

#### Actively fetch Console Output

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

When it is not suitable to call back directly from native threads to virtual machines (such as C#, Java, IL2CPP environments) using callback method, you can switch to "actively fetch":

- `set_console_buffer_enable(true)`: Enable console buffer;
- `fetch_and_remove_console_buffer(...)`: Actively read and consume console logs in the buffer in the business thread.

**Note (IL2CPP environment):**

- Please ensure `on_console_callback` is a `static unsafe` method;
- And add `[MonoPInvokeCallback(typeof(type_console_callback))]` attribute to ensure the callback is not collected by GC.

#### Modify Log Configuration

```cpp
/// <summary>
/// Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
/// </summary>
/// <param name="config_content"></param>
/// <returns></returns>
bool reset_config(const bq::string& config_content);
```

If you need to adjust some configurations at runtime, you can call `reset_config`.
Some fields (such as `buffer_size`, `thread_mode`, etc.) are not allowed to be modified at runtime due to safety and implementation complexity considerations, see the corresponding table in the Configuration chapter.

#### Temporarily disable or enable certain Appenders

```cpp
/// <summary>
/// Temporarily disable or enable a specific Appender.
/// </summary>
/// <param name="appender_name"></param>
/// <param name="enable"></param>
void set_appender_enable(const bq::string& appender_name, bool enable);
```

By default, Appenders declared in the configuration are all enabled.
Through this API, you can temporarily close/open certain Appenders during operation as needed (for example, temporarily closing a file output).

#### Snapshot

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

Certain scenarios (such as anomaly detection, key event reporting) require obtaining a snapshot of logs for "the recent period", which can be achieved through this function:

1. Enable `snapshot` in configuration (see [snapshot](#snapshot-configuration) configuration section), and set buffer size, level and category filtering;
2. Call `take_snapshot()` when a snapshot is needed, then you can obtain the formatted recent log string.
   The return type in C++ is `bq::string`, which can be implicitly converted to `std::string` or C-style string.

#### Decode binary log files

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

This tool class is used to decode log files output by binary Appenders (such as [CompressedFileAppender](#compressedfileappender)) at runtime.

- Create `log_decoder` object;
- Call `decode()` repeatedly, decoding one log per time:
  - If it returns `bq::appender_decode_result::success`, you can call `get_last_decoded_log_entry()` to get the text;
  - If it returns `bq::appender_decode_result::eof`, it means decoding has reached the end of the file;
- If the log has encryption enabled, you need to pass in the private key string when constructing `log_decoder` or calling `decode_file` (see "Log encryption and decryption" later).

---

## Synchronous vs asynchronous logging

BqLog determines whether the log object uses synchronous or asynchronous mode through the configuration item `log.thread_mode`. The differences are as follows:

|                    | **Synchronous Logging**                                                                 | **Asynchronous Logging**                                                                 |
|:------------------:|------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| **Behavior**    | After calling the log function, the log will be output synchronously to the corresponding Appender immediately, ensuring that this log has been processed when the function returns.                                  | After calling the log function, the log is written to the buffer and processed asynchronously by the worker thread, and the thread writing the log returns quickly.                                |
| **Performance** | Lower performance, calling thread needs to block waiting for Appender output to complete.                                                              | Higher performance, calling thread does not need to wait for actual output.                                                                 |
| **Thread Safety** | Thread-safe under the premise that "parameters are not modified during the call"; performance is limited by serial output under multiple threads.                                                | Thread-safe under the premise that "parameters are not modified during the call"; internally improves scalability through high-concurrency ring queue and worker thread scheduling.                       |

### Thread-safety notes for asynchronous logging

Common concern with asynchronous logging is: **When the worker thread actually processes the log, the parameters are already invalid (lifecycle ended)**. For example:

```cpp
{
    const char str_array[5] = {'T', 'E', 'S', 'T', '\0'};
    const char* str_ptr = str_array;
    log_obj.info("This is test param :{}, {}", str_array, str_ptr);
}
```

`str_array` is a stack variable, and the memory is invalid after leaving the scope.
The worry is: if the worker thread processes it later, will it read a dangling pointer?

**This will NOT happen in BqLog:**

- During the call of `info` (or other log functions), BqLog copies all parameter contents completely to the internal ring buffer;
- Once the log function returns, it means the data required for this log has been safely stored in BqLog's internal cache;
- Subsequent worker threads only read internal cache and will not access data on the caller's stack.

**The situation that may really cause problems is "parameter content is modified by other threads during the call":**

```cpp
static std::string global_str = "hello world";   // This is a global variable, multiple threads are modifying it at the same time.

void thread_a()
{
    log_obj.info("This is test param :{}", global_str);
}
```

If other threads modify the content of `global_str` during the process of `info` function processing input parameters to buffer writing, it will cause undefined output content (although BqLog will try to ensure no crash).
The conclusion is: **Please ensure that the parameters passed in a single log call are not modified during the call**, regardless of synchronous/asynchronous.

---

## Appender overview

Appender represents the final output destination of the log, and its concept is basically consistent with the Appender in Log4j.
Currently BqLog provides the following types of Appenders:

### ConsoleAppender

- Output destination: Console / Terminal;
- Output to ADB Logcat on Android;
- Output encoding: UTF-8 text.

### TextFileAppender

- Output log files directly in UTF-8 text format;
- Log files are human-readable, suitable for quick troubleshooting.

### CompressedFileAppender

- Output log files in high-performance compressed format;
- It is **BqLog's recommended default file output format**;
- Highest performance among all Appenders, smallest output file size;
- Reading requires BqLog's built-in decoding tool or `bq::tools::log_decoder`;
- Supports encryption (based on RSA2048 + AES256 hybrid encryption).

Comprehensive comparison is as follows:

| Name                     | Output Destination | Plaintext Readable | Output Performance | Output Size | Encryption Support |
|--------------------------|---------|-------------|----------|----------|-------------|
| ConsoleAppender          | Console   | ✔           | Low       | -        | ✘           |
| TextFileAppender         | File     | ✔           | Low       | Large       | ✘           |
| CompressedFileAppender   | File     | ✘           | High       | Small       | ✔           |

> Note: "Encryption" mentioned in the article refers only to CompressedFileAppender supporting hybrid encryption format using "RSA2048 public key + AES256 symmetric key".
> The encryption format is based on OpenSSH style `ssh-rsa` public key text (PEM), and the private key needs to be generated by `ssh-keygen`, see [Log encryption and decryption](#7-log-encryption-and-decryption).

---

## Configuration

"Configuration" refers to the `config` string in `create_log` and `reset_config` functions.
This string uses **properties file format**, supports `#` single-line comments (must be on a separate line and start with `#`).

### 1. Full example

```ini
# This config configures 5 Appenders for the log object, including two TextFileAppenders outputting to different files.

# The first Appender is named appender_0, type is ConsoleAppender
appenders_config.appender_0.type=console
# appender_0 uses system local time
appenders_config.appender_0.time_zone=localtime
# appender_0 will output logs of all 6 levels (Note: do not have spaces between different log levels, otherwise parsing fails)
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]

# The second Appender is named appender_1, type is TextFileAppender
appenders_config.appender_1.type=text_file
# Use GMT time (UTC0)
appenders_config.appender_1.time_zone=gmt
# Only output logs of info and above four levels, other levels will be ignored
appenders_config.appender_1.levels=[info,warning,error,fatal]
# base_dir_type determines the base directory of relative path, here is 1:
# iOS: /var/mobile/Containers/Data/Application/[APP]/Library/Caches
# Android: [android.content.Context.getExternalFilesDir()]
# HarmonyOS: /data/storage/el2/base/cache
# Other platforms: current working directory
appenders_config.appender_1.base_dir_type=1
# appender_1 save path is relative path bqLog/normal, using rolling file:
# Filename like normal_YYYYMMDD_xxx.log, see "Path and Rolling Strategy" later.
appenders_config.appender_1.file_name=bqLog/normal
# Maximum 10,000,000 bytes per file, new file if exceeded
appenders_config.appender_1.max_file_size=10000000
# Old files older than 10 days will be automatically cleaned
appenders_config.appender_1.expire_time_days=10
# When total size of all files of this Appender in the same output directory exceeds 100,000,000 bytes,
# clean from oldest files by date
appenders_config.appender_1.capacity_limit=100000000

# The third Appender is named appender_2, type is TextFileAppender
appenders_config.appender_2.type=text_file
# Output all levels logs
appenders_config.appender_2.levels=[all]
# base_dir_type is 0:
# iOS: /var/mobile/Containers/Data/Application/[APP]/Documents
# Android: [android.content.Context.getFilesDir()]
# HarmonyOS: /data/storage/el2/base/files
# Other platforms: current working directory
appenders_config.appender_2.base_dir_type=0
# Path is bqLog/new_normal, filename like new_normal_YYYYMMDD_xxx.log
appenders_config.appender_2.file_name=bqLog/new_normal

# The fourth Appender is named appender_3, type is CompressedFileAppender
appenders_config.appender_3.type=compressed_file
# Output all levels logs
appenders_config.appender_3.levels=[all]
# Save path is ~/bqLog/compress_log, filename like compress_log_YYYYMMDD_xxx.logcompr
appenders_config.appender_3.file_name=~/bqLog/compress_log
# appender_3 output content will use RSA2048 public key below for hybrid encryption
appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname

# The fifth Appender is named appender_4, type is ConsoleAppender
appenders_config.appender_4.type=console
# appender_4 is disabled by default, can be enabled via set_appender_enable later
appenders_config.appender_4.enable=false
# Output all levels logs
appenders_config.appender_4.levels=[all]
# Output logs only when Category starts with ModuleA or ModuleB.SystemC, others ignored
# (Concept of Category see "Log objects with Category support" later)
appenders_config.appender_4.categories_mask=[ModuleA,ModuleB.SystemC]

# Overall asynchronous buffer size is 65535 bytes, see meaning later
log.buffer_size=65535
# Enable log Crash recovery function, see "Data protection on abnormal exit"
log.recovery=true
# Process logs only when log Category matches any wildcard below, others ignored
log.categories_mask=[*default,ModuleA,ModuleB.SystemC]
# Use asynchronous log mode (Recommended)
log.thread_mode=async
# Attach call stack after each log when log level is error or fatal
log.print_stack_levels=[error,fatal]

# Enable snapshot function, snapshot buffer size is 64KB
snapshot.buffer_size=65536
# Only record logs of info and error levels to snapshot
snapshot.levels=[info,error]
# Record to snapshot only when Category is ModuleA.SystemA.ClassA or starts with ModuleB
snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
```

### 2. Detailed explanation

#### `appenders_config`

`appenders_config` is a set of configurations about Appender.
`<name>` in `appenders_config.<name>.xxx` is the Appender name, configurations with same `<name>` act on the same Appender instance.

| Name                         | Mandatory | Configurable Values                                | Default Value             | ConsoleAppender | TextFileAppender | CompressedFileAppender |
|------------------------------|---------|-----------------------------------------|--------------------|-----------------|------------------|------------------------|
| `type`                       | ✔       | `console` / `text_file` / `compressed_file` / `raw_file` | -                  | ✔               | ✔                | ✔ (Encryption requires this type)      |
| `enable`                     | ✘       | `true` / `false`                        | `true`             | ✔               | ✔                | ✔                      |
| `levels`                     | ✘       | Log level array (`[verbose,...]` or `[all]`) | `[all]`            | ✔               | ✔                | ✔                      |
| `time_zone`                  | ✘       | `gmt` / `localtime` / `Z` / `UTC` / `utc+8` / `utc-2` / `utc+11:30` etc. | `localtime` | ✔               | ✔                | ✔ (Affects rolling date)      |
| `file_name`                  | ✔ (File type) | Relative or absolute path (no extension)                | -                  | ✘               | ✔                | ✔                      |
| `base_dir_type`             | ✘       | `0` / `1`                               | `0`                | ✘               | ✔                | ✔                      |
| `max_file_size`             | ✘       | Positive integer or `0`                            | `0` (Unlimited)       | ✘               | ✔                | ✔                      |
| `expire_time_seconds`       | ✘       | Positive integer or `0`                            | `0` (No clean)       | ✘               | ✔                | ✔                      |
| `expire_time_days`          | ✘       | Positive integer or `0`                            | `0` (No clean)       | ✘               | ✔                | ✔                      |
| `capacity_limit`            | ✘       | Positive integer or `0`                            | `0` (Unlimited)       | ✘               | ✔                | ✔                      |
| `categories_mask`           | ✘       | String array (`[]`)                       | Empty (No filtering)        | ✔               | ✔                | ✔                      |
| `always_create_new_file`    | ✘       | `true` / `false`                        | `false`            | ✘               | ✔                | ✔                      |
| `pub_key`                   | ✘       | RSA2048 Public Key (OpenSSH `ssh-rsa` text)  | Empty (No encryption)        | ✘               | ✘                | ✔ (Enable hybrid encryption)      |

##### (1) `appenders_config.xxx.type`

Specify Appender type:

- `console` → [ConsoleAppender](#consoleappender)
- `text_file` → [TextFileAppender](#textfileappender)
- `compressed_file` → [CompressedFileAppender](#compressedfileappender)

##### (2) `appenders_config.xxx.enable`

Whether to enable this Appender by default, default is `true`.
If `false`, Appenders will be created during initialization but not actually output, can be switched at runtime via `set_appender_enable`.

##### (3) `appenders_config.xxx.levels`

Array wrapped in `[]`, content is:

- Arbitrary combination: `[verbose,debug,info,warning,error,fatal]`
- Or special value `[all]`, indicating all levels are output.

##### (4) `appenders_config.xxx.time_zone`

Specify timezone used for timestamp formatting, also affects "date boundary" for rolling files by date:

- `"gmt"`, `"Z"`, `"UTC"`: Use UTC0 (Greenwich Mean Time);
- `"localtime"`: Use system local time;
- `"utc+8"`, `"utc-2"`, `"utc+11:30"` etc.: Explicitly specify offset.

Effect:

- ConsoleAppender / TextFileAppender: Determine display of time field in log text;
- TextFileAppender / CompressedFileAppender / RawFileAppender: Determine cut-off point for file rolling by date (0 o'clock every day).

##### (5) `appenders_config.xxx.base_dir_type`

Specify base directory when `file_name` is relative path:

- `0`
  - Android: Try in order
    - `android.content.Context.getFilesDir()`
    - `android.content.Context.getExternalFilesDir()`
    - `android.content.Context.getCacheDir()`
  - iOS: `/var/mobile/Containers/Data/Application/[APP]/Documents`
  - HarmonyOS: `/data/storage/el2/base/files`
  - Other platforms: Process current working directory
- `1`
  - Android: Try in order
    - `android.content.Context.getExternalFilesDir()`
    - `android.content.Context.getFilesDir()`
    - `android.content.Context.getCacheDir()`
  - iOS: `/var/mobile/Containers/Data/Application/[APP]/Library/Caches`
  - HarmonyOS: `/data/storage/el2/base/cache`
  - Other platforms: Process current working directory

##### (6) `appenders_config.xxx.file_name`

Specify path and filename prefix of log file, example:

```ini
appenders_config.appender_3.file_name=~/bqLog/compress_log
```

Final actual filename will be auto-completed by BqLog with date and rolling number, for example:

- `compress_log_20250101_0001.logcompr` (CompressedFileAppender)
- `normal_20250101_0001.log` (TextFileAppender)

If it is relative path, it is based on directory corresponding to `base_dir_type`.

##### Brief description of other fields

- `max_file_size`: Max bytes per single file, new file created if exceeded; `0` means not split by size.
- `expire_time_seconds`: Clean up expired files by seconds; `0` disables this function.
- `expire_time_days`: Clean up expired files by days; `0` disables this function.
- `capacity_limit`: Limit total size of files output by this Appender, delete from oldest files by time when exceeded.
- `categories_mask`: Output logs only when log Category matches prefix in this array (see [Log objects with Category support](#2-log-objects-with-category-support)).
- `always_create_new_file`: When `true`, create new file every time process restarts even within same day; default `false` is append write.
- `pub_key`: Provide encryption public key for CompressedFileAppender, string content should be completely copied from `.pub` file generated by `ssh-keygen`, and start with `ssh-rsa `. Details see [Log encryption and decryption](#6-log-encryption-and-decryption).

---

### `log` Configuration

`log.xxx` configuration acts on the entire Log object.

| Name                                      | Mandatory | Configurable Values                               | Default Value                                                         | Modifiable via `reset_config` |
|-------------------------------------------|---------|----------------------------------------|----------------------------------------------------------------|--------------------------------|
| `log.thread_mode`                         | ✘       | `sync` / `async` / `independent`       | `async`                                                        | ✘                              |
| `log.buffer_size`                         | ✘       | 32-bit Positive Integer                            | Desktop/Server: `65536`; Mobile: `32768`                         | ✘                              |
| `log.recovery`                            | ✘       | `true` / `false`                       | `false`                                                        | ✘                              |
| `log.categories_mask`                     | ✘       | String array (`[]`)                      | Empty (No filtering)                                                   | ✔                              |
| `log.print_stack_levels`                  | ✘       | Log level array                           | Empty (No call stack printing)                                             | ✔                              |
| `log.buffer_policy_when_full`             | ✘       | `discard` / `block` / `expand`         | `block`                                                        | ✘                              |
| `log.high_perform_mode_freq_threshold_per_second` | ✘ | 64-bit Positive Integer                            | `1000`                                                         | ✘                              |

#### `log.thread_mode`

Determines which thread processes data in buffer:

- `sync`: Synchronous log mode. The thread writing logs is directly responsible for processing and outputting logs, output is completed when call ends; (Low performance, not recommended)
- `async` (Default): Asynchronous mode. Thread writing logs only writes to buffer, global worker thread uniformly handles output of all asynchronous log objects;
- `independent`: Independent asynchronous mode. Create a dedicated worker thread for this Log object alone. Suitable for scenarios where single Log write volume is huge and complete decoupling from other Logs is desired.

#### `log.buffer_size`

Log buffer size (bytes).
Larger buffer can withstand larger burst write peaks, but memory usage will also increase.

#### `log.recovery`

- `true`
  When asynchronous log (`log.thread_mode` is `async` or `independent`) encounters abnormal program exit, data in buffer that has not been flushed to disk will be rewritten to log file after next startup.
- `false`
  Log data in buffer will be lost when process exits abnormally and `force_flush()` is not called.

Detailed behavior see [Data protection on abnormal exit](#3-data-protection-on-abnormal-exit).

#### `log.categories_mask`

Behavior consistent with `appenders_config.xxx.categories_mask`, but scope is entire Log object.

- For synchronous logs: Filtering is done in calling thread;
- For asynchronous logs: Filtering is done when writing to buffer, reducing unnecessary data entry/exit.

See [Log objects with Category support](#2-log-objects-with-category-support).

#### `log.print_stack_levels`

Configuration method similar to `appenders_config.xxx.levels`, used to specify which log levels need to automatically attach call stack. For example:

```ini
log.print_stack_levels=[error,fatal]
```

Recommended to enable only in Debug / Test environment to avoid significant impact on online performance.

#### `log.buffer_policy_when_full`

Behavior when buffer is full:

- `discard`: Discard newly written logs until buffer has enough space;
- `block` (Recommended Default): Thread writing logs will block waiting for space in buffer;
- `expand` (Not Recommended): Buffer will dynamically expand to twice original size until writable.
  May significantly increase memory usage, although BqLog reduces expansion frequency through good thread scheduling, it is still recommended to use with caution.

#### `log.high_perform_mode_freq_threshold_per_second`

This configuration item is used to control "High Performance Mode" trigger threshold:

- When number of logs recorded by a single thread in one second exceeds this value, the thread will automatically enter high performance mode;
- Under high performance mode, internal resources more suitable for high frequency writing will be allocated for this thread;
- When writing frequency drops, it will automatically exit high performance mode and release related resources.

Default value is `1000` (entries/second).
Configuring as `0` means disabling this function.

To reduce memory fragmentation, physical memory allocation is usually performed in batches of "several cache lines" as a group (16 for desktop platforms, usually 2 for high-end mobile platforms). Therefore, even if only one thread enters high performance mode, it will occupy extra space of one group of caches.

---

### `snapshot` Configuration

`snapshot.xxx` is used to configure log snapshot function of this Log object.
Applicable to: Need to report "recent period" logs of this Log after detecting anomaly.

| Name                   | Mandatory | Configurable Values       | Default Value | Modifiable via `reset_config` |
|------------------------|---------|----------------|--------|--------------------------------|
| `snapshot.buffer_size` | ✘       | 32-bit Positive Integer    | `0`    | ✔                              |
| `snapshot.levels`      | ✘       | Log level array   | `[all]`| ✔                              |
| `snapshot.categories_mask` | ✘   | String array     | Empty     | ✔                              |

- `snapshot.buffer_size`: Snapshot buffer size (bytes), snapshot function disabled when `0` or not configured;
- `snapshot.levels`: Only levels in configuration will be written to snapshot; defaults to `[all]` if not configured (slightly different from `levels` default behavior above);
- `snapshot.categories_mask`: Behavior consistent with `appenders_config.xxx.categories_mask`, only matching Category will be written to snapshot; filter all Categories if not configured.

---

## Offline decoder for binary Appenders

Outside program, BqLog provides precompiled command line tools for decoding compressed binary log files.
Download tool package `{os}_{arch}_tools_{version}` corresponding to OS and architecture from Releases, unzip to find:

- `BqLog_LogDecoder`

Usage:

```bash
./BqLog_LogDecoder FileToDecode [-o OutputFile] [-k PrivateKeyFile]
```

- When `-o` is not specified, decoding result is output directly to standard output;
- If log file is encrypted format, private key file path needs to be specified via `-k` (see [Log encryption and decryption](#6-log-encryption-and-decryption));
- **Note: Binary format may be incompatible between different versions of BqLog**, please use matching version of decoder.

---

## Build

For users who need to modify and compile BqLog themselves, all build scripts are located in `/build` directory:

```text
/build
├── demo       # Demo Build
├── lib        # Native Static Library and Dynamic Library Build
├── test       # Test Project Build
├── tools      # Tools Project Build (Including LogDecoder, CategoryLogGenerator, etc.)
├── wrapper    # Language Wrapper Projects (Java / C# etc.)
├── benchmark  # Benchmark Project Build
└── plugin     # Game Engine Plugin Build (Unity / Tuanjie / Unreal)
```

### 1. Library build

Build scripts for different platforms are located under `/build/lib`. Please ensure necessary environment variables are set before building:

- `ANDROID_NDK_ROOT`: Android NDK path (Required for compiling Android library);
- `JAVA_HOME`: JDK path (Most scripts enable Java Wrapper by default, if you don't need it, you can remove `-DJAVA_SUPPORT=ON` in CMake or script yourself).

### 2. Demo build and run

Demo project build scripts are located in `/build/demo`.
Note: C# and Java Demos need to manually place dynamic library of corresponding platform to path searchable at runtime (or specify via environment variable / runtime parameter).

### 3. Automated tests

Test project generation script and "generate + run" script are located in `/build/test`.
Recommended to ensure all test cases pass before submitting.

### 4. Benchmark build run

Benchmark project scripts are located in `/build/benchmark`.
Also contain generation project script and direct run script, can be used to evaluate performance under different platforms and configurations.

---

## Advanced topics

### 1. No Heap Alloc

In runtimes like Java, C#, TypeScript, logging library usually generates small amount of Heap allocation when writing each log, which will trigger GC and affect performance over time.
BqLog strives to achieve "Zero or extremely low Heap Alloc" on these platforms via following ways:

- **Internally avoid creating temporary objects and strings in log path**;
- **Avoid extra array allocation caused by variable parameters** (avoided via polymorphism overload in C#);
- **Reduce boxing/unboxing**:

  - In C# Wrapper, when parameter count ≤ 12, no boxing/unboxing operation will occur, boxing degenerates only when exceeding 12 parameters;
  - TypeScript passes parameters directly via NAPI, avoiding multi-layer boxing;
  - Java Wrapper uses special tool methods to manually avoid boxing, for example:

```java
// Java
// Primitive parameters wrapped using bq.utils.param.no_boxing will not cause boxing,
// naked passing 5.3245f will cause boxing, increasing GC risk.
import static bq.utils.param.no_boxing;

my_demo_log.info(
    my_demo_log.cat.node_2.node_5,
    "Demo Log Test Log, {}, {}",
    no_boxing(false),
    5.3245f
);
```

Using above APIs reasonably can significantly reduce GC interference and obtain stable high performance log behavior.

---

### 2. Log objects with Category support

#### Category Concept and Usage

In Unreal Engine, log has Category concept, but native interface is not very friendly to code hints.
In BqLog, Category is used to identify "which module / subsystem a log belongs to", and supports multi-level hierarchy structure.

For example, we define a business log object, its Category tree is roughly:

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

Usage (C++ Example):

```cpp
my_category_log.info("Log0");  // Category = *default
my_category_log.info(my_category_log.cat.Shop, "Log1");  // Category = Shop
my_category_log.info(my_category_log.cat.Shop.Seller, "Log2"); // Category = Shop.Seller
my_category_log.info(my_category_log.cat.Transport.Vehicles.Driver, "Log3"); // Category = Transport.Vehicles.Driver
my_category_log.info(my_category_log.cat.Factory, "Log4"); // Category = Factory
my_category_log.info(my_category_log.cat.Factory.People, "Log5"); // Category = Factory.People
```

Output Example:

```text
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] Log0
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Shop] Log1
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Shop.Seller] Log2
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Transport.Vehicles.Driver] Log3
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Factory] Log4
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ] [I] [Factory.People] Log5
```

Cooperating with `categories_mask` in configuration earlier, flexible filtering can be performed on output side.
Combined with [Intercept Console Output](#intercept-console-output), you can get Category name list via `category_idx` + following API:

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

This is often used to display multi-dimensional filters in custom UI.

#### Category Log Class Generation

Log class supporting Category is not the default `bq::log` / `bq.log`, but a dedicated class generated by tools.
Generation steps are as follows:

1. Prepare a text configuration file defining all Categories:

   **BussinessCategories.txt**

   ```text
   // This config file supports comments with double slash
   Shop.Manager      // No need to list Shop separately, this line automatically generates Shop and Shop.Manager
   Shop.Seller
   Factory.People.Manager
   Factory.People.Worker
   Factory.Machine
   Factory.House
   Transport.Vehicles.Driver
   Transport.Vehicles.Maintenance
   Transport.Trains
   ```

2. Use BqLog built-in command line tool to generate corresponding class:
   Download `{os}_{arch}_tools_{version}` in Releases, unzip to find:

  - `BqLog_CategoryLogGenerator`

3. Usage:

   ```bash
   ./BqLog_CategoryLogGenerator ClassNameToGenerate CategoryConfigFile [OutputDirectory, default current directory]
   ```

   Example:

   ```bash
   ./BqLog_CategoryLogGenerator business_log /path/to/BussinessCategories.txt ./
   ```

   Will generate 5 files in current directory:

  - `business_log.h` (C++ header wrapper)
  - `business_log.java` (Java wrapper)
  - `business_log.cs` (C# wrapper)
  - `business_log.ts` (TypeScript wrapper)
  - `business_log_for_UE.h` (Coordinate with UE project, Category can be referenced in Blueprint)

4. Import these files in project to use Log class with Category.
   For example C++:

   ```cpp
   bq::business_log my_log = bq::business_log::create_log("MyLog", config);
   ```

   Or get created Log object with same name:

   ```cpp
   bq::business_log my_log = bq::business_log::get_log_by_name("MyLog");
   ```

   Use `.` completion on `my_log.cat` to get pre-defined Category list.
   If Category parameter is not passed, `*default` is used by default.

---

### 3. Data protection on abnormal exit

When BqLog uses asynchronous mode, if program exits abnormally (crash, etc.), logs in buffer may not be flushed to disk in time.
BqLog provides two mechanisms to minimize loss:

#### 1) Signal handling mechanism (POSIX)

```cpp
/// <summary>
/// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk.
/// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash.
/// However, this functionality does not guarantee success, and only supports POSIX systems.
/// </summary>
static void enable_auto_crash_handle();
```

After calling `bq::log::enable_auto_crash_handle()`, BqLog will register several signal handlers on POSIX system:

- When process receives exception signals such as `SIGABRT`, `SIGSEGV`, `SIGBUS`, try to force flush buffer (`force_flush_all_logs`) in signal processing callback;
- Internally implemented via `sigaction`, and old signal processing handle will be saved before registration, and original processing logic will be called after self-processing is completed to minimize impact on host program.

However, please note:

- This mechanism is essentially "emergency remediation", **cannot guarantee 100% success** - if memory itself is severely corrupted, any operation may fail;
- This mechanism only works on POSIX platforms, and will not take effect on Windows.

#### 2) Recovery Mechanism (Recovery)

Refer to configuration item [`log.recovery`](#logrecovery).
When this item is `true`, BqLog will try to use platform features on some platforms to ensure content in asynchronous buffer has temporary stub on disk; next time startup, "recovery" can be performed to try to restore logs not fully flushed to disk.

Specific implementation details depend on operating system capabilities and will be continuously enhanced in future versions.

---

### 4. Custom parameter types

As explained in [format parameters](#3-format-parameters) earlier, a large number of common types are supported by default.
If you need to extend custom types, there are two ways:

> **Important Hint:**
> Please be sure to `#include` your custom class and related function declarations before `bq_log.h` or generated Category header file.
> Some compilers (especially Clang) may fail to compile when include order is incorrect.

#### Method 1: Implement `bq_log_format_str_size()` and `bq_log_format_str_chars()` in class

```cpp
// custom_bq_log_type.h
class A {
private:
    bool value_;

public:
    explicit A(bool value) : value_(value) {}

    // Return "character count" instead of "byte count", return type must be size_t
    size_t bq_log_format_str_size() const {
        return value_ ? strlen("true") : strlen("false");
    }

    // Return actual string start character address, can be char* / char16_t* / char32_t* / wchar_t*
    const char* bq_log_format_str_chars() const {
        return value_ ? "true" : "false";
    }
};
```

Usage Example:

```cpp
#include "custom_bq_log_type.h"
#include "bq_log/bq_log.h"

void output(const bq::log& log_obj)
{
    log_obj.info("This should be Class A1:{}, A2:{}", A(true), A(false));
}
```

#### Method 2: Implement global `bq_log_format_str_size()` and `bq_log_format_str_chars()`

Applicable to cases where type definition cannot be modified (such as `FString`, `FName`, etc. of Unreal), or wishing to override default output method of built-in types.

Since priority of custom types is higher than built-in types, you can even redefine output of `int32_t`, for example:

```cpp
// custom_bq_log_type.h
#pragma once
#include <map>
#include <cinttypes>
#include <cstring>

// Override int32_t default output behavior
size_t bq_log_format_str_size(const int32_t& param);
const char* bq_log_format_str_chars(const int32_t& param);

// Allow std::map to be passed as parameter
template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param);

template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param);

// Implementation
template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param)
{
    if (param.empty()) {
        return strlen("empty");
    } else {
        return strlen("full");
    }
}

// Use UTF-16 encoding
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

Usage Example:

```cpp
#include "custom_bq_log_type.h"   // Ensure before bq_log.h
#include "bq_log/bq_log.h"

void output(const bq::log& my_category_log)
{
    std::map<int, bool> param0;
    std::map<int, bool> param1;
    param0[5] = false;

    my_category_log.info("This should be full:{}", param0);   // Output full
    my_category_log.info("This should be empty:{}", param1);  // Output empty
    my_category_log.info("This should be PLUS:{}", 5);        // Output PLUS
    my_category_log.info("This should be MINUS:{}", -1);      // Output MINUS
    my_category_log.info(param0);                             // Output full
}
```

---

### 5. Using BqLog in Unreal

#### 1) Support for `FName` / `FString` / `FText`

In Unreal environment, BqLog has built-in adapters:

- Automatically support `FString`, `FName`, `FText` as format string and parameters;
- Compatible with UE4 and UE5.

Example:

```cpp
bq::log log_my = bq::log::create_log("AAA", config);   // config omitted

FString fstring_1 = TEXT("This is a test FString{}");
FString fstring_2 = TEXT("This is also a test FString");
log_my.error(fstring_1, fstring_2);

FText text1 = FText::FromString(TEXT("This is a FText!"));
FName name1 = FName(TEXT("This is a FName"));
log_my.error(fstring_1, text1);
log_my.error(fstring_1, name1);
```

If you wish to customize adaptation behavior, you can also use "Method 2 (Global Function)" above to define `bq_log_format_str_size` and `bq_log_format_str_chars` yourself.

#### 2) Redirect BqLog output to Unreal Log Window

If you have imported Unreal plugin according to [Unreal Engine Integration Instructions](#unreal-engine), BqLog logs will be automatically redirected to Unreal's Output Log.
If plugin is not used, but BqLog is integrated directly at C++ level, you can use console callback to forward yourself:

```cpp
// You can get Log name and Category name based on different category_idx / log_id,
// forward them to different UE_LOG Categories.
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

#### 3) Using BqLog in Blueprint

After importing plugin according to [Unreal Engine Integration Instructions](#unreal-engine), BqLog can be called directly in Blueprint:

1. **Create Log Data Asset**

  - Create Data Asset in Unreal project, type select BqLog:
    - Default Log Type (without Category):  
      <img src="docs/img/ue_pick_data_asset_1.png" alt="Default Log Creation" style="width: 455px">
    - If Log Class with Category is generated according to [Category Log Class Generation](#category-log-class-generation), and `{category}.h` and `{category}_for_UE.h` are added to project:  
      <img src="docs/img/ue_pick_data_asset_2.png" alt="Category Log Creation" style="width: 455px">

2. **Configure Log Parameters**

  - Double click to open Data Asset, configure log object name and creation method:  
    - `Create New Log`: Create a new Log object at runtime:  
      <img src="docs/img/ue_create_log_config_1.png" alt="Config Log Params Create New Log" style="width: 455px">
    - `Get Log By Name`: Only get Log with same name created elsewhere:  
      <img src="docs/img/ue_create_log_config_2.png" alt="Config Log Params Get Log By Name" style="width: 455px">

3. **Call Log Node in Blueprint**  
  
   <img src="docs/img/ue_print_log.png" alt="Blueprint Call Log" style="width: 655px">

  - Area 1: Add log parameters;
  - Area 2: Added log parameter nodes, can be deleted via right click menu (Remove ArgX);
  - Area 3: Select log object (i.e. Data Asset just created);
  - Area 4: Displayed only when log object has Category, can select log Category.

4. **Test**

  - Run Blueprint, if configured correctly and there is ConsoleAppender output, similar output can be seen in Log window:

    ```text
    LogBqLog: Display: [Bussiness_Log_Obj] UTC+7 2025-11-27 14:49:19.381[tid-27732 ] [I] [Factory.People.Manager] Test Log Arg0:String Arg, Arg1:TRUE, Arg2:1.000000,0.000000,0.000000|2.999996,0.00[...]
    ```

---

### 6. Log encryption and decryption

For external distribution clients (especially internet games and Apps), log encryption is an important requirement.
In version 1.x, there are still a lot of plaintext in BqLog's binary logs. Since 2.x, a complete log encryption scheme has been introduced.

#### 1) Encryption Algorithm Description

BqLog uses **RSA2048 + AES256** hybrid encryption:

- Only applicable to `CompressedFileAppender`;
- Use RSA2048 key pair generated by `ssh-keygen`:
  - Public Key: OpenSSH `ssh-rsa ...` text (essentially OpenSSH expression of PKCS#8 public key);
  - Private Key: PEM format, `-----BEGIN RSA PRIVATE KEY-----` or `-----BEGIN OPENSSH PRIVATE KEY-----` block (compatible with PKCS#1/PKCS#8 private key representation);
- When writing logs:
  - Encrypt randomly generated AES256 symmetric key using public key;
  - Actual log content is encrypted via AES256;
  - Overall encoding is BqLog custom encryption compression format.

Therefore, from cryptography and standard format perspective:

- **Public Key**: OpenSSH `ssh-rsa` text, underlying is PKCS#8 compatible RSA public key;
- **Private Key**: PEM encoded RSA private key (PKCS#1 or PKCS#8), BqLog tool is compatible with `ssh-keygen` default output when parsing.

#### 2) Configure Encryption

Execute in terminal:

```bash
ssh-keygen -t rsa -b 2048 -m PEM -N "" -f "Your Key File Path"
```

Will generate two files:

- `<Your Key File Path>`: Private key file, such as `id_rsa`, usually starting with `-----BEGIN RSA PRIVATE KEY-----`;
- `<Your Key File Path>.pub`: Public key file, such as `id_rsa.pub`, starting with `ssh-rsa `.

Please ensure:

- Public key content starts with `ssh-rsa `;
- Private key content is standard PEM block.

Add in corresponding CompressedFileAppender configuration:

```properties
appenders_config.{AppenderName}.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname
```

Where value is full line content in public key file (trailing newline can be removed, but do not break spaces in between).

Thus, log content output by this Appender will be format using mixed encryption of above RSA2048 public key + AES256 symmetric key.

#### 3) Decrypt Logs

When decrypting encrypted logs, private key file is required. Recommended to use BqLog built-in command line tool `BqLog_LogDecoder`:

- Usage see [Offline decoder for binary Appenders](#offline-decoder-for-binary-appenders).

Example:

```bash
./BqLog_LogDecoder FileToDecode -o OutputFile -k "./YourPrivateKeyFilePath"
```

Where:

- `FileToDecode`: Compressed encrypted log file path;
- `-o OutputFile`: Optional, specify decoded text save path, output to standard output if not filled;
- `-k "./YourPrivateKeyFilePath"`: Point to private key file (PEM) generated by `ssh-keygen`, supporting common PKCS#1 / PKCS#8 forms.

---

## Benchmark

### 1. Benchmark description

Test Environment:

- **CPU**: 13th Gen Intel(R) Core(TM) i9-13900K @ 3.00 GHz
- **Memory**: 128 GB
- **OS**: Windows 11

Test Cases:

- Use 1~10 threads to write logs simultaneously;
- Each thread writes 2,000,000 log entries:
  - One with 4 parameters formatted log;
  - One with no parameter plain text log;
- Wait for all threads to finish, then call `force_flush_all_logs()`, count total time from start of writing to all logs flushed to disk.

Comparison Objects:

- BqLog 2.x (C++ / Java, TextFileAppender and CompressedFileAppender)
- Log4j2 (Only TextFileAppender, **gzip compression not used when rolling**, because its compression is done on existing text files during rolling, performance model is different, cannot be compared fairly)

### 2. BqLog C++ Benchmark code

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

    // Trigger capacity_limit with one log to make sure old log files is deleted
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

### 3. BqLog Java Benchmark code

```java
import java.util.Scanner;

/**
 * Please ensure that the dynamic library corresponding to the current platform is in java.library.path before running,
 * or configure Native Library Location in IDE pointing to dynamic library directory under (ProjectRoot)/dist.
 * Otherwise UnsatisfiedLinkError may be encountered.
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

### 4. Log4j Benchmark code

Log4j2 part only tests text output format, because its gzip compression is "re-gzip compression on existing text files during rolling", which is completely different from BqLog's real-time compression mode performance model, and cannot be directly benchmarked.

**Dependencies:**

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

Enable AsyncLogger:

```properties
# log4j2.component.properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

Log4j2 Configuration:

```xml
<!-- log4j2.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <Console name="Console" target="SYSTEM_OUT">
      <PatternLayout pattern="%d{HH:mm:ss.SSS} [%t] %-5level %logger{36} - %msg%n"/>
    </Console>

    <!-- RollingRandomAccessFile, for text output demo -->
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

Source Code:

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

        // These two tests need to run separately, because LoggerContext is no longer available after force stop.
        test_text_multi_param(thread_count);
        // test_text_no_param(thread_count);
    }

}
```

### 5. Benchmark results

All time costs are in milliseconds, smaller values mean higher performance.
From the results:

- In TextFileAppender scenario, BqLog has about **3 times** performance advantage over Log4j2;
- In CompressedFileAppender scenario, BqLog has about **10 times or more** performance advantage over Log4j2 text output;
- Compared with BqLog 1.5 version, 2.x average performance improved by about **40%**.


> For clarity, BqLog 1.5 data is not included, you can compare directly with [Benchmark](https://github.com/Tencent/BqLog/blob/stable_1.5/README.md#5-benchmark-results) results in old version documentation. Same hardware and OS environment, same test cases, same Log4j results were used. From results, BqLog 2.x version has about 40% performance improvement over 1.5 version.
#### Total Time Cost with 4 parameters (ms)

|                         | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|-------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)    | 110    | 125    | 188    | 256    | 318    | 374    | 449    | 511    | 583    | 642     |
| BqLog Text (C++)        | 344    | 699    | 1036   | 1401   | 1889   | 2211   | 2701   | 3121   | 3393   | 3561    |
| BqLog Compress (Java)   | 129    | 141    | 215    | 292    | 359    | 421    | 507    | 568    | 640    | 702     |
| BqLog Text (Java)       | 351    | 702    | 1052   | 1399   | 1942   | 2301   | 2754   | 3229   | 3506   | 3695    |
| Log4j2 Text             | 1065   | 2583   | 4249   | 4843   | 5068   | 6195   | 6424   | 7943   | 8794   | 9254    |

<img src="docs/img/benchmark_4_params.png" alt="Results of 4 params" style="width: 100%;">

#### Total Time Cost without parameters (ms)

An interesting phenomenon is that in "no parameter" case, Log4j2's time cost is slightly lower than its "with parameter" case, but still significantly slower than BqLog.

|                         | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|-------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)    | 97     | 101    | 155    | 228    | 290    | 341    | 415    | 476    | 541    | 601     |
| BqLog Text (C++)        | 153    | 351    | 468    | 699    | 916    | 1098   | 1212   | 1498   | 1733   | 1908    |
| BqLog Compress (Java)   | 109    | 111    | 178    | 240    | 321    | 378    | 449    | 525    | 592    | 670     |
| BqLog Text (Java)       | 167    | 354    | 491    | 718    | 951    | 1139   | 1278   | 1550   | 1802   | 1985    |
| Log4j2 Text             | 3204   | 6489   | 7702   | 8485   | 9640   | 10458  | 11483  | 12853  | 13995  | 14633   |

<img src="docs/img/benchmark_no_param.png" alt="Results of no param" style="width: 100%;">

---

## How to contribute

If you want to contribute code, please make sure your changes can pass the following workflows under GitHub Actions in the repository:

- `AutoTest`
- `Build`

It is recommended to run corresponding scripts locally before submitting to ensure both testing and building pass normally.