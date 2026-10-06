# BqLog benchmark 2.6.0

Cross-library logging benchmark for **BqLog 2.6.0**: the code, the runner scripts and the raw data behind [docs/BENCHMARK.md](https://github.com/Tencent/BqLog/blob/Release_2.6.0/docs/BENCHMARK.md) ([简体中文](https://github.com/Tencent/BqLog/blob/Release_2.6.0/docs/BENCHMARK_CHS.md)) of the 2.6.0 release. Benchmarks of other versions: [index on the `benchmark` branch](https://github.com/Tencent/BqLog/tree/benchmark).

Third-party libraries are fetched at build time and are not part of this repository; their licenses are listed in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## What it measures

1. **Throughput** (`bench_<lib>`): 1, 2, 4, 6, 8 and 10 threads, 2,000,000 entries per thread, timed until everything is on disk, with the CPU time of the whole process and its peak memory. Fixed size buffers that block when full for every library, plus a group with growing buffers (BqLog `expand`, quill's default unbounded queue).
2. **Latency on the logging thread** (`bench_latency_<lib>`, one source `bench_latency.cpp` for every library): every thread writes batches of 20 calls with a 2000-2200 us busy wait between them, each batch timed with a serialized hardware counter; p50 / p99 / p99.9 / mean per call, next to the consumer CPU and peak memory.

BqLog C++ is built in both its normal mode (`log.info`) and its fast mode (`BQ_LOG_FAST_INFO`).

## Library versions

| Library | Version | Configuration |
|---|---|---|
| BqLog | 2.6.0 (built from the main repository's `src/`) | default configuration |
| quill | v13.0.0 | `BoundedBlocking` 64 KiB per thread (on Windows with a 0 retry interval: its default 800 ns becomes `Sleep(1)`, a whole millisecond); `UnboundedBlocking` in the growing group |
| fmtlog | v2.3.0 | `FMTLOG_BLOCK=1` (its default drops entries when the queue is full) |
| spdlog | v1.17.0 | async, 8192 slots, blocking overflow policy |
| glog | v0.7.1 | synchronous by design |
| Log4j2 | 2.26.0 + Disruptor 4.0.0 | AsyncLogger, throughput only |

## How to run

`benchmark/cross` builds BqLog from the main repository's `src/` and `CMake_utils.txt`, so put this branch's `benchmark/` directory into a checkout of [Release_2.6.0](https://github.com/Tencent/BqLog/tree/Release_2.6.0) first:

```
git clone -b Release_2.6.0 https://github.com/Tencent/BqLog.git
cd BqLog
git fetch origin benchmarks/2.6.0
git checkout origin/benchmarks/2.6.0 -- benchmark README.md THIRD_PARTY_LICENSES.md
```

Build (third-party libraries come from CMake FetchContent; offline, point `-DFETCHCONTENT_SOURCE_DIR_QUILL=` and friends at local clones):

```
# Linux / macOS ("mac" on macOS)
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release
cmake --build benchmark/cross/build -j --target bench_bqlog bench_quill bench_fmtlog bench_spdlog bench_glog \
    bench_latency_bqlog_fast bench_latency_bqlog_normal bench_latency_quill bench_latency_fmtlog bench_latency_spdlog

# Windows (MSVC)
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=win64
cmake --build benchmark/cross/build --config Release
```

Run on AC power with nothing else busy. Results go to `$RUN_DIR` (default `benchmark/cross/run`).

```
# Linux / macOS: throughput, 3 rounds for the libraries compared closely, 1 round for the slow ones
export RUN_DIR=$PWD/benchmark/cross/run THREADS="1 2 4 6 8 10"
for r in 1 2 3; do for lib in bqlog quill fmtlog; do ./benchmark/cross/run_benchmark.sh $lib; done; done
THREADS="1 2 4 6" ./benchmark/cross/run_benchmark.sh spdlog
THREADS="1 2 4 6" ./benchmark/cross/run_benchmark.sh glog
./benchmark/cross/log4j/fetch_deps.sh && ./benchmark/cross/log4j/run_benchmark.sh
# latency on the logging thread: 5 interleaved rounds, 1 and 4 threads
./benchmark/cross/run_latency.sh 5 1 4

# Windows
powershell -File benchmark/cross/run_benchmark.ps1 -Lib bqlog
powershell -File benchmark/cross/log4j/fetch_deps.ps1; powershell -File benchmark/cross/log4j/run_benchmark.ps1
powershell -File benchmark/cross/run_latency.ps1 -Rounds 5 -Threads 1,4
```

A full run on an Apple M4 Pro takes about 20 minutes. Then turn the CSV files into the tables and charts of the docs (medians of the rounds):

```
python3 benchmark/cross/make_tables.py benchmark/cross/run en          # tables, "chs" for Chinese
python3 benchmark/cross/make_tables.py benchmark/cross/run en charts   # Mermaid charts
```

## Results

`benchmark/cross/results_mac/`: Apple M4 Pro (10 performance + 4 efficiency cores), 48 GB, macOS 15.6.1, Apple clang 17, 2026-10-05.

- `results.csv`: throughput, every round (`lib,test,threads,ms,cpu_ms,peak_mb`)
- `latency.csv`: latency on the logging thread, every round
- `filesizes.txt`: output file sizes of the 1-thread run

`benchmark/cross/results_win/`: AMD Ryzen 9 9950X (16 cores / 32 threads), 96 GB, Windows 11 Pro 24H2 (10.0.26100), MSVC 19.51 (Visual Studio 2026) Release x64, OpenJDK 25.0.2, 2026-10-06, BqLog built with the MSVC atomic fix that follows 2.6.0 on `develop`. Same files as above.
