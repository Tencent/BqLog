# Benchmark

[← 返回首页](../README_CHS.md) | [English](./BENCHMARK.md)

### 1. Benchmark 说明

测试环境：

- **机型**：PC
- **CPU**：AMD Ryzen 9 9950X（16 核 / 32 线程）
- **Memory**：96 GB
- **OS**：Windows 11 专业版（10.0.26100）
- **编译器**：MSVC 14.51（Visual Studio 18 Insiders），Release x64
- **Java**：JBR 21.0.9（OpenJDK，Android Studio 自带）

> 此前在 Apple M4 Pro / macOS 上测得的旧结果可在本文档的 git 历史中找到。

测试用例如下：

- 使用 1～10 个线程同时写日志；
- 每个线程写入 2,000,000 条日志：
  - 一种为带 4 个参数的格式化日志；
  - 一种为不带参数的纯文本日志；
- 等待所有线程结束，再将所有日志强制落盘（BqLog 用 `force_flush_all_logs()`，spdlog 用 `spdlog::shutdown()`，glog 用 `FlushLogFiles`，fmtlog 用 `poll(true)`，quill 用 `flush_log()`，Log4j2 用 `LoggerContext.stop()`），统计从开始写入到所有日志落盘的总耗时。

对比对象：

- BqLog 2.4.0（C++，TextFileAppender、CompressedFileAppender、CompressedFileAppender + 加密）
- spdlog 1.17.0（**异步**文件日志：8192 槽位队列 + 1 个后台线程，溢出策略为阻塞——不丢日志；计时在 `spdlog::shutdown()` 排空队列并落盘后才结束）
- glog 0.7.1（同步文件日志，流式 API——**glog 没有异步模式**，见下方说明）
- fmtlog（异步文件日志，编译时启用 `FMTLOG_BLOCK=1` 防止静默丢日志）
- quill 11.1.0（异步文件日志，按官方 benchmark 配置：后端 busy-spin）
- Log4j2 2.23.1（Java，AsyncLogger + Disruptor + Async Appender）

所有库的完整可运行 benchmark 工程（CMake + FetchContent，每个库一个可执行文件，附吞吐量与峰值内存的 PowerShell 运行脚本）存放在专门的 [`benchmark` 分支](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross)。

### 2. Benchmark 结果

下表 BqLog 数据来自 C++ 接口。其他语言的 wrapper 通常会增加运行时、
跨语言调用和参数处理开销，性能因语言及负载而异，应使用对应语言的
benchmark 在相同条件下比较。

所有耗时单位为毫秒，数值越小性能越高。

#### 2.1 吞吐量 — 带 4 个参数的总耗时（毫秒）

|                              | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|------------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)         | 95     | 144    | 210    | 210    | 226    | 267    | 362    | 395    | 439    | 507     |
| BqLog Compress+Encrypt (C++) | 102    | 166    | 167    | 190    | 236    | 308    | 350    | 391    | 453    | 493     |
| BqLog Text (C++)             | 258    | 513    | 777    | 1054   | 1324   | 1587   | 1891   | 2143   | 2465   | 2811    |
| fmtlog                       | 548    | 1173   | 1665   | 2194   | 2881   | 3440   | 4386   | 5242   | 5888   | 6926    |
| quill                        | 639    | 1429   | 2232   | 3082   | 3915   | 4726   | 5609   | 6246   | 6957   | 7812    |
| Log4j2 (Java)                | 873    | 1484   | 2087   | 2727   | 3738   | 4541   | 4889   | 6127   | 9475   | 7192    |
| spdlog（异步）               | 560    | 1649   | 3402   | 5737   | 9069   | 13827  | 21494  | 24518  | 28463  | 32939   |
| glog                         | 4485   | 8548   | 14875  | 21387  | 28295  | 36060  | 45742  | 62368  | 102370 | 127550  |

#### 2.2 峰值内存占用（MB）

以 Windows 进程峰值工作集（Peak Working Set）衡量，运行全程每 5 ms 采样一次。每个 benchmark 以独立进程运行。其中 BqLog 进程同时挂载全部三种 appender（Text / Compress / Compress+Encrypt），其余每个进程只挂载 1 个 logger。

|                                          | 1 线程 | 4 线程 | 10 线程 |
|------------------------------------------|--------|--------|---------|
| BqLog（单进程内同时挂载 3 种 appender）  | 12.7   | 13.3   | 14.7    |
| spdlog（异步）                           | 14.4   | 14.4   | 14.7    |
| glog                                     | 11.9   | 12.1   | 12.4    |
| fmtlog                                   | 17.1   | 20.2   | 23.3    |
| quill                                    | 282.1  | 1062.7 | 2714.9  |
| Log4j2 (Java)                            | 1537.8 | 6884.3 | 4631.5  |

