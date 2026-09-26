# 高级用法

[← 返回首页](../README_CHS.md) | [English](./ADVANCED_USAGE.md)

---

<a id="1-无-heap-alloc"></a>

## 1. 无 Heap Alloc

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

<a id="2-支持分类category的-log-对象"></a>

## 2. 支持分类（Category）的 Log 对象

### Category 概念与使用

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

配合[配置说明](./CONFIGURATION_CHS.md)中的 `categories_mask`，可以在输出侧进行灵活过滤。
结合 [API 参考](./API_REFERENCE_CHS.md) 中的 Console 回调，您可以通过 `category_idx` + 下述 API 获取 Category 名称列表：

```cpp
decltype(categories_name_array_)::size_type get_categories_count() const;
const bq::array<bq::string>& get_categories_name_array() const;
```

这常用于在自定义 UI 中展示多维过滤器。

### Category 日志类生成

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

<a id="3-程序异常退出的数据保护"></a>

## 3. 程序异常退出的数据保护

当 BqLog 使用异步模式时，如果程序非正常退出（崩溃等），可能造成缓冲中的日志尚未来得及落盘。
BqLog 提供了两种机制尽力减少损失：

### 1）异常信号处理机制（POSIX）

```cpp
static void enable_auto_crash_handle();
```

调用 `bq::log::enable_auto_crash_handle()` 后，BqLog 会在 POSIX 系统上注册若干信号处理器：

- 当进程收到如 `SIGABRT`、`SIGSEGV`、`SIGBUS` 等异常信号时，尝试在信号处理回调中强制刷新缓冲区（`force_flush_all_logs`）；
- 内部通过 `sigaction` 实现，并且在注册前会保存旧的信号处理句柄，在自身处理结束后再调用原有处理逻辑，尽量降低对宿主程序的影响。

不过需要注意：

- 该机制本质上是「紧急补救」，**不能保证 100% 成功**——如果内存本身已严重破坏，任何操作都可能失败；
- 该机制仅作用于 POSIX 平台，Windows 上不会生效。

### 2）复盘机制（Recovery）

参考配置项 [`log.recovery`](./CONFIGURATION_CHS.md)。
当该项为 `true` 时，BqLog 在部分平台上会尝试使用平台特性，尽量保证异步缓冲区中的内容在磁盘上有临时存根；下一次启动时，可以进行「复盘」，尽量恢复未完全落盘的日志。

具体实现细节依赖操作系统能力，会在未来版本中持续增强。

---

<a id="4-自定义参数类型"></a>

## 4. 自定义参数类型

在 [API 参考](./API_REFERENCE_CHS.md) 中已说明，默认支持大量常见类型。
若需扩展自定义类型，有两种方式：

> **重要提示：**
> 请务必在 `bq_log.h` 或生成的 Category 头文件之前先 `#include` 您的自定义类和相关函数声明。
> 部分编译器（尤其是 Clang）在 include 顺序不正确时可能编译失败。

### 方法一：在类中实现 `bq_log_format_str_size()` 与 `bq_log_format_str_chars()`

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

### 方法二：实现全局的 `bq_log_format_str_size()` 与 `bq_log_format_str_chars()`

适用于无法改动类型定义的情况（如 Unreal 的 `FString`、`FName` 等），或希望覆盖内置类型的默认输出方式。

由于自定义类型的优先级高于内置类型，您甚至可以重定义 `int32_t` 的输出，例如：

```cpp
// custom_bq_log_type.h
#pragma once
#include <map>
#include <cinttypes>
#include <cstring>

size_t bq_log_format_str_size(const int32_t& param);
const char* bq_log_format_str_chars(const int32_t& param);

template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param);
template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param);

template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param) {
    return param.empty() ? strlen("empty") : strlen("full");
}

template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param) {
    return param.empty() ? u"empty" : u"full";
}
```

