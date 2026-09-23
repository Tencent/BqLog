# Benchmark

[← Back to Home](../README.md) | [简体中文](./BENCHMARK_CHS.md)

### 1. Benchmark description

Test Environment:

- **Machine**: MacBook Pro
- **CPU**: Apple M4 Pro (14-core: 10 performance + 4 efficiency)
- **Memory**: 48 GB
- **OS**: macOS

Test Cases:

- Use 1~10 threads to write logs simultaneously;
- Each thread writes 2,000,000 log entries:
  - One with 4 parameters formatted log;
  - One with no parameter plain text log;
- Wait for all threads to finish, then call `force_flush_all_logs()`, count total time from start of writing to all logs flushed to disk.

Comparison Objects:

- BqLog 2.4.0 (C++, TextFileAppender, CompressedFileAppender, and CompressedFileAppender with Encryption)
- spdlog 1.17.0 (synchronous file logger)
- glog 0.7.1 (synchronous file logger, stream-based API)
- fmtlog (async file logger, compiled with `FMTLOG_BLOCK=1` to prevent log dropping)
- quill 11.1.0 (async file logger, configured per official benchmark: busy-spin backend)
- Log4j2 (Java, async with Disruptor — retained from previous benchmark)

### 2. Benchmark results

The BqLog figures below measure the C++ API. Other language wrappers generally
add runtime, foreign-call and argument-processing overhead. Performance varies
by language and workload; compare each language's benchmark under matching
conditions.

All time costs are in milliseconds, smaller values mean higher performance.

#### 2.1 Throughput — Total Time Cost with 4 parameters (ms)

|                              | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|------------------------------|----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|------------|
| BqLog Compress (C++)         | 64       | 105       | 122       | 163       | 251       | 313       | 372       | 413       | 496       | 729        |
| BqLog Compress+Encrypt (C++) | 71       | 114       | 127       | 176       | 250       | 313       | 370       | 428       | 502       | 744        |
| BqLog Text (C++)             | 198      | 399       | 618       | 880       | 1137      | 1405      | 1692      | 1931      | 2244      | 2685       |
| fmtlog                       | 248      | 489       | 765       | 1059      | 1341      | 1588      | 1906      | 2234      | 2379      | 2818       |
| quill                        | 425      | 805       | 1222      | 1700      | 2108      | 2592      | 2951      | 3458      | 3957      | 4316       |
| spdlog                       | 434      | 1366      | 3133      | 4779      | 6228      | 9241      | 10829     | 11348     | 11197     | 12003      |
| Log4j2 (Java)                | 946      | 1841      | 2422      | 3685      | 5542      | 5245      | 5775      | 5786      | 8048      | 8752       |
| glog                         | 2138     | 3812      | 7144      | 10446     | 13552     | 21695     | 28806     | 35153     | 40397     | 45162      |

#### 2.2 Peak Memory Usage (MB)

Measured with `/usr/bin/time -l` on macOS (peak resident set size). Each benchmark runs as a separate process with 1 logger instance.

|                              | 1 Thread | 4 Threads | 10 Threads |
|------------------------------|----------|-----------|------------|
| BqLog Compress (C++)         | 2.3      | 2.7       | 3.4        |
| BqLog Compress+Encrypt (C++) | 2.7      | 3.4       | 3.8        |
| BqLog Text (C++)             | 2.6      | 2.9       | 3.8        |
| spdlog                       | 2.1      | 2.2       | 2.4        |
| glog                         | 2.1      | 2.5       | 3.2        |
| fmtlog                       | 3.9      | 6.1       | 12.7       |
| quill                        | 272.9    | 1058.5    | 2746.6     |

#### 2.3 Output File Size Comparison (1 thread, 4M log entries)

| Library | Format | File Size | Bytes/Entry |
|---------|--------|-----------|-------------|
| BqLog Compress | Binary (compressed) | 45 MB | 12 B |
| BqLog Compress+Encrypt | Binary (encrypted) | 45 MB | 12 B |
| BqLog Text | Text | 302 MB | 79 B |
| spdlog | Text | 293 MB | 77 B |
| glog | Text | 348 MB | 91 B |
| fmtlog | Text | 285 MB | 75 B |
| quill | Text | 255 MB | 67 B |
| Log4j2 | Text | ~300 MB | — |

#### 2.4 Summary

- **BqLog Compress** achieves the highest throughput — **4–7x faster than fmtlog**, **7–30x faster than spdlog**, **33–85x faster than glog**
- Even **BqLog Text** outperforms all other text-based loggers at every thread count
- **Encryption adds near-zero overhead** — BqLog Compress vs Compress+Encrypt performance is nearly identical
- **Memory efficient** — BqLog uses only **2.3–3.8 MB** regardless of mode
- **Compressed format is 6.7x smaller** than text output, reducing storage and I/O costs

> Note: glog does not support `{fmt}`-style formatting; it uses stream-based `operator<<` in the parameterized test. fmtlog was compiled with `FMTLOG_BLOCK=1` to prevent silent log dropping (its default behavior). quill was configured per its official benchmark with busy-spin backend for maximum performance.

### 3. Feature Comparison

| Feature | BqLog | spdlog | glog | fmtlog | quill | Log4j2 |
|---------|-------|--------|------|--------|-------|--------|
| Async logging | ✅ | ✅ (optional) | ❌ | ✅ | ✅ | ✅ |
| Real-time compression | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ (rolling gzip) |
| Log encryption | ✅ (RSA+AES hybrid) | ❌ | ❌ | ❌ | ❌ | ❌ |
| Crash recovery | ✅ (Recovery) | ❌ | ✅ (signal handler) | ❌ | ✅ (signal handler) | ❌ |
| Multi-language | ✅ (C++/Java/C#/Python/JS/ArkTS) | ❌ (C++ only) | ❌ (C++ only) | ❌ (C++ only) | ❌ (C++ only) | Java only |
| Cross-platform | ✅ (Win/Mac/Linux/iOS/Android/HarmonyOS) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | JVM |
| `{fmt}` formatting | ✅ | ✅ | ❌ (stream) | ✅ | ✅ | ✅ (similar) |
| No-heap-alloc hot path | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| Game engine plugins | ✅ (Unity/Unreal) | ❌ | ❌ | ❌ | ❌ | ❌ |

### 4. Appendix: Benchmark Source Code

#### 4.1 BqLog C++ Benchmark code

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

Only the configuration differs — the test logic is identical:

```cpp
    bq::log log_obj = bq::log::create_log("bench_compress", R"(
        log.high_perform_mode_freq_threshold_per_second=1
        appenders_config.appender_0.type=compressed_file
        appenders_config.appender_0.levels=[all]
        appenders_config.appender_0.file_name=output/bqlog_compress
        appenders_config.appender_0.always_create_new_file=true
    )");
```

##### BqLog CompressedFileAppender + Encryption

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

#### 4.2 spdlog Benchmark code

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

#### 4.3 glog Benchmark code

> Note: glog does not support `{fmt}`-style formatting; it uses stream-based `operator<<` as its standard API.

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

#### 4.4 fmtlog Benchmark code

> Note: `FMTLOG_BLOCK=1` is required to prevent silent log dropping (fmtlog's default behavior drops logs when the queue is full).

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

#### 4.5 quill Benchmark code

> Note: Configured per quill's official benchmark with busy-spin backend (`sleep_duration = 0ns`) for maximum performance.

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

#### 4.6 Log4j Benchmark code

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