> quill 的每线程无界 SPSC 队列在持续压力下会倍增至每条 64 MiB，这是其 10 线程时内存峰值达到数 GB 的原因。Log4j2 的数值包含 JVM 堆内存和 GC 活动，多次运行之间波动明显。

#### 2.3 日志文件大小对比（1 线程，400 万条日志）

| 库 | 格式 | 文件大小 | 每条日志字节数 |
|----|------|---------|-------------|
| BqLog Compress | 二进制（压缩） | 45 MB | 12 B |
| BqLog Compress+Encrypt | 二进制（加密） | 45 MB | 12 B |
| BqLog Text | 文本 | 283 MB | 74 B |
| spdlog（异步） | 文本 | 285 MB | 75 B |
| glog | 文本 | 314 MB | 82 B |
| fmtlog | 文本 | 270 MB | 71 B |
| quill | 文本 | 247 MB | 65 B |
| Log4j2 | 文本 | 410 MB（200 万条，仅 multi_param） | 215 B |

#### 2.4 总结

- **BqLog Compress** 吞吐量最高 — 比 fmtlog 快 **6-14 倍**，比 Log4j2 快 **9-22 倍**，比 spdlog（异步）快 **6-65 倍**，比 glog 快 **47-252 倍**
- 即使是 **BqLog Text** 模式，在所有线程数下也优于所有其他文本日志库（比最快的对手 fmtlog 快 **2.1-2.5 倍**）
- **加密几乎零额外开销** — BqLog Compress 与 Compress+Encrypt 性能几乎相同
- **内存高效** — 即使同时挂载三种 appender，BqLog 峰值工作集也仅 **12.7-14.7 MB**
- **压缩格式比文本小 6.3 倍**，大幅节省存储和 I/O 成本

> 公平性说明：
> - **spdlog** 以其**异步模式**（`async_logger` + 线程池）参与测试，这是它官方推荐的高吞吐配置。队列使用默认的阻塞溢出策略，不丢任何日志；计时区间在 `spdlog::shutdown()` 排空队列并落盘后才结束——与其他库「全部落盘」的口径一致。
> - **glog** 的同步是其**架构本质**：该库没有异步模式，因此其成绩天然反映调用线程内同步格式化、同步写盘的特征。它也不支持 `{fmt}` 格式化参数，有参数测试使用其标准的流式 `operator<<` API。
> - **fmtlog** 编译时启用了 `FMTLOG_BLOCK=1` 以防止静默丢日志（其默认行为）。**quill** 按照其官方 benchmark 配置使用 busy-spin 后端以获得最佳性能。

### 3. 功能对比

| 特性 | BqLog | spdlog | glog | fmtlog | quill | Log4j2 |
|------|-------|--------|------|--------|-------|--------|
| 异步写入 | ✅ | ✅（本次测试已使用） | ❌（不支持） | ✅ | ✅ | ✅ |
| 实时压缩 | ✅ | ❌ | ❌ | ❌ | ❌ | ❌（滚动 gzip） |
| 日志加密 | ✅（RSA+AES 混合） | ❌ | ❌ | ❌ | ❌ | ❌ |
| 崩溃恢复 | ✅（Recovery） | ❌ | ✅（信号处理） | ❌ | ✅（信号处理） | ❌ |
| 多语言支持 | ✅（C++/Java/C#/Python/TypeScript/ArkTS/Go） | ❌（仅 C++） | ❌（仅 C++） | ❌（仅 C++） | ❌（仅 C++） | 仅 Java |
| 跨平台 | ✅（Win/Mac/Linux/iOS/Android/鸿蒙） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | JVM |
| `{fmt}` 格式化 | ✅ | ✅ | ❌（流式） | ✅ | ✅ | ✅（类似） |
| 热路径无堆分配 | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| 游戏引擎插件 | ✅（Unity/Unreal） | ❌ | ❌ | ❌ | ❌ | ❌ |

### 4. 附录：Benchmark 源代码

以下代码与 [`benchmark` 分支](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross)上的可运行工程一致。

#### 4.1 BqLog C++ Benchmark 代码

##### BqLog TextFileAppender

