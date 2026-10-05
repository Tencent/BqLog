# Benchmark

[← 返回首页](../README_CHS.md) | [English](./BENCHMARK.md)

## 为什么分成两部分

一台现代设备上，每条日志都对应两份工作：调用 `info()` 的**日志线程**，以及在后台格式化、压缩、写盘的**消费线程**。两者都跑在同一台设备的真实核心上。一个日志库完全可以让日志线程看起来很快：把工作推给消费线程，让消费线程占满一个核心空转，或者让队列无上限地增长。但成本只是换了个地方。这是跷跷板，一边上去了一边下来，不是真的变快。

BqLog 两手都抓、两手都硬，这份 benchmark 也两边都测：

1. **总消耗（吞吐）。** 从第一条日志调用开始，到所有日志都落盘为止，总共花多少时间。这是日志库给整台设备带来的全部负载——生产者和消费者加在一起——也决定了繁忙的服务器或游戏能不能扛得住。
2. **日志线程自身的开销。** 调用线程本身在日志调用里花了多少时间，分两种贴近实际的写法来测，同时给出为此付出的消费端 CPU 和内存。

两部分都针对真实使用场景：写真实的磁盘文件，所有库写同一条带 4 个参数的日志，每个库都用它推荐的异步配置、消费线程空闲时休眠，并且不丢任何一条日志。

BqLog C++ 测两种模式：

