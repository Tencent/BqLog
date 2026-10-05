# Benchmark

[← Back to Home](../README.md) | [简体中文](./BENCHMARK_CHS.md)

### 1. Benchmark description

Test Environment:

- **Machine**: PC
- **CPU**: AMD Ryzen 9 9950X (16 cores / 32 threads)
- **Memory**: 96 GB
- **OS**: Windows 11 Pro (10.0.26100)
- **Compiler**: MSVC 14.51 (Visual Studio 18 Insiders), Release x64
- **Java**: JBR 21.0.9 (OpenJDK, bundled with Android Studio)

> Earlier results measured on an Apple M4 Pro / macOS machine can be found in the git history of this document.

Test Cases:

- Use 1~10 threads to write logs simultaneously;
- Each thread writes 2,000,000 log entries:
  - One with 4 parameters formatted log;
  - One with no parameter plain text log;
- Wait for all threads to finish, then flush everything to disk (`force_flush_all_logs()` for BqLog, `spdlog::shutdown()` for spdlog, `FlushLogFiles` for glog, `poll(true)` for fmtlog, `flush_log()` for quill, `LoggerContext.stop()` for Log4j2), count total time from start of writing to all logs flushed to disk.

Comparison Objects:

- BqLog 2.5.0 (C++, TextFileAppender, CompressedFileAppender, and CompressedFileAppender with Encryption)
- spdlog 1.17.0 (**async** file logger: 8192-slot queue, 1 backend thread, blocking overflow policy — no log dropping; timing ends after `spdlog::shutdown()` has drained the queue and flushed to disk)
- glog 0.7.1 (synchronous file logger, stream-based API — **glog has no async mode**, see the note below)
- fmtlog (async file logger, compiled with `FMTLOG_BLOCK=1` to prevent log dropping)
- quill 11.1.0 (async file logger, configured per official benchmark: busy-spin backend)
- Log4j2 2.23.1 (Java, AsyncLogger with Disruptor + Async Appender)