```cpp
#include "bq_log/bq_log.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    bq::log log_obj = bq::log::create_log("bench_text", R"(
        log.high_perform_mode_freq_threshold_per_second=1
        appenders_config.appender_0.type=text_file
        appenders_config.appender_0.levels=[all]
        appenders_config.appender_0.file_name=output/bqlog_text
        appenders_config.appender_0.always_create_new_file=true
    )");

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t, &log_obj]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
            }
        });
    }
    for (auto& th : threads) th.join();
    bq::log::force_flush_all_logs();
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time Cost:" << ms << " ms" << std::endl;
    return 0;
}
```

##### BqLog CompressedFileAppender

仅配置不同 — 测试逻辑完全一致：

```cpp
    bq::log log_obj = bq::log::create_log("bench_compress", R"(
        log.high_perform_mode_freq_threshold_per_second=1
        appenders_config.appender_0.type=compressed_file
        appenders_config.appender_0.levels=[all]
        appenders_config.appender_0.file_name=output/bqlog_compress
        appenders_config.appender_0.always_create_new_file=true
    )");
```

##### BqLog CompressedFileAppender + 加密

```cpp
    bq::log log_obj = bq::log::create_log("bench_compress_enc", R"(
        log.high_perform_mode_freq_threshold_per_second=1
        appenders_config.appender_0.type=compressed_file
        appenders_config.appender_0.levels=[all]
        appenders_config.appender_0.file_name=output/bqlog_compress_enc
        appenders_config.appender_0.always_create_new_file=true
        appenders_config.appender_0.pub_key=<YOUR_RSA_PUBLIC_KEY>
    )");
```

#### 4.2 spdlog Benchmark 代码（异步）

spdlog 以其异步模式参与测试——`async_logger` 基于线程池，由专用后台线程消费。
溢出策略为默认的 `block`，不丢任何日志（与 fmtlog 的 `FMTLOG_BLOCK=1` 口径一致）。
计时在 `spdlog::shutdown()` 之后结束，它会排空队列并将 sink 落盘——与其他库
「全部写入磁盘」的口径一致。

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;
static const size_t QUEUE_SIZE = 8192;      // spdlog 默认异步队列槽位数
static const size_t BACKEND_THREADS = 1;

