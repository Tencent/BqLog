# Benchmark

[← 返回首页](../README_CHS.md) | [English](./BENCHMARK.md)

### 1. Benchmark 说明

测试环境：

- **机型**：MacBook Pro
- **CPU**：Apple M4 Pro（14 核：10 性能核 + 4 能效核）
- **Memory**：48 GB
- **OS**：macOS

测试用例如下：

- 使用 1～10 个线程同时写日志；
- 每个线程写入 2,000,000 条日志：
  - 一种为带 4 个参数的格式化日志；
  - 一种为不带参数的纯文本日志；
- 等待所有线程结束，再调用 `force_flush_all_logs()`，统计从开始写入到所有日志落盘的总耗时。

对比对象：

- BqLog 2.4.0（C++，TextFileAppender、CompressedFileAppender、CompressedFileAppender + 加密）
- spdlog 1.17.0（同步文件日志）
- glog 0.7.1（同步文件日志，流式 API）
- fmtlog（异步文件日志，编译时启用 `FMTLOG_BLOCK=1` 防止静默丢日志）
- quill 11.1.0（异步文件日志，按官方 benchmark 配置：后端 busy-spin）
- Log4j2（Java，异步 + Disruptor — 保留自之前的 benchmark）

### 2. Benchmark 结果

下表 BqLog 数据来自 C++ 接口。其他语言的 wrapper 通常会增加运行时、
跨语言调用和参数处理开销，性能因语言及负载而异，应使用对应语言的
benchmark 在相同条件下比较。

#### 2.1 吞吐量 — 带 4 个参数的总耗时（毫秒）

|                              | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|------------------------------|--------|--------|--------|--------|--------|--------|--------|--------|--------|---------|
| BqLog Compress (C++)         | 64     | 105    | 122    | 163    | 251    | 313    | 372    | 413    | 496    | 729     |
| BqLog Compress+Encrypt (C++) | 71     | 114    | 127    | 176    | 250    | 313    | 370    | 428    | 502    | 744     |
| BqLog Text (C++)             | 198    | 399    | 618    | 880    | 1137   | 1405   | 1692   | 1931   | 2244   | 2685    |
| fmtlog                       | 248    | 489    | 765    | 1059   | 1341   | 1588   | 1906   | 2234   | 2379   | 2818    |
| quill                        | 425    | 805    | 1222   | 1700   | 2108   | 2592   | 2951   | 3458   | 3957   | 4316    |
| spdlog                       | 434    | 1366   | 3133   | 4779   | 6228   | 9241   | 10829  | 11348  | 11197  | 12003   |
| Log4j2 (Java)                | 946    | 1841   | 2422   | 3685   | 5542   | 5245   | 5775   | 5786   | 8048   | 8752    |
| glog                         | 2138   | 3812   | 7144   | 10446  | 13552  | 21695  | 28806  | 35153  | 40397  | 45162   |

#### 2.2 峰值内存占用（MB）

使用 macOS 上的 `/usr/bin/time -l` 测量进程 RSS 峰值。每个 benchmark 作为独立进程运行，仅含 1 个 logger 实例。

|                              | 1 线程 | 4 线程 | 10 线程 |
|------------------------------|--------|--------|---------|
| BqLog Compress (C++)         | 2.3    | 2.7    | 3.4     |
| BqLog Compress+Encrypt (C++) | 2.7    | 3.4    | 3.8     |
| BqLog Text (C++)             | 2.6    | 2.9    | 3.8     |
| spdlog                       | 2.1    | 2.2    | 2.4     |
| glog                         | 2.1    | 2.5    | 3.2     |
| fmtlog                       | 3.9    | 6.1    | 12.7    |
| quill                        | 272.9  | 1058.5 | 2746.6  |

#### 2.3 日志文件大小对比（1 线程，400 万条日志）

| 库 | 格式 | 文件大小 | 每条日志字节数 |
|----|------|---------|-------------|
| BqLog Compress | 二进制（压缩） | 45 MB | 12 B |
| BqLog Compress+Encrypt | 二进制（加密） | 45 MB | 12 B |
| BqLog Text | 文本 | 302 MB | 79 B |
| spdlog | 文本 | 293 MB | 77 B |
| glog | 文本 | 348 MB | 91 B |
| fmtlog | 文本 | 285 MB | 75 B |
| quill | 文本 | 255 MB | 67 B |
| Log4j2 | 文本 | ~300 MB | — |

#### 2.4 总结

