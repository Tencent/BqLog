# API 参考

[← 返回首页](../README_CHS.md) | [English](./API_REFERENCE.md)

所有语言 Wrapper 共享一致的 API 命名与语义。以下每个 API 均展示所有支持的语言。

---

## 1. 创建 Log 对象

通过 `create_log` 静态函数创建 Log。如果名称已存在，会返回已有对象并更新其配置（部分字段如 `buffer_size` 不可修改，详见 [配置说明](./CONFIGURATION_CHS.md)）。

<details open>
<summary><b>C++</b></summary>

```cpp
#include <bq_log/bq_log.h>
// bq::string 可隐式接受 char*、std::string、std::string_view
static bq::log bq::log::create_log(const bq::string& log_name, const bq::string& config_content);
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
import bq.log;
bq.log logObj = bq.log.create_log("my_log", config);
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
using bq;
bq.log logObj = bq.log.create_log("my_log", config);
```
</details>

<details>
<summary><b>TypeScript (Node.js / HarmonyOS ArkTS)</b></summary>

```typescript
import { bq } from "@pippocao/bqlog";  // Node.js ESM
// import { bq } from "bqlog";          // HarmonyOS ArkTS
const logObj = bq.log.create_log("my_log", config);
```
</details>

<details>
<summary><b>Python</b></summary>

```python
from bq.log import log
log_obj = log.create_log("my_log", config)
```
</details>

要点：

1. 任何语言中，返回值都**不会为 null**。如创建失败，可通过 `is_valid()` 判断。
2. 若 `log_name` 为空字符串，BqLog 会自动分配唯一名称，如 `"AutoBqLog_1"`。
3. 对已存在的同名 Log 调用 `create_log`，会复用原对象并覆盖配置。
4. 可在全局/静态变量中安全调用——无需担心初始化顺序问题。

---

## 2. 获取 Log 对象

通过名称获取已创建的 Log 对象：

<details open>
<summary><b>C++</b></summary>

```cpp
static bq::log bq::log::get_log_by_name(const bq::string& log_name);
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
bq.log logObj = bq.log.get_log_by_name("my_log");
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
bq.log logObj = bq.log.get_log_by_name("my_log");
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
const logObj = bq.log.get_log_by_name("my_log");
```
</details>

<details>
<summary><b>Python</b></summary>

```python
log_obj = log.get_log_by_name("my_log")
```
</details>

> 注意：请确保该 Log 已通过 `create_log` 创建过，否则 `is_valid()` 返回 false。

---

## 3. 写日志

BqLog 提供 6 个日志等级：`verbose`、`debug`、`info`、`warning`、`error`、`fatal`（重要性递增）。

格式参数使用 `{}` 占位符，遵循 `C++20 std::format` 规则（不支持排序序号与时间格式化）。**强烈建议使用格式参数而非字符串拼接**，以获得最佳性能和压缩效果。

<details open>
<summary><b>C++</b></summary>

```cpp
log_obj.info("Hello {}, count={}", "world", 42);
log_obj.error("Error code: {}", err_code);
// 所有等级: verbose, debug, info, warning, error, fatal
```

支持的 `STR` 类型：`char*`、`char16_t*`、`char32_t*`、`wchar_t*`、`std::string`、`std::u16string`、`std::wstring`、Unreal `FString` / `FName` / `FText` 等。

<a id="fast-mode"></a>

#### C++ 专属的快速模式

除了普通模式（`log_obj.info(...)` 等各级别函数），C++ 还有一个快速模式：六个宏，写出的日志和普通模式完全一样，但调用线程的开销小得多。

```cpp
BQ_LOG_FAST_INFO(log_obj, "Hello {}, count={}", "world", 42);
BQ_LOG_FAST_ERROR(log_obj, "Error code: {}", err_code);
// BQ_LOG_FAST_VERBOSE, BQ_LOG_FAST_DEBUG, BQ_LOG_FAST_INFO, BQ_LOG_FAST_WARNING, BQ_LOG_FAST_ERROR, BQ_LOG_FAST_FATAL

// 生成的 Category 日志类：同一个宏，Category 放在格式串前面，写法和 info(cat, ...) 一样
BQ_LOG_FAST_INFO(my_category_log, my_category_log.cat.Shop.Seller, "order {} paid", order_id);
```