static void run_test(const char* name, const char* file, int thread_count, bool multi_param)
{
    spdlog::init_thread_pool(QUEUE_SIZE, BACKEND_THREADS);
    auto logger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(name, file, true);
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%f [%t] [%l] %v");

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&logger, t, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    logger->info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    logger->info("Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    spdlog::shutdown(); // 排空队列，落盘
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|spdlog_async|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    run_test("bench_mp", "output/spdlog_mp.log", thread_count, true);
    run_test("bench_np", "output/spdlog_np.log", thread_count, false);
    return 0;
}
```

#### 4.3 glog Benchmark 代码

> 注：glog 的同步是其架构本质——该库没有异步模式。它也不支持 `{fmt}` 格式化，
> 使用流式 `operator<<` 作为标准 API。

```cpp
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    google::InitGoogleLogging("benchmark");
    FLAGS_log_dir = "output/";
    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;

    // multi_param
    {
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG(INFO) << "idx:" << t << ", num:" << i << ", This test, " << 2.4232f << ", " << true;
                }
            });
        }
        for (auto& th : threads) th.join();
        google::FlushLogFiles(google::GLOG_INFO);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|glog|multi_param|" << thread_count << "|" << ms << std::endl;
    }

    // no_param
    {
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG(INFO) << "Empty Log, No Param";
                }
            });
        }
        for (auto& th : threads) th.join();
        google::FlushLogFiles(google::GLOG_INFO);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|glog|no_param|" << thread_count << "|" << ms << std::endl;
    }

    google::ShutdownGoogleLogging();
    return 0;
}
```

#### 4.4 fmtlog Benchmark 代码

> 注：需要 `FMTLOG_BLOCK=1` 防止静默丢日志（其默认行为是队列满时丢日志）。
> fmtlog 的 `setLogFile()` 与其轮询线程存在竞态，因此两个测试分别在独立的
> 进程中运行（`argv[2]` = `mp` | `np`）。

```cpp
#define FMTLOG_BLOCK 1
#include "fmtlog.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);
    const char* which = (argc >= 3) ? argv[2] : "both";

    if (strcmp(which, "np") != 0) {  // multi_param
        fmtlog::setLogFile("output/fmtlog_mp.log", false);
        fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
        fmtlog::startPollingThread(1);
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    FMTLOG(fmtlog::INF, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                }
            });
        }
        for (auto& th : threads) th.join();
        fmtlog::poll(true);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|multi_param|" << thread_count << "|" << ms << std::endl;
        fmtlog::stopPollingThread();
    }

    if (strcmp(which, "mp") != 0) {  // no_param
        fmtlog::setLogFile("output/fmtlog_np.log", false);
        fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
        fmtlog::startPollingThread(1);
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    FMTLOG(fmtlog::INF, "Empty Log, No Param");
                }
            });
        }
        for (auto& th : threads) th.join();
        fmtlog::poll(true);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|no_param|" << thread_count << "|" << ms << std::endl;
        fmtlog::stopPollingThread();
    }

    _exit(0);  // fmtlog has cleanup issues, use _exit
}
```

#### 4.5 quill Benchmark 代码

> 注：按照 quill 官方 benchmark 配置，使用 busy-spin 后端（`sleep_duration = 0ns`）以获得最佳性能。

```cpp
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    // Backend config following quill's official benchmark
    quill::BackendOptions backend_options;
    backend_options.sleep_duration = std::chrono::nanoseconds{0};  // busy spin
    quill::Backend::start(backend_options);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let backend init

    // multi_param
    {
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "output/quill_mp.log");
        quill::Logger* logger = quill::Frontend::create_or_get_logger(
            "bench_mp", std::move(file_sink),
            quill::PatternFormatterOptions{
                "%(time) [%(thread_id)] %(log_level) %(message)",
                "%H:%M:%S.%Qns"});

        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([logger, t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG_INFO(logger, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                }
            });
        }
        for (auto& th : threads) th.join();
        logger->flush_log();
        quill::Frontend::remove_logger(logger);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|quill|multi_param|" << thread_count << "|" << ms << std::endl;
    }

    // no_param
    {
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
            "output/quill_np.log");
        quill::Logger* logger = quill::Frontend::create_or_get_logger(
            "bench_np", std::move(file_sink),
            quill::PatternFormatterOptions{
                "%(time) [%(thread_id)] %(log_level) %(message)",
                "%H:%M:%S.%Qns"});

        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([logger]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG_INFO(logger, "Empty Log, No Param");
                }
            });
        }
        for (auto& th : threads) th.join();
        logger->flush_log();
        quill::Frontend::remove_logger(logger);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|quill|no_param|" << thread_count << "|" << ms << std::endl;
    }

    return 0;
}
```

#### 4.6 Log4j Benchmark 代码

Log4j2 部分只测试了文本输出格式，因为其 gzip 压缩是在「滚动时对已有文本文件重新 gzip 压缩」，这与 BqLog 实时压缩模式的性能模型完全不同，无法直接对标。

依赖为 Maven Central 上的普通 jar 包，由 `log4j/fetch_deps.ps1` 下载（不需要 Maven）：

- log4j-api 2.23.1、log4j-core 2.23.1、disruptor 3.4.2

启用 AsyncLogger（classpath 下的 `log4j2.component.properties`）：

```properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

Log4j2 配置（classpath 下的 `log4j2.xml`）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <!-- RollingRandomAccessFile，用于演示文本输出 -->
    <RollingRandomAccessFile name="my_appender"
                             fileName="output/log4j2.log"
                             filePattern="output/log4j2-%d{yyyy-MM-dd}-%i.log"
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

源代码：

```java
package bq.benchmark.log4j;

import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.core.async.AsyncLoggerContextSelector;

import static org.apache.logging.log4j.util.Unbox.box;

public class main {

    public static final Logger log_obj = LogManager.getLogger(main.class);

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.out.println("usage: main <thread_count> [mp|np]");
            return;
        }
        int thread_count = Integer.parseInt(args[0]);
        boolean multi_param = args.length < 2 || !args[1].equals("np");

        System.out.println("Is Async:" + AsyncLoggerContextSelector.isSelected());

        Thread[] threads = new Thread[thread_count];
        long start_time = System.currentTimeMillis();
        for (int idx = 0; idx < thread_count; ++idx) {
            final int t = idx;
            threads[idx] = new Thread(() -> {
                for (int i = 0; i < 2000000; ++i) {
                    if (multi_param) {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}",
                            box(t), box(i), box(2.4232f), box(true));
                    } else {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            });
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        // stop() 排空 Disruptor 环形缓冲区并冲刷 appender
        ((org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false)).stop();
        LogManager.shutdown();

        long flush_time = System.currentTimeMillis();
        System.out.println("RESULT|log4j2|" + (multi_param ? "multi_param" : "no_param")
            + "|" + thread_count + "|" + (flush_time - start_time));
    }
}
```