The complete, runnable benchmark project for all libraries (CMake + FetchContent, one executable per library, PowerShell runners for throughput and peak memory) is kept at the [`benchmark_2.5.0` tag](https://github.com/Tencent/BqLog/tree/benchmark_2.5.0/benchmark/cross).

### 2. Benchmark results

The BqLog figures below measure the C++ API. Other language wrappers generally
add runtime, foreign-call and argument-processing overhead. Performance varies
by language and workload; compare each language's benchmark under matching
conditions.

All time costs are in milliseconds, smaller values mean higher performance.

#### 2.1 Throughput — Total Time Cost with 4 parameters (ms)

|                              | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|------------------------------|----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|------------|
| BqLog Compress (C++)         | 95       | 144       | 210       | 210       | 226       | 267       | 362       | 395       | 439       | 507        |
| BqLog Compress+Encrypt (C++) | 102      | 166       | 167       | 190       | 236       | 308       | 350       | 391       | 453       | 493        |
| BqLog Text (C++)             | 258      | 513       | 777       | 1054      | 1324      | 1587      | 1891      | 2143      | 2465      | 2811       |
| fmtlog                       | 672      | 1219      | 1766      | 2428      | 3024      | 3923      | 4612      | 5935      | 6293      | 7934       |
| quill                        | 639      | 1429      | 2232      | 3082      | 3915      | 4726      | 5609      | 6246      | 6957      | 7812       |
| Log4j2 (Java)                | 873      | 1484      | 2087      | 2727      | 3738      | 4541      | 4889      | 6127      | 9475      | 7192       |
| spdlog (async)               | 560      | 1649      | 3402      | 5737      | 9069      | 13827     | 21494     | 24518     | 28463     | 32939      |
| glog                         | 4485     | 8548      | 14875     | 21387     | 28295     | 36060     | 45742     | 62368     | 102370    | 127550     |

#### 2.2 Peak Memory Usage (MB)

Measured as the Windows process peak working set, sampled every 5 ms for the whole run. Each benchmark runs as a separate process. The BqLog process hosts all three appenders (Text / Compress / Compress+Encrypt) at the same time; each of the other processes hosts a single logger.

|                                          | 1 Thread | 4 Threads | 10 Threads |
|------------------------------------------|----------|-----------|------------|
| BqLog (all 3 appenders in one process)   | 12.7     | 13.3      | 14.7       |
| spdlog (async)                           | 14.4     | 14.4      | 14.7       |
| glog                                     | 11.9     | 12.1      | 12.4       |
| fmtlog                                   | 17.1     | 20.2      | 23.3       |
| quill                                    | 282.1    | 1062.7    | 2714.9     |
| Log4j2 (Java)                            | 1537.8   | 6884.3    | 4631.5     |

> quill's unbounded per-thread SPSC queues double up to 64 MiB each under sustained load, which explains its multi-GB peak at 10 threads. Log4j2's numbers include the JVM heap and GC activity and fluctuate noticeably between runs.

#### 2.3 Output File Size Comparison (1 thread, 4M log entries)

| Library | Format | File Size | Bytes/Entry |
|---------|--------|-----------|-------------|
| BqLog Compress | Binary (compressed) | 45 MB | 12 B |
| BqLog Compress+Encrypt | Binary (encrypted) | 45 MB | 12 B |
| BqLog Text | Text | 283 MB | 74 B |
| spdlog (async) | Text | 285 MB | 75 B |
| glog | Text | 314 MB | 82 B |
| fmtlog | Text | 270 MB | 71 B |
| quill | Text | 247 MB | 65 B |
| Log4j2 | Text | 410 MB (2M entries, multi_param only) | 215 B |

#### 2.4 Summary

- **BqLog Compress** achieves the highest throughput — **7–16x faster than fmtlog**, **9–22x faster than Log4j2**, **6–65x faster than spdlog (async)**, **47–252x faster than glog**
- Even **BqLog Text** outperforms all other text-based loggers at every thread count (**2.3–2.8x faster than fmtlog**, the fastest competitor)
- **Encryption adds near-zero overhead** — BqLog Compress vs Compress+Encrypt performance is nearly identical
- **Memory efficient** — BqLog uses only **12.7–14.7 MB** peak working set even with three appenders active simultaneously
- **Compressed format is 6.3x smaller** than text output, reducing storage and I/O costs

> Notes on fairness:
> - **spdlog** is benchmarked in its **async mode** (`async_logger` + thread pool), its recommended high-throughput configuration. The queue uses the default blocking overflow policy, so no log entry is ever dropped, and the timed region ends only after `spdlog::shutdown()` has drained the queue and flushed the file — the same "everything on disk" semantics as the other libraries.
> - **glog** is synchronous **by design**: the library offers no async mode, so its numbers inherently reflect synchronous, in-thread formatting and writing. It also does not support `{fmt}`-style formatting; the parameterized test uses its standard stream-based `operator<<` API.
> - **fmtlog** was compiled with `FMTLOG_BLOCK=1` to prevent silent log dropping (its default behavior). The final flush stops the polling thread first and then drains synchronously in a `poll(true)` loop — calling `poll()` concurrently with the polling thread is unsafe and loses the tail. Every run was verified to land exactly 2,000,000 × thread_count entries on disk. **quill** was configured per its official benchmark with a busy-spin backend for maximum performance.

### 3. Feature Comparison

| Feature | BqLog | spdlog | glog | fmtlog | quill | Log4j2 |
|---------|-------|--------|------|--------|-------|--------|
| Async logging | ✅ | ✅ (used in this benchmark) | ❌ (not supported) | ✅ | ✅ | ✅ |
| Real-time compression | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ (rolling gzip) |
| Log encryption | ✅ (RSA+AES hybrid) | ❌ | ❌ | ❌ | ❌ | ❌ |
| Crash recovery | ✅ (Recovery) | ❌ | ✅ (signal handler) | ❌ | ✅ (signal handler) | ❌ |
| Multi-language | ✅ (C++/Java/C#/Python/TypeScript/ArkTS/Go) | ❌ (C++ only) | ❌ (C++ only) | ❌ (C++ only) | ❌ (C++ only) | Java only |
| Cross-platform | ✅ (Win/Mac/Linux/iOS/Android/HarmonyOS) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | ✅ (Win/Mac/Linux) | JVM |
| `{fmt}` formatting | ✅ | ✅ | ❌ (stream) | ✅ | ✅ | ✅ (similar) |
| No-heap-alloc hot path | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| Game engine plugins | ✅ (Unity/Unreal) | ❌ | ❌ | ❌ | ❌ | ❌ |

### 4. Appendix: Benchmark Source Code

The sources below mirror the runnable project at the [`benchmark_2.5.0` tag](https://github.com/Tencent/BqLog/tree/benchmark_2.5.0/benchmark/cross).

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

#### 4.2 spdlog Benchmark code (async)

spdlog is benchmarked in its async mode — `async_logger` over a thread pool with a
dedicated backend thread. The overflow policy defaults to `block`, so no log entries
are dropped (comparable to fmtlog's `FMTLOG_BLOCK=1`). Timing ends after
`spdlog::shutdown()`, which drains the queue and flushes the sink to disk — the same
"written to disk" semantics as the other benchmarks.

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
static const size_t QUEUE_SIZE = 8192;      // spdlog default async queue slots
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
    spdlog::shutdown(); // drains queue, flushes sink to disk
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

#### 4.3 glog Benchmark code

> Note: glog is synchronous **by design** — the library has no async mode. It also does
> not support `{fmt}`-style formatting; it uses stream-based `operator<<` as its standard API.

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

#### 4.4 fmtlog Benchmark code

> Note: `FMTLOG_BLOCK=1` is required to prevent silent log dropping (fmtlog's default
> behavior drops logs when the queue is full). fmtlog's `setLogFile()` races with its
> polling thread, so the two tests run in separate process invocations (`argv[2]` = `mp` | `np`).

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
        fmtlog::stopPollingThread();
        for (int drain = 0; drain < 10; ++drain) {
            fmtlog::poll(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|multi_param|" << thread_count << "|" << ms << std::endl;
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
        fmtlog::stopPollingThread();
        for (int drain = 0; drain < 10; ++drain) {
            fmtlog::poll(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|no_param|" << thread_count << "|" << ms << std::endl;
    }

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

#### 4.6 Log4j Benchmark code

Log4j2 part only tests text output format, because its gzip compression is "re-gzip compression on existing text files during rolling", which is completely different from BqLog's real-time compression mode performance model, and cannot be directly benchmarked.

Dependencies are plain jars fetched from Maven Central by `log4j/fetch_deps.ps1` (no Maven required):

- log4j-api 2.23.1, log4j-core 2.23.1, disruptor 3.4.2

Enable AsyncLogger (`log4j2.component.properties` on the classpath):

```properties
log4j2.contextSelector=org.apache.logging.log4j.core.async.AsyncLoggerContextSelector
```

Log4j2 Configuration (`log4j2.xml` on the classpath):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="WARN">
  <Appenders>
    <!-- RollingRandomAccessFile, for text output demo -->
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

Source Code:

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

        // stop() drains the Disruptor ring buffer and flushes the appender
        ((org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false)).stop();
        LogManager.shutdown();

        long flush_time = System.currentTimeMillis();
        System.out.println("RESULT|log4j2|" + (multi_param ? "multi_param" : "no_param")
            + "|" + thread_count + "|" + (flush_time - start_time));
    }
}
```
