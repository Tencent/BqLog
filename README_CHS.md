# BqLog (扁鹊日志) (V 2.0.1) ([ChangeLog](/CHANGELOG.md))
[![license](https://img.shields.io/badge/license-APACHE2.0-brightgreen.svg?style=flat)](https://github.com/Tencent/BqLog/blob/main/LICENSE.txt)
[![Release Version](https://img.shields.io/badge/release-2.0.1-red.svg)](https://github.com/Tencent/BqLog/releases)

> BqLog是一个轻量级，高性能的工业级日志系统，应用于《王者荣耀》等项目，已经上线并良好运行。  
> **BqLog 2.0.1 版本正式发布！带来了纯血鸿蒙支持、Node.js支持、性能更强的并发模式以及高强度的非对称混合加密方案。**

## 如果您有以下困扰，可以尝试BqLog
- 如果您的客户端产品（尤其是游戏）希望突破以下不可能三角。
  - 方便追述问题（日志应写尽写）
  - 性能足够好（日志少写）
  - 节约存储空间（日志最好就别写）
- 如果您是后台服务开发者，现有的日志库性能无法满足**高并发场景**下的需求，导致日志丢失或者程序阻塞。
- 如果您的编程语言是C++， Java， C#， Kotlin， Typescript， Javascript之一或者同时使用多种语言，想要一个跨语言的统一日志解决方案。

## 支持操作系统和平台
- Windows 64 bit
- MacOS
- Linux(包含嵌入式Linux)
- iOS(包括iPhone，iPad，Apple Vision， Apple Watch等所有苹果设备和模拟器)
- Android
- HarmonyOS(包括手机，平板，智能屏等所有鸿蒙设备和模拟器)
- Unix(在FreeBSD, NetBSD, OpenBSD, Solaris, DragonFly, Omins通过测试)

## 支持编程语言及开发环境
- C++(C++11及以上, MSVC, Clang, GCC, MinGW-GCC, MinGW-Clang)
- Java / Kotlin (Android & Server)
- C# (Unity, 团结引擎, .NET)
- **ArkTS / C++ (HarmonyOS)**
- **JavaScript / TypeScript (Node.js, CJS或者ESM)**
- **Unreal Engine (UE4 & UE5)**

## 支持的硬件架构
- X86
- X86_64
- ARM32
- ARM64
-
## 支持的引入方式
- 动态库
- 静态库
- 源代码

## 特点
- 对比现有开源日志库有巨大的性能优势（见[Benchmark](#5-benchmark结果)），不仅适用于服务器，客户端，也非常适合移动端设备
- 内存消耗少，在[Benchmark](#benchmark)的用例中，10线程20000000条日志，BqLog本身内存消耗在1M左右。
- 提供高性能高压缩比的实时压缩日志格式
- 用小于10%的性能损耗为代价，提供高强度的非对称加密日志，保护日志内容安全（可选）
- 可以在游戏引擎（`Unity`， `Unreal`）中正常使用，其中对[Unreal提供了蓝图和常用类型的支持](#6-在unreal中使用bqlog)
- 支持`utf8`, `utf16`, `utf32`的字符和字符串，支持bool,float,double，各种长度和类型的整数等常用参数类型
- 支持`C++20`的`format`规范
- 异步日志支持Crash后复盘，避免丢失数据
- 尺寸小巧，Android编译后进包动态库仅有300k左右
- 在Java和C#和Typescript上可以不额外产生Heap Alloc，不会随着运行不停new对象。
- 仅依赖标准C语言库和平台API，可以在安卓的`ANDROID_STL = none`的模式下通过编译
- 支持`C++11`及以后的编译标准，可以在极端严格的编译参数下工作
- 编译模块基于`Cmake`，并提供不同平台的编译脚本，使用方便
- 支持自定义参数类型
- 对代码提示非常友好

## 技术文章
- **[为何BqLog如此快 - 高性能实时压缩日志格式](/docs/文章1_为何BqLog如此快%20-%20高性能实时压缩日志格式.MD)**
- **[为何BqLog如此快 - 高性能环形队列](/docs/文章2_为何BqLog如此快%20-%20高并发环形队列.MD)**


## 目录
**[从1.x版本升级到2.x版本的变化](#从1x版本升级到2x版本的变化)**
**[将BqLog引入您的项目](#将bqlog引入您的项目)**  
**[简单的Demo](#简单的demo)**  
**[架构介绍](#架构介绍)**  
**[主流程API使用说明](#主流程API使用说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1-创建log对象](#1-创建log对象)  
[&nbsp;&nbsp;&nbsp;&nbsp;2-获取log对象](#2-获取log对象)  
[&nbsp;&nbsp;&nbsp;&nbsp;3-写日志](#3-写日志)  
[&nbsp;&nbsp;&nbsp;&nbsp;4-其他api](#4-其他api)   
**[同步日志和异步日志](#同步日志和异步日志)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 异步日志的线程安全性说明](#异步日志的线程安全性说明)  
**[Appender介绍](#appender介绍)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. ConsoleAppender](#consoleappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. TextFileAppender](#textfileappender)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. （重点推荐）CompressedFileAppender](#compressedfileappender)  
**[配置说明](#配置说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 完整示例](#1-完整示例)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. 详细解释](#2-详细解释)  
**[离线解码二进制格式的Appender](#离线解码二进制格式的appender)**  
**[构建说明](#构建说明)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 库构建](#1-库构建)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. Demo构建和运行](#2-demo构建和运行)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. 自动测试运行说明](#3-自动测试运行说明)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Benchmark运行说明](#4-benchmark运行说明)  
**[高级使用话题](#高级使用话题)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. 无Heap Alloc](#1-无heap-alloc)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. 支持分类（Category）的Log对象](#2-支持分类category的log对象-)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. 程序异常退出的数据保护](#3-程序异常退出的数据保护)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. 关于NDK和ANDROID_STL = none相关](#4-关于ndk和android_stlnone相关)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. 自定义参数类型](#5-自定义参数类型)  
[&nbsp;&nbsp;&nbsp;&nbsp;6. 在Unreal中使用BqLog](#6-在unreal中使用bqlog)
[&nbsp;&nbsp;&nbsp;&nbsp;7. 日志加密和解密](#7-日志加密和解密)  
**[Benchmark](#benchmark)**  
[&nbsp;&nbsp;&nbsp;&nbsp;1. Benchmark说明](#1-benchmark说明)  
[&nbsp;&nbsp;&nbsp;&nbsp;2. BqLog C++ Benchmark代码](#2-bqlog-c-benchmark-代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;3. BqLog Java Benchmark代码](#3-bqlog-java-benchmark-代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;4. Log4j Benchmark代码](#4-log4j-benchmark代码)  
[&nbsp;&nbsp;&nbsp;&nbsp;5. Benchmark结果](#5-benchmark结果)  
**[如何贡献代码](#如何贡献代码)**


## 从1.x版本升级到2.x版本的变化
1. 增加对鸿蒙系统的支持，包括ArkTS和C++两种语言
2. 增加对NodeJs的支持（CJS和ESM）
3. 增加跨平台兼容性、稳定性、泛用性。能支持更多Unix系统
4. 性能有40%提升
5. 安卓不再要求必须和Java一起使用
6. 去除is_in_sandbox配置，用base_dir_type代替。 对snapshot增加过滤配置， 支持每次启动新开日志文件。参见**[配置说明](#配置说明)**
7. 支持高性能非对称加密。 参见[&nbsp;&nbsp;&nbsp;&nbsp;7. 日志加密和解密](#7-日志加密和解密)
8. 提供Unity，团结引擎，Unreal引擎的插件，方便在游戏引擎中使用，提供ConsoleAppender对游戏引擎编辑器日志输出重定向，提供Unreal蓝图支持。 参见[&nbsp;&nbsp;&nbsp;&nbsp;6. 在Unreal中使用BqLog](#6-在unreal中使用bqlog)
9. 仓库不再包含二进制产物，2.x版本开始请从Release页面下载对应平台和语言的二进制包。


## 将BqLog引入您的项目

### C++ (动态库/静态库/源码)
- **动态库**：下载Release页面中的预编译库，请将`dynamic_lib/include`目录添加到头文件搜索路径，并链接`dynamic_lib/lib`中的动态库文件。
- **静态库**，下载Release页面中的预编译库，请将`static_lib/include`目录添加到头文件搜索路径，并链接`static_lib/lib`对应的静态库文件。
- **源码集成**：将代码仓库下的`/src`目录加入源码编译。并将`/include`目录添加到头文件搜索路径。
  - Windows下Visual Studio请添加编译选项 `/Zc:__cplusplus`。
  - Android模式下支持 `ANDROID_STL=none`。
  - 如果要定义Java和NAPI的支持，以及对于系统链接库和一些宏的定义，请参考 `/src/CMakeLists.txt`。
  - 如果有NAPI环境（Nodejs或HarmonyOS ArkTS）或者Java和C#调用需求，不推荐用`C++`源码接入，因为需要手动做一些初始化调用和修改库载入逻辑。

### C# (Unity / 团结引擎 / .NET)
- **Unity**：下载 `unity_package_{version}`。 解压后在Unity的package manager中选择添加tarball， 选择其中的tar文件导入(官方引擎无鸿蒙支持，需要手动DIY)。
- **团结引擎**：下载 `tuanjie_package_{version}`。 解压后在Unity的package manager中选择添加tarball， 选择其中的tar文件导入。和Unity的区别主要是集成了鸿蒙的二进制库支持
- **.NET**：先下载对应平台动态库`{os}_{arch}_libs_{version}`，引入其中动态库， 然后将仓库下`/wrapper/csharp/src` 源码加入工程。

### Java / Kotlin (Android / Server)
- **Android**：下载`android_libs_{version}`， 直接引入其中的`.aar` 包或手动引入`.so` + 仓库下`/wrapper/java/src` 源码（可选）。
- **Server**：先下载对应平台动态库`{os}_{arch}_libs_{version}`，引入其中动态库， 然后下载`java_wrapper_{version}`，引入其中jar包或将仓库下`/wrapper/java/src` 源码加入工程。

### HarmonyOS (ArkTS / C++)
- 下载 `harmony_os_libs_{version}`
- 引入 `har` 包或者直接引入其中的`.so` + 仓库下`/wrapper/typescript/src` 源码（可选）
- 支持在ArkTS侧直接调用，也支持在Native C++侧调用。

### Node.js
- 支持 CommonJS 和 ESM。
- 通过下载Release页面中的`nodejs_npm_{version}`包，解压后找到其中的bqlog-{version}.tgz进行 npm 安装：
  ```bash
  npm install ./bqlog-{version}.tgz
  ```

### Unreal Engine
- **Prebuilt版**：下载Release页面中的`unreal_plugin_prebuilt_{version}`，解压后根据自己的引擎版本，找到其中的压缩包，解压到游戏项目的Plugins目录下(官方引擎无鸿蒙支持，需要手动DIY)。
- **源码版**：下载Release页面中的`unreal_plugin_source_{version}`，解压后根据自己的引擎版本，找到其中的压缩包，解压到游戏项目的Plugins目录下。

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

![基础结构](docs/img/log_structure.png)


上图能够清晰为您介绍BqLog的基本结构。图中右边部分`BqLog Core Engine`是BqLog库内部实现，左边的是您的程序和代码。您的程序可以通过BqLog提供的`BqLog Wrapper API`（可以理解成给不同语言用的面向对象的API）来调用BqLog。
图中总共创建了三个Log，一个的名字叫做`Log A`，另外两个叫`Log B`和`Log C`，每个Log后面都挂载了一个或者多个Appender。Appender可以理解为日志内容的输出目标，可以是控制台（Android对应ADB Logcat日志），文本文件，也能是一些特殊格式的文件，比如压缩日志文件。

**同一个进程内，不同语言的wrapper可以访问同一个Log对象，比如图中的Android应用在`Java`侧创建了一个叫Log A的Log对象，也可以在NDK C++侧通过`Log A`这个名字获取到这个Log对象进行使用**  
在一些极限的情况下，比如一个Unity开发的游戏，运行在Android系统上，会在一个APP内同时涉及到Java、Kotlin，C#和C++语言。他们完全可以共享同一个Log对象。您可以在任意一种语言侧调用create_log创建它，然后在其他语言调用get_log_by_name来获取这个Log对象使用。


<br><br>

## 主流程API使用说明

**注意，以下API都声明在bq::log(或者bq.log)类里面。为了节约篇幅，就只列C++的API，`Java`和`C#`,`typescript`等其他wrapper都有一模一样的API，就不重复浪费篇幅了**  
**C++这里的`bq::string`是bqLog库里的utf8字符串类型，也可以传入char*这样的c-style string或者是`std::string`和`std::string_view`，都能完成自动的隐式转换。**

### 1. 创建Log对象
log对象可以通过create_log这个静态函数进行创建。其声明如下：

```cpp
    /// <summary>
    /// Create a log object
    /// </summary>
    /// <param name="log_name">If the log name is an empty string, bqLog will automatically assign you a unique log name. If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.</param>
    /// <param name="config_content">Log config string</param>
    /// <returns>A log object, if create failed, the is_valid() method of it will return false</returns>
    static log create_log(const bq::string& log_name, const bq::string& config_content);
```

代码通过传入一个log对象的名称和一个配置字符串，完成了一个日志的创建。其中日志的配置可以参考[配置说明](#配置说明)
这里要关注几个点：
1. 不管是C#还是Java，返回的log对象永远不会是null，但是有可能由于配置错误等原因，会生成一个无效的log对象，所以针对返回对象，要用is_valid()函数进行一次判断。如果是无效的对象，对其进行操作可能会造成程序崩溃。
2. 如果log名称传入空字符串，那么bqLog会自动给他生成一个不重复的日志名，类似"AutoBqLog_1"这种。
3. 如果对一个已经存在的重名的log对象调用create_log，并不会创建新的log对象，而是会用新的config去覆盖之前的config，但是中间有些参数是无法被修改的，详细见[配置说明](#配置说明)
4. 可以在全局变量或者静态变量中直接通过该API初始化log对象，不用担心初始化顺序问题(Static Initialization Order Fiasco)、析构顺序问题或者多线程问题。


### 2. 获取Log对象
如果log对象是在其他地方已经创建过了，可以直接通过get_log_by_name函数获得已经创建的log对象
```cpp
//C++版本API
    /// <summary>
    /// Get a log object by it's name
    /// </summary>
    /// <param name="log_name">Name of the log object you want to find</param>
    /// <returns>A log object, if the log object with specific name was not found, the is_valid() method of it will return false</returns>
    static log get_log_by_name(const bq::string& log_name);
```
同样可以在全局变量或者静态函数中采用该函数初始化一个log对象，不过要注意一点。请务必保证该名称的log对象已经存在，否则返回的log对象将不可用，其is_valid()会返回false。


### 3. 写日志
```cpp
    ///Core log functions, there are 6 log levels:
    ///verbose, debug, info, warning, error, fatal
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
写日志要注意三个个关键点。
#### 1. 日志等级
可以看到，我们的日志成分成了verbose，debug，info，warning，error和fatal总共六个等级，和安卓一致。其重要程度依次递增。同时如果在console中输出，会表现为不同的颜色。  
![日志等级](docs/img/log_level.png)
#### 2. format字符串（STR参数）
STR参数类似于printf的第一个参数，其类型是各种常用类型的字符串。包含：
- Java中的java.lang.String
- C#的string
- Unreal中的FName, FString, FText等
- C++中的`C Style String`和`std::string`的各种编码（`char*`, `char16_t*`, `char32_t*`, `wchar_t*`, `std::string`, `std::u8string`, `std::u16string`, `std::u32string`, `std::wstring`, `std::string_view`, `std::u16string_view`, `std::u32string_view`, `std::wstring_view`甚至是你自定义的字符串类型，自定义的字符串类型参考[自定义参数类型](#5-自定义参数类型) )

#### 3. format参数
可以在STR后面接各种参数，这些参数会被格式化到STR中指定的地方，其规则基本上和C++ 20的std::format一致（除了不支持排序序号和时间格式化等）。如只用一个{}就代表一个参数的默认格式化方式，{.2f}代表浮点数的格式化精度等。  
**请尽量用format参数的形式去输出日志，而不要自己去拼接字符串，这样对于性能和压缩格式存储是最优解**  
目前支持的参数类型包含
- 空指针（输出null)
- 指针（输出0x开头的16进制地址）
- bool
- 单字节字符(char)
- 双字节字符(char16_t,wchar_t，C#的char， Java的char)
- 四字节字符(char32_t或wchar_t)
- 8位整数
- 8位无符号整数
- 16位整数
- 16位无符号整数
- 32位整数
- 32位无符号整数
- 64位整数
- 64位无符号整数
- 32位浮点数
- 64位浮点数
- C++其他不认识的POD类型（但是尺寸只能是1,2,4,8字节，会被当成int8，int16，int32和int64处理）
- 字符串，和上面[STR参数](#2-format字符串str参数)一样的各种字符串类型
- C#和Java的任何类和对象(会输出他们的ToString()字符串)
- 自定义参数类型，参考[自定义参数类型](#5-自定义参数类型)

### 4. 其他API
还有一些常用API，可以完成一些特殊的作用，具体可以参考bq_log/bq_log.h，以及Java和C#,typescript等Wrapper的bq.log类，里面都有详细的API说明。  
这里对一些重点需要介绍的API做一个说明

#### 异常退出保护
```cpp
    /// <summary>
    /// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk. 
    /// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However, 
    /// this functionality does not guarantee success, and only support POSIX systems.
    /// </summary>
    static void enable_auto_crash_handle();
```
详细介绍见[程序异常退出的数据保护](#3-程序异常退出的数据保护)

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
由于bqLog默认情况下是异步日志，所以有时候想要立即同步输出所有日志需要强制调用一次force_flush()。

#### 拦截console输出
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
[ConsoleAppender](#consoleappender)的输出是控制台，在android是ADB Logcat日志，但是这些无法涵盖所有的情况。比如自研游戏引擎，自研IDE等，这里提供了一种机制，可以让每一条console日志输出都调用一次参数里的回调，你可以在自己的程序里任意地方重新处理和输出这个控制台日志。  
***注意:***
1. 不要在console callback中再去输出任何同步刷新的扁鹊日志，不然很容易造成死锁
2. Unity引擎，团结引擎和Unreal引擎如果是通过插件的形式引入，则不需要调用这个接口，在其插件中已经将ConsoleAppender的输出重定向到了编辑器的日志窗口中。

#### 主动获取console的输出
```cpp
    /// <summary>
    /// Enable or disable the console appender buffer.
    /// Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke callbacks from a native thread,
    /// we can enable this option. This way, all console outputs will be saved in the buffer until we fetch them.
    /// </summary>
    /// <param name="enable"></param>
    /// <returns></returns>
    static void set_console_buffer_enable(bool enable);

    /// <summary>
    /// Fetch and remove a log entry from the console appender buffer in a thread-safe manner.
    /// If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry.
    /// Please ensure not to output synchronized BQ logs within the callback function.
    /// </summary>
    /// <param name="on_console_callback">A callback function to be invoked for the fetched log entry if the console appender buffer is not empty</param>
    /// <returns>True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.</returns>
    static bool fetch_and_remove_console_buffer(bq::type_func_ptr_console_callback on_console_callback);
```
除了用console callback去拦截console的输出之外，还可以通过主动调用去获取日志的console输出。有的时候，我们并不希望这个console的日志输出是通过callback调用过来的，因为你并不知道callback会通过什么线程过来。  
这里采用的方法是通过`set_console_buffer_enable`先启用console的缓冲功能，每一条console日志输出都会被留在内存中，直到我们主动调用`fetch_and_remove_console_buffer`将它取出来。所以如果使用这种方法，请一定记得及时去获取和清理日志，不然内存会无法释放。
***注意:*** 如果您是在IL2CPP的环境中使用这个代码，请保证on_console_callback 是static unsafe的，并且加上了[MonoPInvokeCallback(typeof(type_console_callback))]这样的Attribute。


#### 修改log的配置
```cpp
    /// <summary>
    /// Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
    /// </summary>
    /// <param name="config_content"></param>
    /// <returns></returns>
    bool reset_config(const bq::string& config_content);
```
有时候希望在程序中对一个log的配置做一些修改，除了重新创建这个log对象去覆盖配置之外（参考[创建log对象](#1-创建log对象))，也可以调用这个reset接口，但是要注意，不是所有的配置内容都能被修改的，详细见[配置说明](#配置说明)

#### 临时禁用和启用某些Appender
```cpp
    /// <summary>
    /// Temporarily disable or enable a specific Appender.
    /// </summary>
    /// <param name="appender_name"></param>
    /// <param name="enable"></param>
    void set_appender_enable(const bq::string& appender_name, bool enable);
```
默认情况下配置中的Appender都是会生效的，但是这里提供了一种机制可以临时禁用和重新启用它们。

#### 输出快照
```cpp
      /// <summary>
      /// Works only when snapshot is configured.
      /// It will decode the snapshot buffer to text.
      /// </summary>
      /// <param name="time_zone_config">Use this to specify the time display of log text. such as : "localtime", "gmt", "Z", "UTC", "UTC+8", "UTC-11", "utc+11:30"</param>
      /// <returns>the decoded snapshot buffer</returns>
      bq::string take_snapshot(const bq::string& time_zone_config) const;
```
有时候有些特殊的功能，需要输出最后的一部分日志，就可以用到这个快照功能  
要启用这个功能，首先需要在日志的配置中启用snapshot，并且设置最大的缓冲大小，单位字节。 还有快照需要筛选的日志等级和category（可选） ，具体配置请参考[snapshot配置](#snapshot)。
当需要快照的时候，调用一下take_snapshot()，就会返回格式化好的快照缓冲区里存储的最后的日志内容字符串。C++里的类型是`bq::string`，可以被隐式转换为`std::string`。

#### 解码二进制日志文件
```cpp
namespace bq{
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
          /// <param name="log_file_path">the path of a binary log file, is can be relative path or absolute path</param>
          /// <param name="priv_key">private key generated by "ssh-keygen" to decrypt encrypted log file, left it to empty when log file is not encrypted.</param>
          log_decoder(const bq::string& log_file_path, const bq::string& priv_key = "");
          ~log_decoder();
          /// <summary>
          /// Decode a log entry. each call of this function will decode only 1 log entry
          /// </summary>
          /// <returns>decode result, appender_decode_result::eof means the whole log file was decoded</returns>
          bq::appender_decode_result decode();
          /// <summary>
          /// get the last decode result
          /// </summary>
          /// <returns></returns>
          bq::appender_decode_result get_last_decode_result() const;
          /// <summary>
          /// get the last decode log entry content
          /// </summary>
          /// <returns></returns>
          const bq::string& get_last_decoded_log_entry() const;
          /// <summary>
          /// Direct decode a log file to a text file.
          /// </summary>
          /// <param name="log_file_path"></param>
          /// <param name="output_file"></param>
          /// <param name="priv_key">private key generated by "ssh-keygen" to decrypt encrypted log file, left it to empty when log file is not encrypted.</param>
          /// <returns>success or not</returns>
          static bool decode_file(const bq::string& log_file_path, const bq::string& output_file, const bq::string& priv_key = "");
      };
  }
}
```
这是一个工具类，可以在运行时解码二进制类的Appender输出的日志文件，比如[CompressedFileAppender](#compressedfileappender)。
使用方式是先创建一个log_decoder对象，然后每调用一次decode()函数可以按顺序解码一条日志，如果返回结果是bq::appender_decode_result::success，则可以继续调用get_last_decoded_log_entry()返回最后解码的那条日志格式化后的文本内容。  
如果返回是bq::appender_decode_result::eof，则代表日志已经全部读取完成



## 同步日志和异步日志
BqLog可以通过配置的方式来确定一个日志对象是同步日志还是异步日志，具体配置方式参考[thread_mode](#logthread_mode)。两者主要区别如下：

| | **同步日志 (Synchronous Logging)** | **异步日志 (Asynchronous Logging)** |
|:---:|:---|:---|
| **行为 (Behavior)** | 调用日志函数之后，日志马上会保证输出到对应Appender | 调用对应日志函数之后，日志不会立刻输出，而是交给 worker 线程定期处理 |
| **性能 (Performance)** | 低，输出日志的线程需要阻塞等待日志输出到对应Appender之后才会从日志函数返回 | 高，输出日志的线程不用等待日志实际输出就会返回 |
| **线程安全性 (Thread Safety)** | 高，但是要保证当条日志的内容和参数在调用日志函数期间不会被修改 | 高，但是要保证当条日志的内容和参数在调用日志函数期间不会其他线程修改 |
### 异步日志的线程安全性说明
异步日志给人最大的误解是认为其线程安全性较差，使用者担心实际worker进行处理的时候，对应的参数已经被回收了。比如下面的情况：
```cpp
{
    const char str_array[5] = {'T', 'E', 'S', 'T', '\0'};
    const char* str_ptr = str_array;
    log_obj.info("This is test param :{}, {}", str_array, str_ptr);
}
```
上面用例`str_array`是保存在栈上的，当从作用域中退出的时候，其内存就已经变得没有意义了。所以用户会担心如果是异步日志，等worker线程实际处理的时候，`str_array`和`str_ptr`实际上已经是一个无效变量了。  
其实这样的情况是不会发生的，因为BqLog会在`info`函数被调用的过程中，就将所有的参数内容全部拷贝到内部的`ring_buffer`中，只要从`info`函数返回，就再也不需要`str_array`或者`str_ptr`这样的外部变量了。而且`ring_buffer`上保存的也不会是一个`const char*`的指针地址，而总是会将整个字符串保存在`ring_buffer`中。

实际上可能出现问题的是这样的情况。
```cpp
static std::string global_str = "hello world";   //这是一个全局变量，有多个线程在同时修改它。

void thread_a()
{
    log_obj.info("This is test param :{}", global_str);
}
```
如果在从进入.info函数到该函数返回之前，global_str的内容发生了改变，那么最后可能会出现未定义的情况。BqLog已经尽可能保证了不会发生程序崩溃，但是最后输出的内容正确性却无法保证。

<br><br>

## Appender介绍
Appender代表日志的输出目标，这里Appender的概念和Log4j的Appender概念基本是一致的。目前bqLog提供以下几种Appender
### ConsoleAppender
该Appender的输出目标是控制台，Android的ADB，以及iOS对应的控制台，其文本编码为UTF-8
### TextFileAppender
该Appender会直接用UTF-8的文本格式输出日志文件。
### CompressedFileAppender
该Appender会用压缩后的格式输出日志文件，是`bqLog重点推荐的格式`。其性能是所有Appender中最高的，同时输出的文件也是最小的。不过最终文件的读取需要解码。可以在[运行时解码](#解码二进制日志文件)，以及[离线解码](#离线解码二进制格式的appender)。

下面是几种Appender的综合对比

| 名称                    | 输出目标 | 能否明文可读 | 输出性能 | 输出尺寸 | 是否支持加密 |
|-------------------------|---------|--------|----------|----------|--------|
| ConsoleAppender         | 控制台   | ✔      | 低       | -        | ✘      |
| TextFileAppender        | 文件     | ✔      | 低       | 大       | ✘      |
| CompressedFileAppender  | 文件     | ✘      | 高       | 小       | ✔      |



<br><br>

## 配置说明
所谓配置就是create_log和reset_config函数中的config字符串，该字符串采用properties文件的格式，支持#注释符号（但是记得要单开一行并且用#开头）
### 1. 完整示例
以下是一个完整的案例
```ini
    #这个配置给log对象配置了整整5个Appender，其中有两个TextFileAppender，会输出到两个不同的文件

    #第一个Appender名叫appender_0，他的类型是ConsoleAppender
    appenders_config.appender_0.type=console
    #appender_0对应的时区是系统当地时间
    appenders_config.appender_0.time_zone=localtime
    #appender_0会输出所有6个等级的日志（注意，每个日志等级之间千万不要有空格，不然会解析失败）
    appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
            
    #第二个Appender名叫appender_1，他的类型是TextFileAppender
    appenders_config.appender_1.type=text_file
    #appender_1对应的时区是GMT时间，也就是utc0
    appenders_config.appender_1.time_zone=gmt
    #appender_1只输出info和以上的四个等级日志，其他的会被忽略
    appenders_config.appender_1.levels=[info,warning,error,fatal]
    #base_dir_type决定了暴露目录文件的基准路径，这里类型为1代表
    #如果是iOS，则会保存在/var/mobile/Containers/Data/Application/[APP]/Library/Caches下
    #如果是Android，则会保存在[android.content.Context.getExternalFilesDir()]下
    #如果是HarmonyOS，则会保存在/data/storage/el2/base/cache下
    #其他情况则在程序的当前工作目录下 
    appenders_config.appender_1.base_dir_type=1
    #appender_1保存的路径会是在程序的相对路径bqLog目录下，保存为滚动文件，文件名用normal开头，后面会跟上日期和.log扩展名，相对路径由base_dir_type决定
    appenders_config.appender_1.file_name=bqLog/normal
    #文件的最大尺寸是10000000字节，如果超过了，则会新开一个文件
    appenders_config.appender_1.max_file_size=10000000
    #超过十天的文件则会被清理
    appenders_config.appender_1.expire_time_days=10
    #该Appender的输出总尺寸超过了100000000字节则会从按日期从最早的开始清理
    appenders_config.appender_1.capacity_limit=100000000
    
    #第三个Appender名叫appender_2，他的类型是TextFileAppender
    appenders_config.appender_2.type=text_file
    #appender_2会输出所有等级的日志
    appenders_config.appender_2.levels=[all]
    #base_dir_type决定了暴露目录文件的基准路径，这里类型为0代表
    #如果是iOS，则会保存在/var/mobile/Containers/Data/Application/[APP]/Documents下
    #如果是Android，则会保存在[android.content.Context.getFilesDir()]下
    #如果是HarmonyOS，则会保存在/data/storage/el2/base/files
    #其他情况则在程序的当前工作目录下 
    appenders_config.appender_1.base_dir_type=0
    #appender_2保存的路径会是在程序的相对路径bqLog目录下，保存为滚动文件，文件名用new_normal开头，后面会跟上日期和.log扩展名
    appenders_config.appender_2.file_name=bqLog/new_normal
    
    
    #第四个Appender名叫appender_3，他的类型是CompressedFileAppender
    appenders_config.appender_3.type=compressed_file
    #appender_3会输出所有等级的日志
    appenders_config.appender_3.levels=[all]
    #appender_3保存的路径会是在程序的绝对路径~/bqLog目录下，保存为滚动文件，文件名用compress_log开头，后面会跟上日期和.logcompr扩展名
    appenders_config.appender_3.file_name=~/bqLog/compress_log
    #appender_3的输出将会被加密，使用下面的rsa2048公钥进行加密
    appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname
    
    
    #第五个Appender名叫appender_4，他的类型是ConsoleAppender
    appenders_config.appender_4.type=console
    #appender_4默认是禁用的，后续可以通过set_appenders_enable启用
    appenders_config.appender_4.enable=false
    #appender_4会输出所有等级的日志
    appenders_config.appender_4.levels=[all]
    #只有当日志的category是ModuleA,ModuleB.SystemC开头的时候，才会被处理，否则全部忽略，具体见后文详细解释（Category的概念会在后面的高级使用话题）
    appenders_config.appender_4.categories_mask=[ModuleA,ModuleB.SystemC]

    #总的异步缓存的buffer size为65535字节，具体意义看后文详细解释
    log.buffer_size=65535
    #日志支持Crash后的复盘，具体意义看后文详细解释
    log.recovery=true
    #只有当日志的category符合下面这三种通配符的时候，才会被处理，否则全部忽略，具体见后文详细解释（Category的概念会在后面的高级使用话题）
    log.categories_mask=[*default,ModuleA,ModuleB.SystemC]
    #这是一个异步日志，异步日志是性能最高的日志，也是推荐的日志类型
    log.thread_mode=async
    #如果日志等级是error和fatal的话，在每一条日志后面带上调用栈信息
    log.print_stack_levels=[error,fatal]

    #启用快照功能，快照缓存64K
    snapshot.buffer_size=65536
    #只有info和error等级的日志才会被快照记录
    snapshot.levels=[info,error]
    #只有当日志的category是ModuleA.SystemA.ClassA或者ModuleB开头的时候，才会被快照记录，否则全部忽略
    snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
```

### 2. 详细解释

### appenders_config
appenders_config是一组关于Appender的配置。其中后面接的第一个参数就是Appender的名字，所有相同名字的Appender共用一组配置。

| 名称                     | 是否必须 | 可配置值                                | 默认值            | 适用于ConsoleAppender | 适用于TextFileAppender | 适用于CompressedFileAppender | 
|------------------------|----------|-------------------------------------|----------------|-----------------------|------------------------|-----------------------------|
| type                   | ✔        | console, text_file, compressed_file |                | ✔                     | ✔                      | ✔                           |
| enable                 | ✘        | Appender是否默认启用                      | true           | ✔                     | ✔                      | ✔                           |
| levels                 | ✘        | 日志等级的任意组合数组                         | [all]          | ✔                     | ✔                      | ✔                           |
| time_zone              | ✘        | gmt,localtime,utc+8,utc-2等          | localtime,当地时间 | ✔                     | ✔                      | ✔                           |
| file_name              | ✔        | 相对路径或者绝对路径                          |                | ✘                     | ✔                      | ✔                           |
| base_dir_type          | ✘        | 0或者1                                | 0              | ✘                     | ✔                      | ✔                           | 
| max_file_size          | ✘        | 正整数或者0                              | 0              | ✘                     | ✔                      | ✔                           | 
| expire_time_seconds    | ✘        | 正整数或者0                              | 0              | ✘                     | ✔                      | ✔                           |
| expire_time_days       | ✘        | 正整数或者0                              | 0              | ✘                     | ✔                      | ✔                           |
| capacity_limit         | ✘        | 正整数或者0                              | 0              | ✘                     | ✔                      | ✔                           |
| categories_mask        | ✘        | []包围的字符串数组                          | 空              |              ✔        | ✔                      | ✔                           |
| always_create_new_file | ✘        | true或者false                         | false          |               ✘        | ✔                      | ✔                           |
| pub_key                | ✘        | rsa2048的公钥                          | 空              |               ✘        | ✘                      | ✔                           |


#### appenders_config.xxx.type
决定了该Appender的类型。
- `console代表`[ConsoleAppender](#consoleappender)
- `text_file代表`[TextFileAppender](#textfileappender)
- `compressed_file代表`[CompressedFileAppender](#compressedfileappender)
- `raw_file代表`[RawFileAppender](#rawfileappender)
#### appenders_config.xxx.enable
默认为true，如果填false的话，那么这个Appender默认是不会生效的，直到用户调用set_appenders_enable后启用。
#### appenders_config.xxx.levels
这是一个用[]包起来的数组，里面可以填入verbose,debug,info,warning,error,fatal这六个等级的任意组合，也可以直接填[all]代表所有等级都接受。**注意！不同等级之间不能有空格符号，不然会解析失败**
#### appenders_config.xxx.time_zone
代表日志的时区，"gmt", "Z", "UTC"代表使用UTC0的格林威治时间（GMT），localtime代表使用系统当地时间，其他格式可以用utc+8,utc-2，utc+11:30等形式表示对应的时区。
- 格式化出来的文本的时间显式会受到时区影响（适用于ConsoleAppender和TextFileAppender)
- 每次越过对应时区的午夜0点的时候，都会新开一个日志文件，适用于(TextFileAppender，CompressedFileAppender和RawFileAppender)。
#### appenders_config.xxx.base_dir_type
如果后面的`file_name`是一个相对路径的话，那么这个配置决定了相对路径的基准目录是什么。
* `0`
  * 安卓会依次尝试[android.content.Context.getFilesDir()], [android.content.Context.getExternalFilesDir()]和[android.content.Context.getCacheDir()]这三个目录，取第一个可用的目录作为基准目录
  * iOS会保存在`/var/mobile/Containers/Data/Application/[APP]/Documents`目录下
  * HarmonyOS会保存在`/data/storage/el2/base/files`目录下
* `1`
  * 安卓会依次尝试[android.content.Context.getExternalFilesDir()], [android.content.Context.getFilesDir()]和[android.content.Context.getCacheDir()]这三个目录，取第一个可用的目录作为基准目录
  * iOS会保存在`/var/mobile/Containers/Data/Application/[APP]/Library/Caches`目录下
  * HarmonyOS会保存在`/data/storage/el2/base/cache`目录下

#### appenders_config.xxx.file_name
保存文件的路径以及文件名前缀。其中路径可以是绝对路径（安卓和iOS最好不要用绝对路径），也可以是相对路径。最终输出的文件是用这个路径和名字再加上日期和文件的编号和对应Appender的扩展名。
#### appenders_config.xxx.max_file_size
最大文件尺寸，单位字节，当保存的文件超过这个尺寸的时候，就会生成一个新的日志文件。文件编号依次递增。0代表关闭该功能。
#### appenders_config.xxx.expire_time_seconds
滚动文件保存的最大秒数，超出秒数的文件将会被自动删除。0代表关闭该功能。
#### appenders_config.xxx.expire_time_days
文件保存的最大天数，超出天数的文件将会被自动删除。0代表关闭该功能。
#### appenders_config.xxx.capacity_limit
该输出目录下，该Appender所输出的文件的最大总尺寸，如果超出了这个尺寸，则从最早的文件开始删除，直到总大小到尺寸之内。0代表关闭该功能。
#### appenders_config.xxx.categories_mask
如果日志对象是[支持分类（Category）的Log对象](#2-支持分类category的log对象) ，可以用来做树状categories列表的过滤。当数组不为空的时候，该能力会生效。比如[*default,ModuleA,ModuleB.SystemC]这样的配置，代表着默认category(就是没有传递category参数的日志调用)的日志能被该Appender处理，ModulesA这个category和他下面的所有子category的日志都能被该Appender处理，同样还有ModuleB.SystemC这个category和他下面的所有子category的日志，其他所有的category日志都会被该Appender处理忽略。一个Appender的categories_mask生效的范围是Appender上的categories_mask和log对象上的全局[log.categories_mask](#logcategories_mask)的并集。如果您的日志是异步模式的（参考[log.thread_mode](#logthread_mode)），这个选项生效会有一点点延迟性。
#### appenders_config.xxx.always_create_new_file
如果为true，则哪怕是同一天之内，每次重新启动进程，都会新开一个日志文件，而不是继续往原来的文件里追加日志。默认是false。
#### appenders_config.xxx.pub_key
用于日志内容加密，详情见[日志加密和解密](#7-日志加密和解密)
<br>

### log
log配置是针对整个log对象的。有以下配置：

| 名称               | 是否必须 | 可配置值                     | 默认值                                | 是否可以在reset_config中修改 |
|--------------------|---------|--------------------------|------------------------------------|-----------------------------|
| thread_mode        | ✘      | sync, async, independent | async                              | ✘                          |
| buffer_size        | ✘      | 32位正整数                   | 65536(桌面，服务器平台),  <br/>32768(移动设备） | ✘                          |
| recovery     | ✘      | true或false               | false                              | ✘                          |
| categories_mask    | ✘      | []包围的字符串数组               | 空                                  | ✔                          |
| print_stack_levels | ✘      | 日志等级的任意组合数组              | 空                                  | ✔                          |
|buffer_policy_when_full| ✘      | discard, block, expand   | block                              | ✘                          |
|high_perform_mode_freq_threshold_per_second| ✘      | 64位正整数                   | 1000                               | ✘                          |

#### log.thread_mode
日志的线程模式，用户调用日志接口写入的日志会先写到日志缓存中，这里的配置代表这些缓存中的数据将在哪个线程被处理。
- `sync`，这些数据就会在当前写日志的线程被同步处理，也就是说，当您调用info一类的函数，当函数返回的时候，日志数据已经被处理完成了。
- `async`（默认），当前写日志线程不做处理立刻返回，等待工作线程去处理缓存中的日志数据。整个进程只有一个工作线程，这个工作线程会负责所有async日志的处理。
- `independent`, 当前写日志线程不做处理立刻返回，而是会开启一个专属线程专门处理这个日志对象的数据，当有很多个日志对象，而日志对象日志量又特别大的时候，可以用这个选项分担工作线程的负担。
#### log.buffer_size
日志的缓存大小，单位字节。
#### log.recovery
-`true` 如果使用默认的异步日志，并且程序意外崩溃或者没有调用force_flush()就退出程序，日志缓存中还有未处理完的日志数据，下次程序重启的时候，会把这些数据重新处理一遍，保证日志数据不丢失。该特性几乎没有性能损耗并且经过了严格测试，放心使用。
-`false` 如果使用默认的异步日志，并且程序意外崩溃或者没有调用force_flush()就退出程序，日志缓存中还有未处理完的日志数据，则数据会丢失。
#### log.categories_mask
逻辑和Appender上的[appenders_config.xxx.categories_mask](#appenders_configxxxcategories_mask)一致，不过是作用于整个log对象的。如果您的日志是异步模式的（参考[log.thread_mode](#logthread_mode)），这个选项生效会有一点点延迟性。
#### log.print_stack_levels
配置方式跟[appenders_config.levels](#appenders_configxxxlevels) 里的一样，匹配等级的每一条日志都会在后面带上调用栈的信息，但是请一定注意，最好只在Debug环境使用这个功能，正式环境最多是针对`error`和`fatal`这样的错误日志开启，因为它不仅会带来性能的明显下降，还会给Java和C#带来GC。目前`Java`, `C#`, `Win64`的栈信息显示比较清晰友好，其他平台相对较难以阅读，在没有符号表的情况下，只有地址信息。
#### log.buffer_policy_when_full
- `discard`（默认），当日志缓存满了之后，新写入的日志会被丢弃，直到有足够的空间可以写入新的日志
- `block`，当日志缓存满了之后，写日志的线程会被阻
- `expand`，（不推荐）当日志缓存满了之后，日志缓存会自动扩展一倍，直到可以写入新的日志为止。可能带来内存占用提升，由于bqLog优秀的线程调度机制，选用block模式的性能其实还高于expand。
#### log.high_perform_mode_freq_threshold_per_second
该配置项用于控制日志系统在高性能模式下的行为。当某个线程在一秒钟内记录的日志条数超过这个阈值时，该线程会自动切换到高性能模式，以优化日志处理的效率，性能提升大概在50%左右。代价是每个进入该模式的线程会额外占用`log.buffer_size`大小的内存用于高速缓存。
默认值是1000条每秒，意味着如果一个线程在一秒钟内记录超过1000条日志，该线程将进入高性能模式，一旦频率降下来了就会退出高速模式并释放内存。用户可以根据应用程序的具体需求调整这个阈值，以平衡性能和资源使用之间的关系。
填0代表关闭该功能。
注意：为了减少内存碎片，物理内存的申请是以16个高速缓存为一组申请的（移动平台比如iOS，android和鸿蒙是2个为一组），所以即使只有一个线程进入高性能模式，实际占用的内存也会是`16 * log.buffer_size`（或者`2 * log.buffer_size`）。
<br><br>


### snapshot
snapshot配置是针对整个log的快照设置，有时候有些特殊的场景，比如检测到异常的时候，需要截取一个log对象最后的一部分日志进行上报，就可以用到这个快照功能。会带来一点点额外的性能和内存开销。
有以下配置：

| 名称               | 是否必须 | 可配置值                  | 默认值 | 是否可以在reset_config中修改 |
|--------------------|---------|---------------------------|-------|----------------------------|
| buffer_size        | ✘      | 32位正整数                 | 0     | ✔                          |
| levels             | ✘      | 日志等级的任意组合数组      | [all]  | ✔                          |
| categories_mask    | ✘      | []包围的字符串数组          | 空    | ✔                          |

#### snapshot.buffer_size
快照缓存的大小，如果为0或者没配置这一项，快照功能就会被关闭
#### snapshot.levels
只有被配置在里面的日志等级的日志才会被快照记录，如果不配置，则默认是all，这一点和前面的不一样
#### snapshot.categories_mask
逻辑和Appender上的[appenders_config.xxx.categories_mask](#appenders_configxxxcategories_mask)一致，只有匹配的category，才会被快照机会，如果没有配置该选项，则全部的category都会被记录。
<br><br>

## 离线解码二进制格式的Appender
在程序执行之外，bqLog也提供了预编译好的二进制文件解码命令行工具，在Release页面下载对应操作系统和架构的软件包{os}_{arch}_tools_{version}，解压后可得可执行文件：
BqLog_LogDecoder

用法是
```cpp
./BqLog_LogDecoder 要解码的文件 [-o 输出文件][-k 私钥文件]  
```
其中输出文件目录可以不填，就会直接将解码出来的文本输出在当前命令行窗口中（**标准输出流**)
私钥文件是用来解密加密日志文件的，如果日志文件没有加密，则不需要填这个参数。详情见[日志加密和解密](#7-日志加密和解密)。
**注意，不同版本的bqLog之间的二进制文件可能会不兼容**
<br><br>

## 构建说明
针对需要自己做修改和编译的用户：  
BqLog所有的构建脚本都在/build目录下，分为  
/build  
├── demo // demo构建  
├── lib // native的静态库和动态库构建  
├── test // 测试工程构建  
├── tools // 工具工程构建  
└── wrapper // wrappers工程，Java和C#
└── bechmark // benchmark工程构建
└── plugin // 游戏引擎插件构建

### 1. 库构建
里面有不同平台构件库的脚本，要注意一下，运行之前设置一下环境变量（对应你构建的环境可能需要）：
- `ANDROID_NDK_ROOT` ： Android NDK的路径，编译Android库需要
- `JAVA_HOME` : JDK的路径，基本上所有的平台都需要（如果你确认你不用Java Wrapper，可以自己在相关脚本中删除-DJAVA_SUPPORT=ON 这个配置

### 2. Demo构建和运行
这里要注意的是C#和Java的Demo，需要自己把相关平台的动态库放到可以被程序加载的路径去

### 3. 自动测试运行说明
测试用例有生成工程的脚本和对应的直接生成+运行的脚本。

### 4. Benchmark运行说明
Benchmark有生成工程的脚本和对应的直接生成+运行的脚本。

<br><br>


## 高级使用话题
### 1. 无Heap Alloc
在Java和C#还有Typescript这类运行环境中，一般的日志库随着日志越来越多，每条日志都难免会产生少量的Heap Alloc，最终带来GC和性能下降。而BqLog在这些语言都几乎避免了这一点。一般来说Heap Alloc主要来自于以下三点：
- **函数内部的对象生成**  
  一般函数内部都会做一些字符串处理，对象的创建等，但是类似的操作在BqLog中是不存在的。
- **函数可变参数产生的Heap Array**  
  C#中通过预定义多个重载版本进行了避免
- **对象的装箱和拆箱操作**  
  装箱和拆箱操作主要会出在传递日志的格式化参数的时候，比如传递int, float等Primitive Type参数的时候。
  - 在BqLog的C# Wrapper中，如果参数个数在12个以内的时候，都不会产生装箱拆箱，只有参数超过12个的时候，才会产生。
  - Typescript由于是直接将调用转到了NAPI，所以也避免了装箱了拆箱操作。
  - Java Wrapper中，没有12个参数限制，但是要通过一些手动的代码来避免装箱拆箱操作，比如：
```java
//Java
//用bq.utils.param.no_boxing包裹起来的bool变量false就不会产生装箱拆箱操作，而裸传递的primitive type参数5.3245f就会产生装箱，带来GC。
import static bq.utils.param.no_boxing；
my_demo_log.info(my_demo_log.cat.node_2.node_5, "Demo Log Test Log, {}, {}", no_boxing(false), 5.3245f);
```


### 2. 支持分类（Category）的Log对象
#### Category 日志概念和使用
在Unreal引擎里面的日志，有Category的概念。但是对代码提示很不友好，代码书写比较麻烦。  
在BqLog里面，Category代表分类的概念，用于区分一条日志属于什么模块什么功能。同时Category是有层级的，每一个Category可以有子Category。举例来说，下面就是一个典型的Category层级：

```cpp
/*default  
├── Shop 
    ├── Manager 
    ├── Seller 
├── Factory
    ├── People 
        ├── Manager 
        ├── Worker 
    ├── Machine 
    ├── House 
├── Transport 
    ├── Vehicles
        ├── Driver
        ├── Maintenance
    ├── Trains
```

这是一个关于销售的日志对象，里面分了很多很多日志分类(Category)，下面用一个例子来说明如何使用这样的日志系统。

```cpp
my_category_log.info("Log0");  //这条日志的分类(Category)是默认分类，*default
my_category_log.info(my_category_log.cat.Shop, "Log1");  //这条日志的分类(Category)是Shop
my_category_log.info(my_category_log.cat.Shop.Seller, "Log2"); //这条日志的分类(Category)是Shop.Seller
my_category_log.info(my_category_log.cat.Transport.Vehicles.Driver, "Log3"); //这条日志的分类(Category)是Transport.Vehicles.Driver
my_category_log.info(my_category_log.cat.Factory, "Log4"); //这条日志的分类(Category)是Factory
my_category_log.info(my_category_log.cat.Factory.People, "Log5"); //这条日志的分类(Category)是Factory.People
```

最后输出的内容是
```cpp
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     Log0
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     [Shop] Log1
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     [Shop.Seller] Log2
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     [Transport.Vehicles.Driver] Log3
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     [Factory] Log4
[CategoryDemoLog]   UTC+08 2024-07-04 17:35:14.144[tid-54912 ]      [I]     [Factory.People] Log5
```

配合前面的配置里的[category_mask](#logcategories_mask)，可以给输出做过滤。
同时，如果你用[ConsoleCallback拦截](#拦截console输出)，回调里会有该条日志的Category Index，用这个参数可以配合log对象的
```cpp
        /// <summary>
        /// get log categories count
        /// </summary>
        /// <returns></returns>
        decltype(categories_name_array_)::size_type get_categories_count() const;

        /// <summary>
        /// get names of all categories
        /// </summary>
        /// <returns></returns>
        const bq::array<bq::string>& get_categories_name_array() const;
```
这两个函数，去获得对应的Category的内容，可以在一些自定义的界面上做一些复杂的过滤功能。

#### Category 日志类生成
支持分类(Category)的类不是默认的`bq::log`或者`bq.log`，是需要生成的。生成方式是使用bqLog自带的工具。  
首先，您需要准备一个文本文件把您关于所有的分类(Category)都配置好，比如这样一个文件
##### BussinessCategories.txt
```cpp
//这个配置文件支持用双斜杠来进行注释
Shop.Manager  //您不需要单独给Shop列一行，只要这样就会自动生成Shop和Shop.Manager两个Category
Shop.Seller
Factory.People.Manager
Factory.People.Worker
Factory.Machine
Factory.House
Transport.Vehicles.Driver
Transport.Vehicles.Maintenance
Transport.Trains
```

接下来，您需要用BqLog提供的命令行工具来生成对应的支持分类(Category)的日志类，，在Release页面下载对应操作系统和架构的软件包{os}_{arch}_tools_{version}，解压后可得可执行文件：
BqLog_CategoryLogGenerator

使用方式为
```cpp
./BqLog_CategoryLogGenerator 要生成的类名 配置文件 [生成的目标目录，不填就是当前目录]  
```

在本例中，如果执行
```cpp
./BqLog_CategoryLogGenerator business_log (your dir)/BussinessCategories.txt ./
```

就会在当前目录下生成五个文件
- business_log.h (C++ header wrapper)
- business_log.java (Java wrapper)
- business_log.cs (C# wrapper)
- business_log.ts (Typescript wrapper)
- business_log_for_UE.h (和business_log.h一起放到UE引擎中，可以在蓝图中引用新生成的Category)

将其引入自己的工程，就能创建对应的log对象了。以C++为例
```cpp
    bq::business_log my_log = bq::business_log::create_log("MyLog", config); 
```
或者直接获取已经创建的对象
```cpp
    bq::business_log my_log = bq::business_log::get_log_by_name("MyLog"); 
```

这个`my_log.cat`之后再接`.`符号，如果有代码提示的话，就会出现你预先配置的分类(Category)可以选择了，或者你也可以选择不用这个参数，日志就会输出为默认的空分类(Category)。



### 3. 程序异常退出的数据保护
BqLog如果是异步日志的话，难免会遇到程序在运行时发生非正常退出，而内存中的数据还没来得及输出到文件的情况。
BqLog提供了两种机制共同来保护这种情况，尽可能减少异常退出带来的损失。
#### 异常信号处理机制
```cpp
    /// <summary>
    /// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk. 
    /// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However, 
    /// this functionality does not guarantee success, and only support POSIX systems.
    /// </summary>
    static void enable_auto_crash_handle();
```
bq::Log的这个API，一旦调用之后就会启用本机制。不过该机制只支持非Windows平台。该API的作用是当程序发生异常信号时，比如SIGABORT， SIGSEGV, SIGBUS这一类的Crash的时候，会在程序推出前紧急强制把日志缓存中的数据全部处理完。  
这里有两个关键点：
- 该机制底层是用的`sigaction`，如果您的程序也用了`sigaction`，不用担心，BqLog的`sigaction`注册之前记录了上一个信号处理句柄，当他自己处理完之后，会继续重新调用之前的信号处理回调，而不会导致之前的信号处理回调被覆盖。您需要担心的是您自己的`sigaction`没有这种机制，而把BqLog的异常处理程序给覆盖掉了。
- 这是一种紧急处理机制，不敢保证百分百成功。毕竟当这个问题发生的时候，可能您的内存或者其他地方已经被写坏了。

#### 复盘机制
参考配置章节的[log.recovery](#logrecovery)，当该配置为`true`的时候，在支持的系统上，操作系统会尽量保证日志缓存中的数据会在硬盘上有存档。下一次启动日志系统的时候会优先恢复硬盘上的未处理的存档内容。虽然依赖于操作系统行为，但是该机制本身经过了严格测试，绝大部分操作系统和环境都有非常高的成功率。


### 4. 自定义参数类型
在[format](#3-format参数-)提到了支持的参数类型，可以看出，C++默认支持的只有常见的参数类型。不过BqLog也支持两种方式来实现自定义类型的参数化。
___________________________________________________________________________________________________________________
*请一定注意，请你在bq_log.h或者生成的分类(Category)头文件的前面include你相关的自定义类和函数声明，这样才能保证兼容各种编译器，不然那就看命了*  
*根据实测，这里的方法二在clang下，如果顺序不对，可能编译不过*
___________________________________________________________________________________________________________________

#### 方法一：让类实现bq_log_format_str_size()和bq_log_format_str_chars接口
```cpp
// "custom_bq_log_type.h"
class A {
private:
    bool value_;

public:
    A(bool value):value_(value){}

    // 这里返回的是字符的个数，不是字节的个数，一定记得返回类型是size_t
    size_t bq_log_format_str_size() const
    {
        if (value_) {
            return strlen("true");
        } else {
            return strlen("false");
        }
    }
    // 返回实际的字符串的首字符地址，可以是char*, char16_t*, char32_t* 和wchar_t*
    const char* bq_log_format_str_chars() const
    {
        if (value_) {
            return "true";
        } else {
            return "false";
        }
    }
};
```

```cpp
#include "custom_bq_log_type.h"
#include "bq_log/bq_log.h"
void output(const bq::log& log_obj)
{
    log_obj.info("This should be Class A1:{}, A2:{}", A(true), A(false));
}
```

#### 方法二：实现全局的bq_log_format_str_size()和bq_log_format_str_chars()函数
有时候，可能要自定义的参数是别人写的类型我们无法更改（比如Unreal的`FString`和`FName`），或者干脆是某些不支持的`primitive`类型，我们也可以用全局函数的方式进行自定义。只要保证下面的函数声明在你调用该类型参数之前被include就行
由于自定义类型的优先级高于内置类型，所以你甚至可以用该方法去覆盖bqLog对于常规类型的输出。比如让`int32_t`类型大于零输出"PLUS"，负数输出"MINUS"，零输出"ZERO"
```cpp
//custom_bq_log_type.h
#pragma once
#include <map>
#include <cinttypes>
//覆盖int32_t作为参数的默认输出
size_t bq_log_format_str_size(const int32_t& param);
const char* bq_log_format_str_chars(const int32_t& param);

// 让std::map能作为参数被传入
template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param);
template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param);

template <typename KEY, typename VALUE>
size_t bq_log_format_str_size(const std::map<KEY, VALUE>& param)
{
    if (param.size() == 0) {
        return strlen("empty");
    } else {
        return strlen("full");
    }
}

//这个版本可以用utf16编码
template <typename KEY, typename VALUE>
const char16_t* bq_log_format_str_chars(const std::map<KEY, VALUE>& param)
{
    if (param.size() == 0) {
        return u"empty";
    } else {
        return u"full";
    }
}
```
```cpp
//custom_bq_log_type.cpp
#include "custom_bq_log_type.h"
size_t bq_log_format_str_size(const int32_t& param)
{
    if (param > 0) {
        return strlen("PLUS");
    } else if(param < 0){
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


```cpp
#include "custom_bq_log_type.h"
//保证自定义的全局函数能够出现在bq_log.h前面
#include "bq_log/bq_log.h"
void output(const bq::log& log_obj)
{
    std::map<int, bool> param0;
    std::map<int, bool> param1;
    param0[5] = false;
    my_category_log.info("This should be full:{}", param0); // 输出This should be full:full
    my_category_log.info("This should be empty:{}", param1); // 输出This should be empty:empty
    my_category_log.info("This should be PLUS:{}", 5); // 输出This should be PLUS:PLUS
    my_category_log.info("This should be MINUS:{}", -1); // 输出This should be MINUS:MINUS
    my_category_log.info(param0); // 输出Full
}
```
<br><br>


### 6. 在Unreal中使用BqLog
#### 1. 对FName, FString, FText的支持
`FString`，`FName`和`FText`是`Unreal`中常见的字符串类型，BqLog已经自带了adapter，在`Unreal`环境下会自动生效，兼容`Unreal 4`和`Unreal 5`。您可以直接使用以下代码：
```cpp
bq::log log_my = bq::log::create_log("AAA", config);   //config省略
FString fstring_1 = TEXT("这是一个测试的FString{}");
FString fstring_2 = TEXT("这也是一个测试的FString");
log_my.error(fstring_1, fstring_2);

FText text1 = FText::FromString(TEXT("这是一个FText!"));
FName name1 = FName(TEXT("这是一个FName"));
log_my.error(fstring_1, text1);
log_my.error(fstring_1, name1);
```

可以看到`FString`，`FName`和`FText`既可以直接作为format参数传入，也能作为可变参数传入。如果您希望自己定义自己版本的adapter，请在您的工程里定义全局宏`BQ_LOG_DISABLE_ADAPTER_FOR_UE`，这样的话这些adapter都会失效，您可以再定义一个您自己的版本。具体方法参见[上文](#方法二实现全局的bq_log_format_str_size和bq_log_format_str_chars函数)。


#### 2. 将BqLog的输出转接到Unreal的日志输出界面
如果您已经按照[Unreal集成说明](#unreal-engine)中引入了Unreal插件，那么BqLog的日志会自动转接到Unreal的日志输出界面中。如果您没有使用插件，而是直接在Unreal工程中使用BqLog，那么您可以通过以下代码实现转接：
```cpp
//事实上，你可以针对不同的category_idx，log_id，拿到对应的log对象名字，category的名字（见前面api），输出到UE_LOG不同的CategoryName中去
static void on_bq_log(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
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

void call_this_on_your_game_start()
{
    bq::log::register_console_callback(&on_bq_log);
}
```

#### 2. 将蓝图中使用BqLog
如果您已经按照[Unreal集成说明](#unreal-engine)中引入了Unreal插件，那么您将可以在蓝图中调用BqLog的日志接口。只要按照以下步骤操作即可：
1. 创建日志Data Asset。  
  在您的Unreal工程中，创建Data Asset资源，选择BqLog：
   <img src="docs/img/ue_pick_data_asset_1.png" alt="默认Log创建" style="width: 455px">  
  或者如果您按照[Category 日志类生成](#category-日志类生成)章节生成了支持Category的日志类，并且把生成的{category}.h和{category}_for_UE.h两个头文件都加入了自己工程，也可以选择对应的Category日志类型：
   <img src="docs/img/ue_pick_data_asset_2.png" alt="Category Log创建" style="width: 455px">
2. 配置日志参数，首先填好日志对象名：  
   双击打开刚才创建的Data Asset资源，配置日志参数， 如果是创建新的日志对象，则在红框处选择Create New Log。  
   <img src="docs/img/ue_create_log_config_1.png" alt="配置Log参数 Create New Log" style="width: 455px">  
   如果只是获取其他地方创建的日志对象，则选择Get Log By Name。
   <img src="docs/img/ue_create_log_config_2.png" alt="配置Log参数 Get Log By Name" style="width: 455px">
3. 在蓝图中，调用日志接口：  
      <img src="docs/img/ue_print_log.png" alt="蓝图调用Log" style="width: 655px">
    - 1 按钮可以增加日志参数
    - 2 是增加的日志参数，可以对节点点右键用（Remove ArgX)这样的选项选择删除某个参数。
    - 3 是可以选择日志对象，对应刚才创建的Data Asset资源
    - 4 只有当日志对象是带Category的情况下才会出现，可以选择日志对应的Category。
4. 测试：
    运行蓝图，如果您的日志配置正确且有ConsoleAppender输出，图中的节点应该会在Log窗口看到类似下面的输出：
   LogBqLog: Display: [Bussiness_Log_Obj] UTC+7 2025-11-27 14:49:19.381[tid-27732 ]   [I] [Factory.People.Manager]    Test Log Arg0:String Arg, Arg1:TRUE, Arg2:1.000000,0.000000,0.000000|2.999996,0.000000,0.000000|1.000000,1.500000,1.000000


### 7. 日志加密和解密
对于外发客户端来说，日志加密是一个非常重要的功能。在2.x之前的版本中，BqLog的二进制日志里依然有大量明文。在2.x版本中，BqLog引入了完整的日志加密功能，支持对日志内容进行加密存储，防止敏感信息泄露。
#### 1. 加密算法说明
BqLog使用了混合加密算法来保证日志的安全性和性能的平衡， 其核心是RSA2048 + AES256的组合。 仅可用于CompressedFileAppender，无法用于ConsoleAppender和TextFileAppender。性能开销较小，会增加5%到10%左右的CPU使用率，不会增加包体大小，几乎不会增加内存使用。
使用方法是先用ssh-keygen生成一对RSA密钥对，然后把公钥配置到日志系统中。解码时，使用私钥解密出AES256的对称密钥，然后用该对称密钥解密日志内容。
#### 2. 配置加密
在Terminal中执行
```bash
ssh-keygen -t rsa -b 2048 -m PEM -N "" -f "你的密钥文件路径"
```
会生成两份文件，一份是私钥文件（没有后缀名），另一份是公钥文件（.pub后缀名）。请确认公钥内容文件是以`ssh-rsa `开头，私钥文件是以`-----BEGIN RSA PRIVATE KEY-----`开头的`PCKS#1`格式的文件。
然后在对应的Appender配置中，加入如下配置项
```properties
 appenders_config.Appender名.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+Fo........rest of your rsa2048 public key...... user@hostname
```
其中的value是将公钥文件的内容全部拷贝过来的字符串。这样对应的CompressedFileAppender就会对日志内容进行加密存储了。
#### 3. 解密日志
解密日志时，需要用到私钥文件。可以使用BqLog自带的命令行工具BqLog_LogDecoder，使用方法见[离线解码二进制格式的Appender](#离线解码二进制格式的Appender)章节。在解码时，加入`-k 私钥文件路径`参数即可。
```bash
./BqLog_LogDecoder 要解码的文件 -o 输出文件 -k "./你的私钥文件路径"
``` 

## Benchmark

### 1. Benchmark说明
测试环境如下：
- **CPU**: 13th Gen Intel(R) Core(TM) i9-13900K   3.00 GHz
- **Memory**: 128 GB
- **OS**: Windows 11


测试用例是用1-10个线程去同时写日志，每个线程写2000000条日志。有带四个参数版本的，有不带参数版本的。最后同步等待所有日志都落地到硬盘上之后计算时间。这里只和Log2j做了对比。因为通过实测，其他所有的日志库，包括Java任何一个比较有名的开源日志库，C++的spdlog，还有C#的Log4net，都远不如Log2j + LMAX Disruptor的组合。所以我们直接和Log4j对比就行了。


### 2. BqLog C++ benchmark 代码
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
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", idx
                    , i
                    , 2.4232f
                    , true);
            }
            });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_text_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 2, 4 params============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", idx
                    , i
                    , 2.4232f
                    , true);
            }
            });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_compress_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 3, no param=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    bq::platform::atomic<int32_t> count(thread_count);
    uint64_t start_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("Empty Log, No Param");
            }
            });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl << std::endl;
}

void test_text_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 4, no param============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    bq::platform::atomic<int32_t> count(thread_count);
    uint64_t start_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i)
            {
                log_obj.info("Empty Log, No Param");
            }
            });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx)
    {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
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
        appenders_config.appender_3.file_name= benchmark_output/compress_
        appenders_config.appender_3.capacity_limit= 1
    )");
    bq::log text_log = bq::log::create_log("text", R"(
        appenders_config.appender_3.type=text_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name= benchmark_output/text_
        appenders_config.appender_3.capacity_limit= 1
    )");
    std::cout << "Please input the number of threads which will write log simultaneously:" << std::endl;
    int32_t thread_count;
    std::cin >> thread_count;

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

### 3. BqLog Java benchmark 代码
```java
import java.io.IOException;
import java.util.*;

/**
 * @author pippocao
 *
 *    Please copy dynamic native library to your classpath before you run this benchmark.
 *  Or set the Native Library Location to the directory of the dynamic libraries for the current platform under `(ProjectRoot)/dist`. 
 *  Otherwise, you may encounter an `UnsatisfiedLinkError`.
 */
public class benchmark_main {
    
    static abstract class benchmark_thread implements Runnable
    {
        protected int idx;
        public benchmark_thread(int idx)
        {
            this.idx = idx;
        }
    }
    
    private static void test_compress_multi_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("=========Begin Compressed File Log Test 1, 4 params=========");
        bq.log log_obj = bq.log.get_log_by_name("compress");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}", bq.utils.param.no_boxing(idx)
                            , bq.utils.param.no_boxing(i)
                            , bq.utils.param.no_boxing(2.4232f)
                            , bq.utils.param.no_boxing(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        bq.log.force_flush_all_logs();
        long flush_time = System.currentTimeMillis();
        System.out.println("\"Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }

    private static void test_text_multi_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 2, 4 params============");
        bq.log log_obj = bq.log.get_log_by_name("text");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}", bq.utils.param.no_boxing(idx)
                            , bq.utils.param.no_boxing(i)
                            , bq.utils.param.no_boxing(2.4232f)
                            , bq.utils.param.no_boxing(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        bq.log.force_flush_all_logs();
        long flush_time = System.currentTimeMillis();
        System.out.println("\"Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }

    private static void test_compress_no_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("=========Begin Compressed File Log Test 3, no param=========");
        bq.log log_obj = bq.log.get_log_by_name("compress");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        bq.log.force_flush_all_logs();
        long flush_time = System.currentTimeMillis();
        System.out.println("\"Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }

    private static void test_text_no_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 4, no param============");
        bq.log log_obj = bq.log.get_log_by_name("text");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        bq.log.force_flush_all_logs();
        long flush_time = System.currentTimeMillis();
        System.out.println("\"Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }
    

    public static void main(String[] args) throws Exception {
        // TODO Auto-generated method stub
        bq.log compressed_log =  bq.log.create_log("compress", """
                appenders_config.appender_3.type=compressed_file
                appenders_config.appender_3.levels=[all]
                appenders_config.appender_3.file_name= benchmark_output/compress_
                appenders_config.appender_3.capacity_limit= 1
            """);

        bq.log text_log =  bq.log.create_log("text", """
                appenders_config.appender_3.type=text_file
                appenders_config.appender_3.levels=[all]
                appenders_config.appender_3.file_name= benchmark_output/text_
                appenders_config.appender_3.capacity_limit= 1
            """);
        

        System.out.println("Please input the number of threads which will write log simultaneously:");
        int thread_count = 0;
        Scanner scanner = new Scanner(System.in);
        try {
            thread_count = scanner.nextInt();
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return;
        }finally {
            scanner.close();
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


### 4. Log4j benchmark代码

Log4j只测试了文本格式，因为他的gzip压缩是在滚动的时候重新启用gzip压缩格式做一遍压缩，要额外耗费性能，和BqLog的CompressedFileAppender没有可比性。

这里附上Log4j2的配置相关
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

```cpp
#log4j2.component.properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

```xml
<!-- log4j2.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <Console name="Console" target="SYSTEM_OUT">
      <PatternLayout pattern="%d{HH:mm:ss.SSS} [%t] %-5level %logger{36} - %msg%n"/>
    </Console>
    <!-- RollingFile Appender for gzip compressed files -->
    <RollingRandomAccessFile  name="my_appender" fileName="logs/compress.log" filePattern="logs/compress-%d{yyyy-MM-dd}-%i.log" immediateFlush="false">
      <PatternLayout>
        <Pattern>%d{yyyy-MM-dd HH:mm:ss} [%t] %-5level %logger{36} - %msg%n</Pattern>
      </PatternLayout>
      <Policies>
        <TimeBasedTriggeringPolicy interval="1" modulate="true"/>
      </Policies>
      <DefaultRolloverStrategy max="5"/>
    </RollingRandomAccessFile >

    <!-- Async Appender wrapping the other appenders -->
    <Async name="Async" includeLocation="false" bufferSize="262144">
      <!-- <AppenderRef ref="Console"/>-->
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

这里是源代码

```java
package bq.benchmark.log4j;

import java.util.*;
import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.core.async.AsyncLoggerContextSelector;
import org.apache.logging.log4j.LogManager;
import static org.apache.logging.log4j.util.Unbox.box;

public class main {
    public static final Logger log_obj = LogManager.getLogger(main.class);
    
    static abstract class benchmark_thread implements Runnable
    {
        protected int idx;
        protected Logger log_obj;
        public benchmark_thread(int idx, Logger log_obj)
        {
            this.idx = idx;
            this.log_obj = log_obj;
        }
    }

    private static void test_text_multi_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 1, 4 params============");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx, log_obj) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}", box(idx)
                            , box(i)
                            , box(2.4232f)
                            , box(true));
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        org.apache.logging.log4j.core.LoggerContext context = (org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false);
        context.stop();
        LogManager.shutdown();
        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }

    private static void test_text_no_param(int thread_count) throws Exception
    {
        System.out.println("============================================================");
        System.out.println("============Begin Text File Log Test 1, no param============");
        Thread[] threads = new Thread[thread_count];

        long start_time = System.currentTimeMillis();
        System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
        for (int idx = 0; idx < thread_count; ++idx)
        {
            Runnable r = new benchmark_thread(idx, log_obj) {
                @Override
                public void run()
                {
                    for (int i = 0; i < 2000000; ++i)
                    {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            };
            threads[idx] = new Thread(r);
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx)
        {
            threads[idx].join();
        }
        org.apache.logging.log4j.core.LoggerContext context = (org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false);
        context.stop();
        LogManager.shutdown();
        long flush_time = System.currentTimeMillis();
        System.out.println("Time Cost:" + (flush_time - start_time));
        System.out.println("============================================================");
        System.out.println("");
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Please input the number of threads which will write log simultaneously:");
        int thread_count = 0;
        Scanner scanner = new Scanner(System.in);
        try {
            thread_count = scanner.nextInt();
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return;
        }finally {
            scanner.close();
        }
        System.out.println("Is Aysnc:" + AsyncLoggerContextSelector.isSelected());

        //这两个函数只能分别测试,因为Log4j2的强制刷新之后，整个日志对象就失效了。要测试一个的时候，就注释掉另外一个
        test_text_multi_param(thread_count);
        //test_text_no_param(thread_count);
    }

}

```


### 5. Benchmark结果

数值为毫秒，值越少，代表耗时越少，性能越高。可以看出，TextFileAppender格式BqLog对比Log4j2有平均300%左右的提升。CompressedFileAppender格式对比Log4j2有800%左右提升。有数量级的差异。

#### 4个参数的总耗时（单位毫秒）

为了排版清晰，没有加入BqLog1.5的数据，可以直接和旧版本的文档的[Benchmark结果](https://github.com/Tencent/BqLog/blob/stable_1.5/README_CHS.md#5-benchmark%E7%BB%93%E6%9E%9C)对比。采取了一样的硬件和操作系统环境，相同的测试用例，相同的Log4j的结果。
从结果上看BqLog 2.x版本对比1.5版本大约有40%左右的性能提升。


|                         | 1线程  | 2线程  | 3线程  | 4线程  | 5线程  | 6线程  | 7线程  | 8线程  | 9线程  | 10线程 |
|-------------------------|------|------|------|------|------|------|------|------|------|------|
| BqLog Compress(C++)     | 110  | 125  | 188  | 256  | 318  | 374  | 449  | 511  | 583  | 642  |
| BqLog Text(C++)         | 344  | 699  | 1036 | 1401 | 1889 | 2211 | 2701 | 3121 | 3393 | 3561 |
| BqLog Compress(Java)    | 129  | 141  | 215  | 292  | 359  | 421  | 507  | 568  | 640  | 702  |
| BqLog Text(Java)        | 351  | 702  | 1052 | 1399 | 1942 | 2301 | 2754 | 3229 | 3506 | 3695 |
| Log4J2 Text             | 1065 | 2583 | 4249 | 4843 | 5068 | 6195 | 6424 | 7943 | 8794 | 9254 |


<img src="docs/img/benchmark_4_params.png" alt="4个参数的结果" style="width: 100%;">

#### 不带参数的总耗时（单位毫秒）
评测结果奇怪的是，不带参数的性能消耗上，Log4j表现得比带参数还要低不少。

|                     | 1线程  | 2线程  | 3线程  | 4线程  | 5线程  | 6线程  | 7线程   | 8线程   | 9线程   | 10线程  |
|-------------------------|------|------|------|------|------|------|-------|-------|-------|-------|
| BqLog Compress(C++)     | 97   | 101  | 155  | 228  | 290  | 341  | 415   | 476   | 541   | 601   |
| BqLog Text(C++)         | 153  | 351  | 468  | 699  | 916  | 1098 | 1212  | 1498  | 1733  | 1908  |
| BqLog Compress(Java)    | 109  | 111  | 178  | 240  | 321  | 378  | 449   | 525   | 592   | 670   |
| BqLog Text(Java)        | 167  | 354  | 491  | 718  | 951  | 1139 | 1278  | 1550  | 1802  | 1985  |
| Log4J2 Text             | 3204 | 6489 | 7702 | 8485 | 9640 | 10458 | 11483 | 12853 | 13995 | 14633 |

<img src="docs/img/benchmark_no_param.png" alt="不带参数的结果" style="width: 100%;">


## 如何贡献代码
如果您希望贡献您的代码，请确保您的代码可以正确执行`Github Action`下面的`AutoTest`和`Build`两个Action。