每个调用点第一次执行时，把格式串注册一次；之后调用线程只做三件事：判断级别、写入时间戳和原始参数到本线程自己的缓冲区、返回。格式串不再拷贝也不再计算哈希，全程不经过库函数调用。

快速模式适合用在热点路径上。其他地方建议继续用普通模式，因为快速模式有代价：

- **每个调用点的日志对象、Category 和格式串固定。** 调用点会绑定第一次调用时的日志对象和格式串（Category 是编译期确定的类型，本来就固定）。之后同一个调用点传入别的日志对象或别的格式串（比如通过一个转发参数的辅助函数，把参数都送进同一行 `BQ_LOG_FAST_*`）是不支持的：日志仍然会写进第一个日志对象、用第一个格式串。配置变更（`reset_config`、级别、类别掩码）照常生效。
- **内存稍多一点。** 每个注册过的格式串在日志对象的生命周期内保存一份；每个用到快速模式的线程多占一个 128 字节的线程局部槽位。
- **IDE 代码提示体验差一些。** 这是宏，代码补全和参数提示不如普通模式的成员函数好用。参数类型仍然会在编译期检查。

支持 [Category](./ADVANCED_USAGE_CHS.md#2-支持分类category的-log-对象)：第二个参数是 `log.cat.xxx` 时按该 Category 输出，是格式串时用默认 Category。是哪一种在编译期决定，带 Category 的调用和不带的一样快。支持的参数类型和普通模式相同。如果某个级别配置了打印调用栈（`log.print_stack_levels`），或者日志对象是同步模式，快速模式的调用会自动退回普通模式，输出依然正确，只是这种情况下不会更快。
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
import static bq.utils.param.no_boxing;

logObj.info("Hello {}, count={}", "world", no_boxing(42));
logObj.error("Error code: {}", no_boxing(errCode));
// 所有等级: verbose, debug, info, warning, error, fatal
```

> **重要：** 使用 `bq.utils.param.no_boxing()` 包裹基本类型（`int`、`float`、`boolean` 等）以避免装箱和 GC 压力。不使用 `no_boxing` 时，基本类型会被自动装箱。
</details>

<details>
<summary><b>C#</b></summary>

```csharp
logObj.info("Hello {}, count={}", "world", 42);
logObj.error("Error code: {}", errCode);
// 所有等级: verbose, debug, info, warning, error, fatal
```

> 参数个数 ≤ 12 时**零装箱/拆箱**；超过 12 个参数才退化为装箱。
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
logObj.info("Hello {}, count={}", "world", 42);
logObj.error("Error code: {}", errCode);
// 所有等级: verbose, debug, info, warning, error, fatal
```

> 参数通过 NAPI 直接传递——无额外装箱。
</details>

<details>
<summary><b>Python</b></summary>

```python
log_obj.info("Hello {}, count={}", "world", 42)
log_obj.error("Error code: {}", err_code)
# 所有等级: verbose, debug, info, warning, error, fatal
```
</details>

### 支持的参数类型

- 空指针 → `null`
- 指针 → `0x` 十六进制地址
- `bool`、单/双/四字节字符
- 8/16/32/64 位整数与无符号整数
- 32 位与 64 位浮点数
- 各语言的所有字符串类型
- C# / Java：任意对象（通过 `ToString()` 输出）
- C++：POD 类型（1/2/4/8 字节）、自定义类型（见 [高级用法 — 自定义参数类型](./ADVANCED_USAGE_CHS.md#4-自定义参数类型)）

![日志等级](img/log_level.png)

---

## 4. 强制刷新

确保所有缓冲日志落盘。在程序退出前或关键检查点调用。

<details open>
<summary><b>C++</b></summary>

```cpp
bq::log::force_flush_all_logs();  // 刷新所有 log 对象
log_obj.force_flush();             // 刷新单个 log 对象
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
bq.log.force_flush_all_logs();
logObj.force_flush();
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
bq.log.force_flush_all_logs();
logObj.force_flush();
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
bq.log.force_flush_all_logs();
logObj.force_flush();
```
</details>

<details>
<summary><b>Python</b></summary>

```python
log.force_flush_all_logs()
log_obj.force_flush()
```
</details>

---

## 5. 崩溃保护

启用异常退出时自动刷新缓冲区（仅 POSIX）：

<details open>
<summary><b>C++</b></summary>

```cpp
bq::log::enable_auto_crash_handle();
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
bq.log.enable_auto_crash_handle();
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
bq.log.enable_auto_crash_handle();
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
bq.log.enable_auto_crash_handle();
```
</details>

<details>
<summary><b>Python</b></summary>

```python
log.enable_auto_crash_handle()
```
</details>

另见：[高级用法 — 程序异常退出的数据保护](./ADVANCED_USAGE_CHS.md#3-程序异常退出的数据保护) 和 [`log.recovery` 配置](./CONFIGURATION_CHS.md)。

---

## 6. 拦截 / 获取 Console 输出

### 注册回调

<details open>
<summary><b>C++</b></summary>

```cpp
static void bq::log::register_console_callback(bq::type_func_ptr_console_callback callback);
static void bq::log::unregister_console_callback(bq::type_func_ptr_console_callback callback);
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
bq.log.register_console_callback(callback);
bq.log.unregister_console_callback(callback);
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
bq.log.register_console_callback(callback);
bq.log.unregister_console_callback(callback);
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
bq.log.register_console_callback(callback);
bq.log.unregister_console_callback(callback);
```
</details>

<details>
<summary><b>Python</b></summary>

```python
log.register_console_callback(callback)
log.unregister_console_callback(callback)
```
</details>

**注意：**
1. **不要**在回调中调用任何同步刷新的 BqLog 函数——会造成死锁。
2. Unity / 团结引擎 / Unreal 插件已自动将 ConsoleAppender 重定向到编辑器日志窗口。

### 主动拉取（适用于虚拟机环境）

当不适合从原生线程直接回调到虚拟机时（C#、Java、IL2CPP、Node.js），使用缓冲拉取模式：

```cpp
// 启用缓冲（register_console_callback 和默认 console 输出将被禁用）
bq::log::set_console_buffer_enable(true);

// 在业务线程中轮询——必须持续调用，否则内存会无限增长
bq::log::fetch_and_remove_console_buffer(on_console_callback);
```

> **IL2CPP 环境：** 确保回调为 `static unsafe` 方法，并添加 `[MonoPInvokeCallback(typeof(type_console_callback))]`。

---

## 7. 运行时修改配置

<details open>
<summary><b>C++</b></summary>

```cpp
bool success = log_obj.reset_config(new_config);
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
boolean success = logObj.reset_config(newConfig);
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
bool success = logObj.reset_config(newConfig);
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
const success = logObj.reset_config(newConfig);
```
</details>

<details>
<summary><b>Python</b></summary>

```python
success = log_obj.reset_config(new_config)
```
</details>

部分字段（`buffer_size`、`thread_mode`）不可在运行时修改。详见 [配置说明](./CONFIGURATION_CHS.md)。

---

## 8. 启用 / 禁用 Appender

<details open>
<summary><b>C++</b></summary>

```cpp
log_obj.set_appender_enable("appender_name", false);  // 禁用
log_obj.set_appender_enable("appender_name", true);   // 重新启用
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
logObj.set_appender_enable("appender_name", false);  // 禁用
logObj.set_appender_enable("appender_name", true);   // 重新启用
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
logObj.set_appender_enable("appender_name", false);
logObj.set_appender_enable("appender_name", true);
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
logObj.set_appender_enable("appender_name", false);
logObj.set_appender_enable("appender_name", true);
```
</details>

<details>
<summary><b>Python</b></summary>

```python
log_obj.set_appender_enable("appender_name", False)
log_obj.set_appender_enable("appender_name", True)
```
</details>

---

## 9. 快照（Snapshot）

捕获最近日志用于异常上报。需在 [配置说明](./CONFIGURATION_CHS.md#snapshot-配置) 中启用 `snapshot`。

<details open>
<summary><b>C++</b></summary>

```cpp
bq::string snapshot = log_obj.take_snapshot("UTC+8");
// bq::string 可隐式转换为 std::string
```
</details>

<details>
<summary><b>Java / Kotlin</b></summary>

```java
String snapshot = logObj.take_snapshot("UTC+8");
```
</details>

<details>
<summary><b>C#</b></summary>

```csharp
string snapshot = logObj.take_snapshot("UTC+8");
```
</details>

<details>
<summary><b>TypeScript</b></summary>

```typescript
const snapshot: string = logObj.take_snapshot("UTC+8");
```
</details>

<details>
<summary><b>Python</b></summary>

```python
snapshot = log_obj.take_snapshot("UTC+8")
```
</details>

---

## 10. 解码二进制日志文件

运行时解码 CompressedFileAppender 日志：

```cpp
// 仅 C++ —— 其他语言使用离线命令行工具
bq::tools::log_decoder decoder("path/to/file.logcompr", "optional_private_key");
while (decoder.decode() == bq::appender_decode_result::success) {
    auto& text = decoder.get_last_decoded_log_entry();
    // 处理 text...
}
// 或一次性解码整个文件：
bq::tools::log_decoder::decode_file("input.logcompr", "output.txt", "optional_key");
```

### 离线命令行解码器

从 Releases 下载 `{os}_{arch}_tools_{version}`：

```bash
./BqLog_LogDecoder 要解码的文件 [-o 输出文件] [-k 私钥文件]
```

> **注意：** 不同版本的 BqLog 之间二进制格式可能不兼容，请使用匹配版本的解码器。
> 另见：[高级用法 — 日志加密和解密](./ADVANCED_USAGE_CHS.md#6-日志加密和解密)

---

## 同步日志和异步日志

通过 [配置说明](./CONFIGURATION_CHS.md) 中的 `log.thread_mode` 控制。

|                    | **同步** (`sync`) | **异步** (`async` / `independent`) |
|:------------------:|---|---|
| **行为** | 日志在函数返回前立刻处理完毕 | 日志写入缓冲区，由工作线程稍后处理 |
| **性能** | 较低——调用线程阻塞等待输出完成 | 较高——调用线程立刻返回 |
| **线程安全** | 调用期间参数不被修改即安全 | 同样保证；内部使用高并发环形缓冲区 |

### 线程安全说明

BqLog **在调用期间将所有参数完整拷贝到内部环形缓冲区**。函数返回后，数据已安全存储。工作线程不会访问调用方栈。

唯一不安全的场景是**调用过程中其他线程修改了参数**：

```cpp
static std::string global_str = "hello";  // 其他线程正在并发修改

void thread_a() {
    log_obj.info("param: {}", global_str);  // 如果 global_str 在此调用期间被修改，结果未定义
}
```

**原则：确保单次日志调用中传入的参数在调用期间不被修改**，无论同步/异步。

---

## Appender 介绍

| 名称 | 输出目标 | 可读 | 性能 | 尺寸 | 加密 |
|------|---------|------|------|------|------|
| ConsoleAppender | 控制台 / ADB Logcat | Yes | 低 | - | No |
| TextFileAppender | UTF-8 文本文件 | Yes | 低 | 大 | No |
| CompressedFileAppender | 二进制压缩文件 | No | **高** | **小** | **Yes**（RSA2048 + AES256）|

> **CompressedFileAppender** 是推荐的默认文件输出格式。

---

## 构建说明

所有构建脚本位于 `/build`：

```text
/build
├── demo       # Demo 构建
├── lib        # Native 静态/动态库构建
├── test       # 测试工程构建
├── tools      # 工具（LogDecoder、CategoryLogGenerator 等）
├── wrapper    # 各语言 Wrapper 工程（Java / C# 等）
├── benchmark  # Benchmark 工程构建
└── plugin     # 游戏引擎插件（Unity / 团结引擎 / Unreal）
```

**所需环境变量：**
- `ANDROID_NDK_ROOT` — Android 构建
- `JAVA_HOME` — Java wrapper（大多数脚本默认开启）
