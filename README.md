# benchmark

Cross-library logging performance comparison: benchmark code and measured data. The main project lives on `main` / `develop`; this orphan branch only carries the benchmark.

## Environment

- AMD Ryzen 9 9950X (16 cores / 32 threads), 96 GB, Windows 11 Pro 26100
- C++: MSVC 14.51 (VS 18 Insiders), Release x64
- Java: JBR 21.0.9 (bundled with Android Studio)

## Library versions and configurations

- BqLog: develop @ 2598992a
- spdlog v1.17.0 — async mode (8192-slot queue + 1 backend thread, blocking overflow policy, nothing dropped; timing includes the final shutdown drain/flush)
- glog v0.7.1 — the library has no async mode
- fmtlog main @ 308b231 — built with FMTLOG_BLOCK=1 (its default drops logs when the queue is full)
- quill v11.1.0 — per its official benchmark config, busy-spin backend
- Log4j2 2.23.1 + disruptor 3.4.2 — AsyncLogger + Async Appender

## How to run

benchmark/cross depends on the main repo's src/ and CMake_utils.txt, so overlay this branch's benchmark/ directory onto a develop checkout first.

Windows (MSVC):

```
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=win64
cmake --build benchmark/cross/build --config Release
```

Linux / macOS:

```
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release   # use "unix" on macOS
cmake --build benchmark/cross/build -j
```

Third-party libraries are fetched by CMake FetchContent. For offline environments, clone them yourself and point CMake at the local copies with `-DFETCHCONTENT_SOURCE_DIR_SPDLOG=` etc. One executable per library; use the runner scripts:

```
# Windows
powershell -File benchmark/cross/run_benchmark.ps1 -Lib bqlog      # bqlog/spdlog/glog/fmtlog/quill, threads 1~10
powershell -File benchmark/cross/measure_memory.ps1 -Lib bqlog -Threads 1,4,10

# Linux / macOS (verified on WSL Ubuntu 24.04)
./benchmark/cross/run_benchmark.sh bqlog 1 10
./benchmark/cross/measure_memory.sh bqlog 1 4 10
```

Log4j2 runs separately:

```
# Windows
powershell -File benchmark/cross/log4j/fetch_deps.ps1              # downloads jars into log4j/lib
javac -cp "lib/*" -d classes src/bq/benchmark/log4j/main.java      # inside log4j/
powershell -File benchmark/cross/log4j/run_benchmark.ps1

# Linux / macOS (JDK 17+; the script compiles automatically)
./benchmark/cross/log4j/fetch_deps.sh
./benchmark/cross/log4j/run_benchmark.sh 1 10
```

## Results

Raw data measured on 2026-09-27 lives in benchmark/cross/run/ (csv files and filesizes.txt). The summary tables are in the main repo's docs/BENCHMARK.md (and its Chinese version).

Note: fmtlog's setLogFile races with its polling thread, so its two test cases run in separate processes (second argument mp/np).