- **BqLog Compress** 吞吐量最高 — 比 fmtlog 快 **4-7 倍**，比 spdlog 快 **7-30 倍**，比 glog 快 **33-85 倍**
- 即使是 **BqLog Text** 模式，在所有线程数下也优于所有其他文本日志库
- **加密几乎零额外开销** — BqLog Compress 与 Compress+Encrypt 性能几乎相同
- **内存高效** — BqLog 仅使用 **2.3-3.8 MB**，无论模式如何
- **压缩格式比文本小 6.7 倍**，大幅节省存储和 I/O 成本

> 注：glog 不支持 `{fmt}` 格式化参数，在有参数测试中使用流式 `operator<<`。fmtlog 编译时启用了 `FMTLOG_BLOCK=1` 以防止静默丢日志（其默认行为）。quill 按照其官方 benchmark 配置使用 busy-spin 后端以获得最佳性能。

### 3. 功能对比

| 特性 | BqLog | spdlog | glog | fmtlog | quill | Log4j2 |
|------|-------|--------|------|--------|-------|--------|
| 异步写入 | ✅ | ✅（可选） | ❌ | ✅ | ✅ | ✅ |
| 实时压缩 | ✅ | ❌ | ❌ | ❌ | ❌ | ❌（滚动 gzip） |
| 日志加密 | ✅（RSA+AES 混合） | ❌ | ❌ | ❌ | ❌ | ❌ |
| 崩溃恢复 | ✅（Recovery） | ❌ | ✅（信号处理） | ❌ | ✅（信号处理） | ❌ |
| 多语言支持 | ✅（C++/Java/C#/Python/JS/ArkTS） | ❌（仅 C++） | ❌（仅 C++） | ❌（仅 C++） | ❌（仅 C++） | 仅 Java |
| 跨平台 | ✅（Win/Mac/Linux/iOS/Android/鸿蒙） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | ✅（Win/Mac/Linux） | JVM |
| `{fmt}` 格式化 | ✅ | ✅ | ❌（流式） | ✅ | ✅ | ✅（类似） |
| 热路径无堆分配 | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| 游戏引擎插件 | ✅（Unity/Unreal） | ❌ | ❌ | ❌ | ❌ | ❌ |

### 4. 附录：Benchmark 源代码

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

#### 4.2 spdlog Benchmark 代码

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    // multi_param
    {
        auto logger = spdlog::basic_logger_mt("bench_mp", "/tmp/bqlog_benchmark/output/spdlog_mp.log", true);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%f [%t] [%l] %v");
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&logger, t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    logger->info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                }
            });
        }
        for (auto& th : threads) th.join();
        logger->flush();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|spdlog|multi_param|" << thread_count << "|" << ms << std::endl;
        spdlog::drop("bench_mp");
    }

    // no_param
    {
        auto logger = spdlog::basic_logger_mt("bench_np", "/tmp/bqlog_benchmark/output/spdlog_np.log", true);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%f [%t] [%l] %v");
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&logger]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    logger->info("Empty Log, No Param");
                }
            });
        }
        for (auto& th : threads) th.join();
        logger->flush();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|spdlog|no_param|" << thread_count << "|" << ms << std::endl;
        spdlog::drop("bench_np");
    }
    return 0;
}
```

#### 4.3 glog Benchmark 代码

注：glog 使用流式 `operator<<`，不支持 `{fmt}` 格式化。

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
    FLAGS_log_dir = "/tmp/bqlog_benchmark/output/";
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

注：需要 `FMTLOG_BLOCK=1` 防止静默丢日志。

```cpp
#define FMTLOG_BLOCK 1
#include "fmtlog.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    fmtlog::setLogFile("/tmp/bqlog_benchmark/output/fmtlog_mp.log", false);
    fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
    fmtlog::startPollingThread(1);

    // multi_param
    {
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
    }

    // no_param
    {
        fmtlog::setLogFile("/tmp/bqlog_benchmark/output/fmtlog_np.log", false);
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
    }

    fmtlog::stopPollingThread();
    _exit(0);  // fmtlog has cleanup issues, use _exit
}
```

#### 4.5 quill Benchmark 代码

注：按照 quill 官方 benchmark 配置，使用 busy-spin 后端。

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
            "/tmp/bqlog_benchmark/output/quill_mp.log");
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
            "/tmp/bqlog_benchmark/output/quill_np.log");
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

**依赖：**

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

启用 AsyncLogger：

```properties
# log4j2.component.properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

Log4j2 配置：

```xml
<!-- log4j2.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <Console name="Console" target="SYSTEM_OUT">
      <PatternLayout pattern="%d{HH:mm:ss.SSS} [%t] %-5level %logger{36} - %msg%n"/>
    </Console>

    <!-- RollingRandomAccessFile，用于演示文本输出 -->
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

源代码：

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

        // 这两个测试需分开运行，因为强制关闭后 LoggerContext 不再可用。
        test_text_multi_param(thread_count);
        // test_text_no_param(thread_count);
    }

}
```
