# benchmark

跨库日志性能对比的代码和实测数据存档。主仓库代码在 main / develop，本分支只放 benchmark。

## 环境

- AMD Ryzen 9 9950X（16 核 32 线程），96 GB，Windows 11 Pro 26100
- C++ 部分：MSVC 14.51（VS 18 Insiders），Release x64
- Java 部分：JBR 21.0.9（Android Studio 自带）

## 各库版本与配置

- BqLog：develop @ 2598992a
- spdlog v1.17.0 —— 异步模式（队列 8192 + 1 后台线程，阻塞溢出策略，不丢日志；计时含 shutdown 排空落盘）
- glog v0.7.1 —— 库本身没有异步模式
- fmtlog main @ 308b231 —— FMTLOG_BLOCK=1（默认队列满会丢日志，这里关掉）
- quill v11.1.0 —— 按官方 benchmark 配置，busy-spin 后端
- Log4j2 2.23.1 + disruptor 3.4.2 —— AsyncLogger + Async Appender

## 跑法

benchmark/cross 依赖主仓库的 src/ 和 CMake_utils.txt，先把本分支的 benchmark/ 目录覆盖到一份 develop 检出上。

Windows（MSVC）：

```
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=win64
cmake --build benchmark/cross/build --config Release
```

Linux / macOS：

```
cmake -S benchmark/cross -B benchmark/cross/build -DTARGET_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release   # macOS 用 unix
cmake --build benchmark/cross/build -j
```

第三方库由 CMake FetchContent 自动拉取；离线环境可以先自己 clone，再用 `-DFETCHCONTENT_SOURCE_DIR_SPDLOG=` 等参数指向本地目录。每个库一个 exe，统一用脚本跑：

```
# Windows
powershell -File benchmark/cross/run_benchmark.ps1 -Lib bqlog      # bqlog/spdlog/glog/fmtlog/quill，1~10 线程
powershell -File benchmark/cross/measure_memory.ps1 -Lib bqlog -Threads 1,4,10

# Linux / macOS（已在 WSL Ubuntu 24.04 上验证）
./benchmark/cross/run_benchmark.sh bqlog 1 10
./benchmark/cross/measure_memory.sh bqlog 1 4 10
```

Log4j2 单独跑：

```
# Windows
powershell -File benchmark/cross/log4j/fetch_deps.ps1              # 拉 jar 到 log4j/lib
javac -cp "lib/*" -d classes src/bq/benchmark/log4j/main.java      # 在 log4j/ 目录下
powershell -File benchmark/cross/log4j/run_benchmark.ps1

# Linux / macOS
./benchmark/cross/log4j/fetch_deps.sh
./benchmark/cross/log4j/run_benchmark.sh 1 10                      # 需要 JDK 17+，脚本会自动编译
```

## 结果

2026-09-27 本机实测原始数据在 benchmark/cross/run/ 下的 csv 和 filesizes.txt 里。汇总表见主仓库 docs/BENCHMARK.md（及中文版）。

注：fmtlog 的 setLogFile 和它的轮询线程有竞态，两个用例放在同进程里跑会崩，所以拆成两个进程（第二个参数 mp/np）。
