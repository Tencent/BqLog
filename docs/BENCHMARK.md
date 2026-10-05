# Benchmark

[← Back to Home](../README.md) | [简体中文](./BENCHMARK_CHS.md)

## Why two parts

A modern machine runs two kinds of work for every log entry: the **logging thread** that calls `info()`, and the **consumer** — the background thread that formats, compresses and writes. Both run on real cores of the same device. A logger can make the logging thread look fast by pushing work to the consumer, by spinning the consumer on a full core, or by letting a queue grow without bound; the cost has only moved. That is a seesaw, not a speed-up.

BqLog works on both sides at once, and this benchmark measures both:

1. **Total cost (throughput).** How long it takes from the first log call until every entry is on disk. This is the whole load a logger puts on the machine — producer and consumer together — and the number that decides whether a busy server or game keeps up.
2. **Cost on the logging thread.** How long the calling thread itself spends inside log calls, in two realistic patterns, reported together with the consumer CPU and memory it took to get there.

Both parts use realistic workloads: real files on disk, the same log line with 4 arguments everywhere, every library in its recommended asynchronous configuration with a consumer that sleeps when idle, and no entry dropped.

BqLog C++ is measured in its two modes:

- **Normal mode** — `log.info(...)`, the default API, with the best IDE support.
- **C++ only fast mode** — `BQ_LOG_FAST_INFO(log, ...)`. Same output, much less work on the logging thread; slightly more memory, weaker IDE hints, and a fixed log object and format string per call site ([details](API_REFERENCE.md#3-write-logs)).

## 1. Environments and versions

| | macOS | Windows |
|---|---|---|
| Machine | MacBook Pro, Apple M4 Pro (10 performance + 4 efficiency cores), 48 GB | PC, AMD Ryzen 9 9950X (16 cores / 32 threads), 96 GB |
| OS | macOS 15.6.1 | Windows 11 Pro (10.0.26100) |
| Compiler | Apple clang 17, Release, arm64 | MSVC 14.51, Release x64 |
| Java | OpenJDK 21.0.8 | JBR 21.0.9 |
| Measured | 2026-10-05, BqLog 2.6.0 | 2026-09-27, BqLog 2.5.0 (to be re-measured on 2.6.0) |

Libraries, all at their latest release (2026-10):

- **BqLog 2.6.0** — Text, Compress and Compress+Encrypt appenders, normal mode and fast mode
- **quill 13.0.0** — default `UnboundedBlocking` queue; default backend (sleeps 100 µs when idle) in the latency part, busy-spin backend per quill's own benchmark in the throughput part
- **fmtlog 2.3.0** — compiled with `FMTLOG_BLOCK=1` (its default drops entries when the queue is full)
- **spdlog 1.17.0** — async logger, 8192-slot queue, blocking overflow policy
- **glog 0.7.1** — synchronous by design (it has no async mode)
- **Log4j2 2.26.0** with Disruptor 4.0.0 — AsyncLogger, throughput part only (Java)

The complete runnable project — CMake with FetchContent, one executable per library, runner scripts for every table below — lives on the [`benchmark` branch](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross). The figures here measure the C++ API; other language wrappers add their own runtime and call overhead.

## 2. Part one: total cost (throughput)

1–10 threads write at the same time; each thread writes 2,000,000 entries. Timing runs from the start until every entry has been flushed to disk (`force_flush_all_logs()` for BqLog, `spdlog::shutdown()`, `FlushLogFiles`, a draining `poll(true)` for fmtlog, `flush_log()` for quill, `LoggerContext.stop()` for Log4j2). Every run was checked to land exactly 2,000,000 × threads entries in the file.

### 2.1 macOS (Apple M4 Pro)

#### Total time, 4 parameters (ms, lower is better)

| | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog Compress, fast mode (C++) | 36 | 70 | 109 | 145 | 182 | 238 | 256 | 313 | 347 | 411 |
| BqLog Compress+Encrypt, fast mode (C++) | 46 | 78 | 123 | 158 | 193 | 248 | 270 | 311 | 358 | 456 |
| BqLog Text, fast mode (C++) | 178 | 334 | 480 | 623 | 805 | 1003 | 1098 | 1280 | 1471 | 1865 |
| BqLog Compress, normal mode (C++) | 53 | 79 | 113 | 148 | 209 | 244 | 272 | 313 | 357 | 438 |
| BqLog Compress+Encrypt, normal mode (C++) | 45 | 84 | 120 | 159 | 209 | 246 | 285 | 326 | 374 | 457 |
| BqLog Text, normal mode (C++) | 169 | 304 | 440 | 603 | 785 | 1010 | 1077 | 1234 | 1428 | 1736 |
| fmtlog | 270 | 534 | 800 | 1065 | 1382 | 1602 | 1885 | 2091 | 2500 | 2899 |
| quill | 361 | 725 | 1112 | 1459 | 1811 | 2221 | 2597 | 3004 | 3323 | 3953 |
| Log4j2 (Java) | 740 | 1024 | 1420 | 1750 | 2313 | 2483 | 3005 | 3469 | 3785 | 3808 |
| spdlog (async) | 573 | 1681 | 3520 | 6904 | 13843 | 25105 | 35947 | 42858 | 49091 | 56292 |
| glog | 2413 | 4135 | 6635 | 9958 | 13673 | 21834 | 28843 | 34818 | 40268 | 44857 |

#### Total time, no parameter (ms, lower is better)

| | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog Compress, fast mode (C++) | 38 | 72 | 103 | 122 | 137 | 167 | 197 | 235 | 255 | 303 |
| BqLog Compress+Encrypt, fast mode (C++) | 38 | 67 | 109 | 129 | 137 | 168 | 203 | 222 | 259 | 319 |
| BqLog Text, fast mode (C++) | 87 | 160 | 211 | 312 | 369 | 452 | 524 | 633 | 696 | 916 |
| BqLog Compress, normal mode (C++) | 25 | 64 | 77 | 107 | 177 | 172 | 188 | 215 | 255 | 321 |
| BqLog Compress+Encrypt, normal mode (C++) | 32 | 63 | 78 | 105 | 138 | 167 | 231 | 224 | 252 | 324 |
| BqLog Text, normal mode (C++) | 98 | 173 | 201 | 284 | 351 | 428 | 497 | 573 | 647 | 856 |
| fmtlog | 156 | 305 | 495 | 666 | 830 | 1031 | 1166 | 1381 | 1481 | 1932 |
| quill | 223 | 462 | 721 | 946 | 1178 | 1442 | 1744 | 2017 | 2215 | 2532 |
| spdlog (async) | 500 | 1480 | 3245 | 6064 | 12925 | 24201 | 33739 | 40360 | 46564 | 55105 |
| glog | 1761 | 3140 | 4956 | 7283 | 10530 | 19427 | 26027 | 31964 | 37053 | 40587 |

#### Peak memory of the throughput run (MB)

Peak resident set size of the whole process, from the kernel's high-water mark. The BqLog process hosts all six configurations (three appenders × two modes) at once; every other process hosts one logger.

| | 1 Thread | 4 Threads | 10 Threads |
|---|---:|---:|---:|
| BqLog (6 configurations in one process) | 5.4 | 6.6 | 8.5 |
| quill | 143.2 | 560.8 | 1405.7 |
| fmtlog | 2.6 | 5.7 | 12.2 |
| spdlog (async) | 5.0 | 8.5 | 8.6 |
| glog | 1.6 | 2.0 | 2.5 |

#### Output file size (1 thread, 4 million entries)

| | Format | Size | Bytes per entry |
|---|---|---:|---:|
| BqLog Compress (either mode) | binary | 47 MB | 12 |
| BqLog Compress+Encrypt (either mode) | binary, encrypted | 47 MB | 12 |
| BqLog Text (either mode) | text | 317 MB | 79 |
| quill | text | 263 MB | 66 |
| fmtlog | text | 299 MB | 75 |
| spdlog (async) | text | 303 MB | 76 |
| glog | text | 349 MB | 87 |
| Log4j2 | text | 213 MB (2 million entries, 4 parameters only) | 106 |

#### Reading the macOS throughput numbers

- **BqLog Compress is 5–7x faster than fmtlog, 7–10x faster than quill and 9–14x faster than Log4j2**, with 4 parameters; spdlog (async) and glog are one to two orders of magnitude behind.
- **BqLog Text** — formatting every entry into a full text line — is still **1.6–1.8x faster than fmtlog** and 2.1–2.5x faster than quill, the fastest text loggers here.
- **Fast mode does not hand its gain back on the consumer.** Total throughput is about the same in both modes: within a few percent for most thread counts, a little faster for Compress, a little slower for Text. The consumer reads fast mode records in place, so what fast mode saves on the logging thread is not paid again on the background core.
- **Encryption is close to free**: Compress+Encrypt stays within a few percent of Compress at 2–10 threads.
- **Memory**: quill's per-thread queues grow under sustained load (up to 1.4 GB at 10 threads); BqLog stays within single-digit megabytes with six loggers in one process.

### 2.2 Windows (AMD Ryzen 9 9950X)

> Measured on BqLog 2.5.0 (normal mode only) with quill 11.1.0, fmtlog @ 308b231 and Log4j2 2.23.1. To be re-measured on 2.6.0 with the library versions listed above.

#### Total time, 4 parameters (ms, lower is better)

| | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads | 9 Threads | 10 Threads |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog Compress (C++) | 95 | 144 | 210 | 210 | 226 | 267 | 362 | 395 | 439 | 507 |
| BqLog Compress+Encrypt (C++) | 102 | 166 | 167 | 190 | 236 | 308 | 350 | 391 | 453 | 493 |
| BqLog Text (C++) | 258 | 513 | 777 | 1054 | 1324 | 1587 | 1891 | 2143 | 2465 | 2811 |
| fmtlog | 672 | 1219 | 1766 | 2428 | 3024 | 3923 | 4612 | 5935 | 6293 | 7934 |
| quill | 639 | 1429 | 2232 | 3082 | 3915 | 4726 | 5609 | 6246 | 6957 | 7812 |
| Log4j2 (Java) | 873 | 1484 | 2087 | 2727 | 3738 | 4541 | 4889 | 6127 | 9475 | 7192 |
| spdlog (async) | 560 | 1649 | 3402 | 5737 | 9069 | 13827 | 21494 | 24518 | 28463 | 32939 |
| glog | 4485 | 8548 | 14875 | 21387 | 28295 | 36060 | 45742 | 62368 | 102370 | 127550 |

#### Peak memory (MB), Windows peak working set

| | 1 Thread | 4 Threads | 10 Threads |
|---|---:|---:|---:|
| BqLog (3 appenders in one process) | 12.7 | 13.3 | 14.7 |
| spdlog (async) | 14.4 | 14.4 | 14.7 |
| glog | 11.9 | 12.1 | 12.4 |
| fmtlog | 17.1 | 20.2 | 23.3 |
| quill | 282.1 | 1062.7 | 2714.9 |
| Log4j2 (Java) | 1537.8 | 6884.3 | 4631.5 |

## 3. Part two: cost on the logging thread

### 3.1 How it is measured

Each logging thread writes **bursts of 100 log calls** and times every burst with `steady_clock` — one clock read per burst, so the clock's own cost and resolution are spread over 100 calls. Two scenarios:

- **busy** — bursts back to back. The thread logs non-stop and keeps the consumer fully loaded; this is where a producer meets a full buffer.
- **occasional** — a 1 ms sleep after every burst. This is how most applications log: in short spurts, coming back to the logger after other work, with cold caches.

For each scenario we report, per log call:

- **mean** — total time spent in log calls ÷ number of calls. Every stall is included, so this is the share of the thread's time that logging takes. If you read only one number, read this one.
- **p50** — a typical burst.
- **p99** — the slowest 1% of bursts: blocking on a full buffer, page faults, interference from the consumer.

We report percentiles over bursts, not a single per-call histogram, on purpose: with an ordinary clock, a single call of a few nanoseconds cannot be measured honestly, while a burst of 100 can.

And next to them, **what the speed costs**: the **consumer CPU** (CPU time of everything but the logging threads, as % of one core) and the **peak memory** of the process. A logging thread can be made almost free by giving the consumer a whole core, or by letting the queue grow without limit; those two columns show whether that happened.

All consumers run in their default, idle-sleeping configuration (quill's backend sleeps 100 µs, BqLog's worker sleeps until woken), all loggers write text files with the same line, and each scenario runs 7 interleaved rounds; the tables show the median.

### 3.2 macOS (Apple M4 Pro)

#### busy, 1 thread

| | mean (ns) | p50 (ns) | p99 (ns) | consumer CPU | peak memory (MB) |
|---|---:|---:|---:|---:|---:|
| BqLog, fast mode | 75.4 | 5.0 | 904.2 | 100% | 2.6 |
| BqLog, normal mode | 79.1 | 13.3 | 376.2 | 100% | 2.6 |
| quill | 5.9 | 2.1 | 17.1 | 97% | 143.4 |
| fmtlog | 118.3 | 115.0 | 186.7 | 99% | 2.8 |
| spdlog (async) | 288.1 | 277.5 | 492.9 | 94% | 5.3 |

#### busy, 4 threads

| | mean (ns) | p50 (ns) | p99 (ns) | consumer CPU | peak memory (MB) |
|---|---:|---:|---:|---:|---:|
| BqLog, fast mode | 287.1 | 7.5 | 4930.0 | 100% | 4.3 |
| BqLog, normal mode | 280.3 | 12.1 | 1967.9 | 100% | 4.2 |
| quill | 8.9 | 2.5 | 17.1 | 97% | 566.8 |
| fmtlog | 529.1 | 496.7 | 1625.0 | 100% | 7.3 |
| spdlog (async) | 3492.3 | 3363.3 | 6442.9 | 83% | 6.8 |

#### occasional, 1 thread

| | mean (ns) | p50 (ns) | p99 (ns) | consumer CPU | peak memory (MB) |
|---|---:|---:|---:|---:|---:|
| BqLog, fast mode | 47.5 | 24.6 | 287.9 | 3% | 2.3 |
| BqLog, normal mode | 92.4 | 55.8 | 542.1 | 3% | 2.3 |
| quill | 4.2 | 3.8 | 11.2 | 5% | 6.4 |
| fmtlog | 47.1 | 35.4 | 199.2 | 6% | 2.5 |
| spdlog (async) | 651.6 | 492.1 | 3471.7 | 7% | 5.0 |

#### occasional, 4 threads

| | mean (ns) | p50 (ns) | p99 (ns) | consumer CPU | peak memory (MB) |
|---|---:|---:|---:|---:|---:|
| BqLog, fast mode | 34.9 | 18.3 | 210.0 | 8% | 2.8 |
| BqLog, normal mode | 68.5 | 41.7 | 387.9 | 8% | 2.8 |
| quill | 4.0 | 3.8 | 12.1 | 10% | 14.3 |
| fmtlog | 43.2 | 35.8 | 165.0 | 12% | 5.9 |
| spdlog (async) | 963.6 | 792.9 | 3523.3 | 20% | 5.3 |

#### Reading the logging thread numbers

- **quill has the cheapest logging thread on this machine**, 4–9 ns per call in every scenario. In the busy scenario that comes with memory: its queues grow to 143 MB with one thread and 567 MB with four, where every other logger stays under 8 MB. In the occasional scenario its backend also polls every 100 µs while the application is idle, which is why its consumer uses more CPU than BqLog's while doing the same work.
- **BqLog's fast mode keeps a typical burst at 5–8 ns per call while the consumer is under full load** (busy, p50), about 2x below normal mode. The busy mean is much higher, for both modes, because the busy scenario produces text faster than any consumer here can format it: BqLog's default buffer (64 KB per thread) fills, and the logging thread waits for the consumer instead of growing memory. That wait is the default `block` policy at work; `log.buffer_policy_when_full=expand` or a larger `log.buffer_size` trades it for memory, which is what quill's default unbounded queue does. Part one shows how quickly BqLog's consumer works through such a backlog.
- **In the occasional scenario, fast mode costs about half of normal mode** (18–25 ns vs 42–56 ns typical per call), but quill stays far ahead at 4 ns. Our measurements show where that gap comes from: the first call after the 1 ms sleep costs BqLog's fast mode about 1,800 cycles against quill's 500, and the following calls of the burst run at roughly half the clock speed. quill's backend wakes every 100 µs even when there is nothing to do, which keeps the core warm and clocked up; BqLog's worker sleeps until there is work. When we let a dummy thread wake every 100 µs next to BqLog, the same fast mode burst dropped from about 51 to 16 cycles per call. In other words, this part of quill's lead is bought with a consumer that never really goes to sleep — a trade-off BqLog deliberately does not make. Narrowing this gap without that trade-off is on our list.
- **BqLog's consumer is the lightest** in the occasional scenario (3–8% of a core, against 5–20% for the others), and in total cost (part one) it finishes the same work 7–10x sooner than quill.

## 4. Feature comparison

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

## 5. Notes on fairness

- **Everyone writes everything.** No logger drops entries: fmtlog runs with `FMTLOG_BLOCK=1`, spdlog with its blocking overflow policy, BqLog with its default `block` policy, quill with its default blocking queue. Every throughput run was verified to land exactly 2,000,000 × threads entries.
- **quill** uses its own benchmark's busy-spin backend in the throughput part (its fastest setting) and its default sleeping backend in the latency part, where every consumer must be in the same, realistic mode.
- **spdlog** is measured in its async mode, its recommended high-throughput configuration; timing ends after `spdlog::shutdown()` has drained the queue.
- **glog** is synchronous by design and uses its stream API; it has no `{fmt}`-style formatting.
- **fmtlog**: `setLogFile()` races with its polling thread, so its two throughput tests run in separate processes, and the final flush stops the polling thread first and drains with `poll(true)`.
- **Log4j2** is in the throughput part only; its numbers include the JVM.

## 6. Appendix: benchmark source code

The sources live on the [`benchmark` branch](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross): `bench_<lib>.cpp` for the throughput part (one executable per library), `bench_latency.cpp` for the logging thread part (one source, built once per library), `run_benchmark.sh`/`.ps1`, `run_latency.sh`, `measure_memory.sh`/`.ps1`, the Log4j2 project under `log4j/`, and `make_tables.py`, which turns the CSV output into the tables above.

### 6.1 BqLog, throughput

```cpp
// normal mode: log.info
static void run_test(bq::log& log_obj, const char* lib_name, int thread_count, bool multi_param)
{
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t, &log_obj, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    log_obj.info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    log_obj.info("Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    bq::log::force_flush_all_logs();
    report(lib_name, thread_count, multi_param, start);
}

// C++ only fast mode: BQ_LOG_FAST_* macros. A call site binds the log object of its first call,
// so every log gets its own instantiation (and with it its own call sites).
template <int LOG_TAG>
static void run_fast_test(bq::log& log_obj, const char* lib_name, int thread_count, bool multi_param)
{
    // ... same loop, with
    //     BQ_LOG_FAST_INFO(log_obj, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
}
```

Appender configuration (Text uses `type=text_file`; Compress+Encrypt adds `pub_key=...`):

```
log.high_perform_mode_freq_threshold_per_second=1
appenders_config.appender_0.type=compressed_file
appenders_config.appender_0.levels=[all]
appenders_config.appender_0.file_name=output/bqlog_compress
appenders_config.appender_0.always_create_new_file=true
```

### 6.2 Logging thread part (all libraries)

```cpp
static void producer(int t, bool occasional, std::vector<double>& out, double& cpu_ms)
{
    const double cpu_start = thread_cpu_ms();
    const int bursts = occasional ? OCCASIONAL_BURSTS : BUSY_BURSTS; // 2000 : 20000
    int counter = 0;
    for (int w = 0; w < 100 * BURST_CALLS; ++w) {                     // warm up
        log_one(t, counter++);
    }
    for (int b = 0; b < bursts; ++b) {
        const auto start = std::chrono::steady_clock::now();
        for (int k = 0; k < BURST_CALLS; ++k) {                        // 100 calls
            log_one(t, counter++);
        }
        const auto end = std::chrono::steady_clock::now();
        out.push_back(std::chrono::duration<double, std::nano>(end - start).count() / BURST_CALLS);
        if (occasional) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    cpu_ms = thread_cpu_ms() - cpu_start;
}
```

`log_one` is the only line that differs between libraries, for example `g_log->info(...)` / `BQ_LOG_FAST_INFO(*g_log, ...)` for BqLog and `LOG_INFO(g_logger, ...)` for quill, always with `"idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true`. Consumer CPU is process CPU time minus the logging threads' own CPU time, divided by the wall time.

### 6.3 Other libraries, throughput

The spdlog, glog, fmtlog, quill and Log4j2 throughput programs follow the same structure as 6.1 — same thread layout, same two log lines, timing until everything is on disk — with each library's own async setup described in section 1 and section 5. See `bench_spdlog.cpp`, `bench_glog.cpp`, `bench_fmtlog.cpp`, `bench_quill.cpp` and `log4j/src/bq/benchmark/log4j/main.java` on the benchmark branch.