```cpp
// custom_bq_log_type.cpp
#include "custom_bq_log_type.h"

size_t bq_log_format_str_size(const int32_t& param) {
    if (param > 0) return strlen("PLUS");
    else if (param < 0) return strlen("MINUS");
    else return strlen("ZERO");
}

const char* bq_log_format_str_chars(const int32_t& param) {
    if (param > 0) return "PLUS";
    else if (param < 0) return "MINUS";
    else return "ZERO";
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

<a id="5-在-unreal-中使用-bqlog"></a>

## 5. 在 Unreal 中使用 BqLog

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

### 2）将 BqLog 的输出转接到 Unreal 日志窗口

如果已按 [集成指南 — Unreal Engine](./INTEGRATION_GUIDE_CHS.md) 引入 Unreal 插件，BqLog 日志会自动转接到 Unreal 的 Output Log 中。
若未使用插件，而是直接在 C++ 级别集成 BqLog，可以自行使用 console 回调进行转发：

```cpp
static void on_bq_log(uint64_t log_id, int32_t category_idx,
                      int32_t log_level, const char* content, int32_t length)
{
    switch (log_level)
    {
    case (int32_t)bq::log_level::verbose:
        UE_LOG(LogTemp, VeryVerbose, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    case (int32_t)bq::log_level::debug:
        UE_LOG(LogTemp, Verbose, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    case (int32_t)bq::log_level::info:
        UE_LOG(LogTemp, Log, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    case (int32_t)bq::log_level::warning:
        UE_LOG(LogTemp, Warning, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    case (int32_t)bq::log_level::error:
        UE_LOG(LogTemp, Error, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    case (int32_t)bq::log_level::fatal:
        UE_LOG(LogTemp, Fatal, TEXT("%s"), UTF8_TO_TCHAR(content)); break;
    default: break;
    }
}

void CallThisOnYourGameStart()
{
    bq::log::register_console_callback(&on_bq_log);
}
```

### 3）在蓝图中使用 BqLog

已按 [集成指南 — Unreal Engine](./INTEGRATION_GUIDE_CHS.md) 引入插件后，可在蓝图中直接调用 BqLog：

1. **创建日志 Data Asset**
  - 在 Unreal 工程中创建 Data Asset，类型选择 BqLog：
    - 默认 Log 类型（不带 Category）：
      <img src="img/ue_pick_data_asset_1.png" alt="默认Log创建" style="width: 455px">
    - 若生成了带 Category 的日志类，并将 `{category}.h` 与 `{category}_for_UE.h` 加入工程：
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

<a id="6-日志加密和解密"></a>

## 6. 日志加密和解密

对于外发客户端（尤其是互联网游戏和 App），日志加密是重要需求。
在 1.x 版本中，BqLog 的二进制日志中仍有大量明文。自 2.x 起，引入了完整的日志加密方案。
当前格式采用下述自定义 `rsa_aes_xor` 方案，运行成本与安全性质应分别评估。

### 1）加密算法说明

BqLog 当前组合使用 **RSA-2048、AES-256-CBC 和循环 XOR 载荷变换**：

- 仅适用于 `CompressedFileAppender`；
- 使用 `ssh-keygen` 生成的 RSA2048 密钥对：
  - 公钥：OpenSSH `ssh-rsa ...` 文本；
  - 与下面示例配套的 PEM RSA 私钥可通过 `ssh-keygen -m PEM` 生成；可接受的私钥容器以解码器说明为准；
- 日志写入时：
  - 每个加密段准备 AES-256 密钥和 IV，用 RSA-2048 保护 AES 密钥；
  - AES-CBC 加密一个 32 KiB 掩码块，随段的密钥材料保存；
  - payload 按文件位置与该掩码块循环异或，并非每条记录正文直接由 AES 加密。

这个自定义格式没有 AEAD 认证标签。重复使用的掩码不具备一次一密条件，也不能从 RSA/AES 名称推导出标准认证加密的保证。不要据此宣称防篡改或所有负载下开销都可忽略。精确布局与执行路径见[当前文件格式文章](<文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>)和 `appender_file_binary.cpp`。

### 2）配置加密

在终端执行：

```bash
ssh-keygen -t rsa -b 2048 -m PEM -N "" -f "你的密钥文件路径"
```

将生成两份文件：

- `<你的密钥文件路径>`：私钥文件，如 `id_rsa`，通常以 `-----BEGIN RSA PRIVATE KEY-----` 开头；
- `<你的密钥文件路径>.pub`：公钥文件，如 `id_rsa.pub`，以 `ssh-rsa ` 开头。

在对应的 CompressedFileAppender 配置中加入：

```properties
appenders_config.{AppenderName}.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname
```

### 3）解密日志

解密加密日志时，需要私钥文件。推荐使用 BqLog 自带命令行工具 `BqLog_LogDecoder`：

- 使用方法参见 [API 参考 — 离线解码](./API_REFERENCE_CHS.md#离线解码二进制格式的-appender)。

示例：

```bash
./BqLog_LogDecoder 要解码的文件 -o 输出文件 -k "./你的私钥文件路径"
```

其中：

- `要解码的文件`：压缩加密日志文件路径；
- `-o 输出文件`：可选，指定解码后的文本保存路径，不填则输出到标准输出；
- `-k "./你的私钥文件路径"`：指向 `ssh-keygen` 生成的私钥文件（PEM），支持常见 PKCS#1 / PKCS#8 形式。