- **普通模式**：`log.info(...)`，默认的 API，IDE 支持最好。
- **C++ 专属的快速模式**：`BQ_LOG_FAST_INFO(log, ...)`。输出相同，日志线程上的工作少得多；代价是内存稍多一点、IDE 代码提示体验差一点、每个调用点的日志对象和格式串固定（[详见这里](API_REFERENCE_CHS.md#3-写日志)）。

## 1. 测试环境和版本

| | macOS | Windows |
|---|---|---|
| 机器 | MacBook Pro，Apple M4 Pro（10 个性能核 + 4 个能效核），48 GB | PC，AMD Ryzen 9 9950X（16 核 / 32 线程），96 GB |
| 系统 | macOS 15.6.1 | Windows 11 专业版（10.0.26100） |
| 编译器 | Apple clang 17，Release，arm64 | MSVC 14.51，Release x64 |
| Java | OpenJDK 21.0.8 | JBR 21.0.9 |
| 测试时间 | 2026-10-05，BqLog 2.6.0 | 2026-09-27，BqLog 2.5.0（待用 2.6.0 重测） |

对比的库全部是最新正式版本（2026 年 10 月）：

- **BqLog 2.6.0**：文本、压缩、压缩+加密三种 Appender，普通模式和快速模式
- **quill 13.0.0**：默认的 `UnboundedBlocking` 队列；日志线程部分用默认后台线程（空闲时休眠 100 µs），吞吐部分按 quill 官方 benchmark 的写法用忙等后台线程
- **fmtlog 2.3.0**：用 `FMTLOG_BLOCK=1` 编译（它默认在队列满时丢日志）
- **spdlog 1.17.0**：异步 logger，8192 槽位队列，阻塞溢出策略
- **glog 0.7.1**：设计上就是同步的（没有异步模式）
- **Log4j2 2.26.0** + Disruptor 4.0.0：AsyncLogger，只参加吞吐部分（Java）

完整可运行的工程——CMake + FetchContent、每个库一个可执行文件、下面每张表对应的运行脚本——在 [`benchmark` 分支](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross)。这里的数据测的是 C++ API；其他语言的 wrapper 会额外引入各自运行时和跨语言调用的开销。

## 2. 第一部分：总消耗（吞吐）

1～10 个线程同时写日志，每个线程写 2,000,000 条。计时从开始写到所有日志都刷到磁盘为止（BqLog 用 `force_flush_all_logs()`，spdlog 用 `spdlog::shutdown()`，glog 用 `FlushLogFiles`，fmtlog 用循环 `poll(true)` 排空，quill 用 `flush_log()`，Log4j2 用 `LoggerContext.stop()`）。每一轮都核对过：文件里正好有 2,000,000 × 线程数 条日志。

### 2.1 macOS（Apple M4 Pro）

#### 总耗时，4 个参数（毫秒，越小越好）

| | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog 压缩，快速模式（C++） | 36 | 70 | 109 | 145 | 182 | 238 | 256 | 313 | 347 | 411 |
| BqLog 压缩+加密，快速模式（C++） | 46 | 78 | 123 | 158 | 193 | 248 | 270 | 311 | 358 | 456 |
| BqLog 文本，快速模式（C++） | 178 | 334 | 480 | 623 | 805 | 1003 | 1098 | 1280 | 1471 | 1865 |
| BqLog 压缩，普通模式（C++） | 53 | 79 | 113 | 148 | 209 | 244 | 272 | 313 | 357 | 438 |
| BqLog 压缩+加密，普通模式（C++） | 45 | 84 | 120 | 159 | 209 | 246 | 285 | 326 | 374 | 457 |
| BqLog 文本，普通模式（C++） | 169 | 304 | 440 | 603 | 785 | 1010 | 1077 | 1234 | 1428 | 1736 |
| fmtlog | 270 | 534 | 800 | 1065 | 1382 | 1602 | 1885 | 2091 | 2500 | 2899 |
| quill | 361 | 725 | 1112 | 1459 | 1811 | 2221 | 2597 | 3004 | 3323 | 3953 |
| Log4j2（Java） | 740 | 1024 | 1420 | 1750 | 2313 | 2483 | 3005 | 3469 | 3785 | 3808 |
| spdlog（异步） | 573 | 1681 | 3520 | 6904 | 13843 | 25105 | 35947 | 42858 | 49091 | 56292 |
| glog | 2413 | 4135 | 6635 | 9958 | 13673 | 21834 | 28843 | 34818 | 40268 | 44857 |

#### 总耗时，无参数（毫秒，越小越好）

| | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog 压缩，快速模式（C++） | 38 | 72 | 103 | 122 | 137 | 167 | 197 | 235 | 255 | 303 |
| BqLog 压缩+加密，快速模式（C++） | 38 | 67 | 109 | 129 | 137 | 168 | 203 | 222 | 259 | 319 |
| BqLog 文本，快速模式（C++） | 87 | 160 | 211 | 312 | 369 | 452 | 524 | 633 | 696 | 916 |
| BqLog 压缩，普通模式（C++） | 25 | 64 | 77 | 107 | 177 | 172 | 188 | 215 | 255 | 321 |
| BqLog 压缩+加密，普通模式（C++） | 32 | 63 | 78 | 105 | 138 | 167 | 231 | 224 | 252 | 324 |
| BqLog 文本，普通模式（C++） | 98 | 173 | 201 | 284 | 351 | 428 | 497 | 573 | 647 | 856 |
| fmtlog | 156 | 305 | 495 | 666 | 830 | 1031 | 1166 | 1381 | 1481 | 1932 |
| quill | 223 | 462 | 721 | 946 | 1178 | 1442 | 1744 | 2017 | 2215 | 2532 |
| spdlog（异步） | 500 | 1480 | 3245 | 6064 | 12925 | 24201 | 33739 | 40360 | 46564 | 55105 |
| glog | 1761 | 3140 | 4956 | 7283 | 10530 | 19427 | 26027 | 31964 | 37053 | 40587 |

#### 吞吐测试的峰值内存（MB）

整个进程的峰值常驻内存，取自内核记录的最高水位。BqLog 进程里同时挂着全部 6 种配置（3 种 Appender × 2 种模式），其他进程各只有一个 logger。

| | 1 线程 | 4 线程 | 10 线程 |
|---|---:|---:|---:|
| BqLog（一个进程 6 种配置） | 5.4 | 6.6 | 8.5 |
| quill | 143.2 | 560.8 | 1405.7 |
| fmtlog | 2.6 | 5.7 | 12.2 |
| spdlog（异步） | 5.0 | 8.5 | 8.6 |
| glog | 1.6 | 2.0 | 2.5 |

#### 输出文件大小（1 线程，400 万条）

| | 格式 | 大小 | 每条字节数 |
|---|---|---:|---:|
| BqLog 压缩（两种模式相同） | 二进制 | 47 MB | 12 |
| BqLog 压缩+加密（两种模式相同） | 二进制，加密 | 47 MB | 12 |
| BqLog 文本（两种模式相同） | 文本 | 317 MB | 79 |
| quill | 文本 | 263 MB | 66 |
| fmtlog | 文本 | 299 MB | 75 |
| spdlog（异步） | 文本 | 303 MB | 76 |
| glog | 文本 | 349 MB | 87 |
| Log4j2 | 文本 | 213 MB（200 万条，只有 4 参数用例） | 106 |

#### 怎么看 macOS 的吞吐数据

- **4 个参数时，BqLog 压缩比 fmtlog 快 5～7 倍，比 quill 快 7～10 倍，比 Log4j2 快 9～14 倍**；spdlog（异步）和 glog 差了一到两个数量级。
- **BqLog 文本**要把每条日志都格式化成完整的一行文本，依然**比 fmtlog 快 1.6～1.8 倍**，比 quill 快 2.1～2.5 倍，这两个是这里最快的文本日志库。
- **快速模式没有在消费端把省下的成本还回去。** 两种模式的总吞吐基本相同：多数线程数下相差几个百分点以内，压缩略快一点，文本略慢一点。消费端直接读取快速模式的记录，不做额外转换，所以快速模式在日志线程上省下的工作，不会在后台核心上再付一遍。
- **加密几乎不要钱**：2～10 线程时，压缩+加密和纯压缩只差几个百分点。
- **内存**：quill 每个线程的队列会在持续负载下不断增长（10 线程时到 1.4 GB）；BqLog 一个进程挂 6 个 logger，仍然只有个位数 MB。

### 2.2 Windows（AMD Ryzen 9 9950X）

> 以下数据是 BqLog 2.5.0（只有普通模式）配 quill 11.1.0、fmtlog @ 308b231、Log4j2 2.23.1 测得，之后会用 2.6.0 和上面列出的库版本重测。

#### 总耗时，4 个参数（毫秒，越小越好）

| | 1 线程 | 2 线程 | 3 线程 | 4 线程 | 5 线程 | 6 线程 | 7 线程 | 8 线程 | 9 线程 | 10 线程 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BqLog 压缩（C++） | 95 | 144 | 210 | 210 | 226 | 267 | 362 | 395 | 439 | 507 |
| BqLog 压缩+加密（C++） | 102 | 166 | 167 | 190 | 236 | 308 | 350 | 391 | 453 | 493 |
| BqLog 文本（C++） | 258 | 513 | 777 | 1054 | 1324 | 1587 | 1891 | 2143 | 2465 | 2811 |
| fmtlog | 672 | 1219 | 1766 | 2428 | 3024 | 3923 | 4612 | 5935 | 6293 | 7934 |
| quill | 639 | 1429 | 2232 | 3082 | 3915 | 4726 | 5609 | 6246 | 6957 | 7812 |
| Log4j2（Java） | 873 | 1484 | 2087 | 2727 | 3738 | 4541 | 4889 | 6127 | 9475 | 7192 |
| spdlog（异步） | 560 | 1649 | 3402 | 5737 | 9069 | 13827 | 21494 | 24518 | 28463 | 32939 |
| glog | 4485 | 8548 | 14875 | 21387 | 28295 | 36060 | 45742 | 62368 | 102370 | 127550 |

#### 峰值内存（MB），Windows 进程峰值工作集

| | 1 线程 | 4 线程 | 10 线程 |
|---|---:|---:|---:|
| BqLog（一个进程挂 3 个 Appender） | 12.7 | 13.3 | 14.7 |
| spdlog（异步） | 14.4 | 14.4 | 14.7 |
| glog | 11.9 | 12.1 | 12.4 |
| fmtlog | 17.1 | 20.2 | 23.3 |
| quill | 282.1 | 1062.7 | 2714.9 |
| Log4j2（Java） | 1537.8 | 6884.3 | 4631.5 |

## 3. 第二部分：日志线程自身的开销

### 3.1 测法

每个日志线程**一次连续写 100 条日志**（一个 burst），用 `steady_clock` 给每个 burst 计时——每个 burst 只读两次时钟，时钟本身的开销和精度被摊到 100 次调用上。分两种场景：

- **busy**：burst 一个接一个。线程不停地写日志，让消费端满负荷运转；生产者撞上缓冲区写满，就发生在这种场景。
- **occasional**：每个 burst 之后休眠 1 ms。大多数应用就是这样写日志的：一阵一阵地写，中间去干别的活，回来时缓存已经凉了。

每种场景按每次日志调用给出：

- **mean（平均）**：花在日志调用上的总时间 ÷ 调用次数。所有卡顿都算在里面，所以这就是日志占了线程多少时间。如果只看一个数，就看这个。
- **p50**：一个典型的 burst。
- **p99**：最慢的 1% 的 burst：缓冲区写满时的阻塞、缺页、消费端的干扰。

我们刻意按 burst 统计分位数，而不是给每次调用单独画直方图：用普通的时钟，几纳秒的一次调用根本没法测准，100 次的 burst 才能测准。

同时给出**为这个速度付出的代价**：**消费端 CPU**（除日志线程外所有线程的 CPU 时间，按占一个核心的百分比计）和进程的**峰值内存**。日志线程完全可以做到几乎零开销：给消费端一整个核心，或者让队列无限增长。这两列就是用来看有没有发生这种事的。

所有消费端都用默认的、空闲时休眠的配置（quill 后台线程休眠 100 µs，BqLog 的工作线程睡到被唤醒为止），所有 logger 都写同一行文本到文件，每种场景交替跑 7 轮，表里是中位数。

### 3.2 macOS（Apple M4 Pro）

#### busy，1 线程

| | mean (ns) | p50 (ns) | p99 (ns) | 消费端 CPU | 峰值内存（MB） |
|---|---:|---:|---:|---:|---:|
| BqLog，快速模式 | 75.4 | 5.0 | 904.2 | 100% | 2.6 |
| BqLog，普通模式 | 79.1 | 13.3 | 376.2 | 100% | 2.6 |
| quill | 5.9 | 2.1 | 17.1 | 97% | 143.4 |
| fmtlog | 118.3 | 115.0 | 186.7 | 99% | 2.8 |
| spdlog（异步） | 288.1 | 277.5 | 492.9 | 94% | 5.3 |

#### busy，4 线程

| | mean (ns) | p50 (ns) | p99 (ns) | 消费端 CPU | 峰值内存（MB） |
|---|---:|---:|---:|---:|---:|
| BqLog，快速模式 | 287.1 | 7.5 | 4930.0 | 100% | 4.3 |
| BqLog，普通模式 | 280.3 | 12.1 | 1967.9 | 100% | 4.2 |
| quill | 8.9 | 2.5 | 17.1 | 97% | 566.8 |
| fmtlog | 529.1 | 496.7 | 1625.0 | 100% | 7.3 |
| spdlog（异步） | 3492.3 | 3363.3 | 6442.9 | 83% | 6.8 |

#### occasional，1 线程

| | mean (ns) | p50 (ns) | p99 (ns) | 消费端 CPU | 峰值内存（MB） |
|---|---:|---:|---:|---:|---:|
| BqLog，快速模式 | 47.5 | 24.6 | 287.9 | 3% | 2.3 |
| BqLog，普通模式 | 92.4 | 55.8 | 542.1 | 3% | 2.3 |
| quill | 4.2 | 3.8 | 11.2 | 5% | 6.4 |
| fmtlog | 47.1 | 35.4 | 199.2 | 6% | 2.5 |
| spdlog（异步） | 651.6 | 492.1 | 3471.7 | 7% | 5.0 |

#### occasional，4 线程

| | mean (ns) | p50 (ns) | p99 (ns) | 消费端 CPU | 峰值内存（MB） |
|---|---:|---:|---:|---:|---:|
| BqLog，快速模式 | 34.9 | 18.3 | 210.0 | 8% | 2.8 |
| BqLog，普通模式 | 68.5 | 41.7 | 387.9 | 8% | 2.8 |
| quill | 4.0 | 3.8 | 12.1 | 10% | 14.3 |
| fmtlog | 43.2 | 35.8 | 165.0 | 12% | 5.9 |
| spdlog（异步） | 963.6 | 792.9 | 3523.3 | 20% | 5.3 |

#### 怎么看日志线程的数据

- **在这台机器上，quill 的日志线程开销最低**，各场景都是每条 4～9 纳秒。在 busy 场景里，这是用内存换来的：一个线程时它的队列涨到 143 MB，四个线程时涨到 567 MB，而其他 logger 都在 8 MB 以内。在 occasional 场景里，它的后台线程即使应用空闲也每 100 µs 轮询一次，所以做同样的工作，它消费端的 CPU 比 BqLog 高。
- **消费端满负荷时，BqLog 快速模式的典型 burst 仍然是每条 5～8 纳秒**（busy 的 p50），约为普通模式的一半。busy 的平均值则高得多，两种模式都是：busy 场景产生文本的速度比这里任何一个消费端格式化的速度都快，BqLog 默认的缓冲区（每线程 64 KB）写满后，日志线程会等消费端，而不是让内存继续涨。这就是默认的 `block` 策略在起作用；改用 `log.buffer_policy_when_full=expand` 或者调大 `log.buffer_size`，就是拿内存换这段等待，quill 默认的无上限队列就是这么做的。第一部分的数据说明了 BqLog 的消费端处理这种积压有多快。
- **occasional 场景下，快速模式的开销约为普通模式的一半**（典型每条 18～25 纳秒对 42～56 纳秒），但 quill 仍然遥遥领先，只要 4 纳秒。我们测了这个差距的来源：休眠 1 ms 之后的第一次调用，BqLog 快速模式要约 1,800 个周期，quill 约 500 个；同一个 burst 里后面的调用，CPU 频率只有大约一半。quill 的后台线程即使无事可做也每 100 µs 醒一次，让核心一直保持在高频、缓存不凉；BqLog 的工作线程则睡到有活为止。我们在 BqLog 旁边放一个每 100 µs 醒一次、什么都不做的线程，同样的快速模式 burst 就从每条约 51 个周期降到了 16 个。也就是说，quill 在这一项上的领先，有一部分是靠一个永不真正睡觉的消费端换来的——这个交换 BqLog 刻意不做。在不做这个交换的前提下缩小这个差距，已经在我们的计划里。
- **occasional 场景下，BqLog 的消费端最轻**（占一个核心的 3～8%，其他库是 5～20%），而且在总消耗（第一部分）上，同样的工作它比 quill 快 7～10 倍完成。

## 4. 功能对比

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

## 5. 关于公平性

- **大家都一条不丢。** 没有哪个库丢日志：fmtlog 开了 `FMTLOG_BLOCK=1`，spdlog 用阻塞溢出策略，BqLog 用默认的 `block` 策略，quill 用默认的阻塞队列。每一轮吞吐测试都核对过，文件里正好有 2,000,000 × 线程数 条。
- **quill** 在吞吐部分用它官方 benchmark 的忙等后台线程（它最快的配置），在日志线程部分用默认的休眠后台线程，因为那一部分所有消费端必须处于同样的、贴近实际的模式。
- **spdlog** 用异步模式，这是它推荐的高吞吐配置；计时在 `spdlog::shutdown()` 排空队列之后才结束。
- **glog** 设计上就是同步的，使用它的流式 API；它不支持 `{fmt}` 风格的格式化。
- **fmtlog**：`setLogFile()` 和它的轮询线程有竞争，所以它的两个吞吐用例分别在两个进程里跑；最后的刷新先停掉轮询线程，再用 `poll(true)` 排空。
- **Log4j2** 只参加吞吐部分，数据包含 JVM。

## 6. 附录：Benchmark 源代码

源代码在 [`benchmark` 分支](https://github.com/Tencent/BqLog/tree/benchmark/benchmark/cross)：吞吐部分是 `bench_<lib>.cpp`（每个库一个可执行文件）；日志线程部分是 `bench_latency.cpp`（一份源码，每个库编一次）；还有 `run_benchmark.sh`/`.ps1`、`run_latency.sh`、`measure_memory.sh`/`.ps1`、`log4j/` 下的 Log4j2 工程，以及把 CSV 结果转成上面这些表格的 `make_tables.py`。

### 6.1 BqLog，吞吐

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

Appender 配置（文本用 `type=text_file`；压缩+加密多一行 `pub_key=...`）：

```
log.high_perform_mode_freq_threshold_per_second=1
appenders_config.appender_0.type=compressed_file
appenders_config.appender_0.levels=[all]
appenders_config.appender_0.file_name=output/bqlog_compress
appenders_config.appender_0.always_create_new_file=true
```

### 6.2 日志线程部分（所有库）

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

各个库之间只有 `log_one` 这一行不同，比如 BqLog 是 `g_log->info(...)` / `BQ_LOG_FAST_INFO(*g_log, ...)`，quill 是 `LOG_INFO(g_logger, ...)`，参数都是 `"idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true`。消费端 CPU = 进程 CPU 时间 − 日志线程自己的 CPU 时间，再除以墙钟时间。

### 6.3 其他库，吞吐

spdlog、glog、fmtlog、quill、Log4j2 的吞吐程序和 6.1 结构相同——同样的线程安排、同样的两条日志、计时到全部落盘——各库的异步配置见第 1 节和第 5 节。源码见 benchmark 分支上的 `bench_spdlog.cpp`、`bench_glog.cpp`、`bench_fmtlog.cpp`、`bench_quill.cpp` 和 `log4j/src/bq/benchmark/log4j/main.java`。
