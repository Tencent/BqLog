# Frequently Asked Questions (FAQ)

[简体中文](FAQ_CHS.md)

## What is BqLog?

An open-source, industrial-grade logging library from Tencent, with a C++ core. It exists to break the "impossible triangle" of shipped builds: you want full logging, but can't pay for it with performance, package size or storage. This is no toy project: it runs in production in Honor of Kings (international) — a client product with over 30 million DAU — and other recent Tencent titles such as 王者万象棋 and 洛克王国 have adopted it since.

## What does BqLog aim for?

Extreme speed, but not speed alone. BqLog is meant to be a mature, industrial-grade component, and that means four things at once:

- **Speed on both ends.** The thread that writes a log entry and the thread that turns it into a file run on the same machine. A library that makes the first one fast by pushing work onto the second has only moved the cost. BqLog keeps both cheap and is measured on realistic workloads.
- **Compatibility.** Many platforms, many languages sharing one core, many environments — game engines, mobile toolchains without an STL, strict compiler settings.
- **Robustness when things go wrong.** Crashes, full disks, corrupted files, a consumer that falls behind: each has a defined, tested behavior.
- **Developer experience.** Simple calls that your IDE completes and type-checks, and compatibility across versions: older headers keep working with newer libraries, and log files stay readable.

## How is BqLog different from spdlog, quill or fmtlog?

On raw performance: spdlog's strength is its ecosystem, quill's is call-site latency, fmtlog's is lean text logging. BqLog leads on total throughput and memory footprint in the same benchmark scenario, and its C++ fast mode roughly halves the cost on the logging thread ([numbers here](BENCHMARK.md)). On the calling thread alone, quill is still the cheapest; the benchmark shows that next to what it costs in consumer CPU and memory, because a logger that is fast only on the calling thread usually pays for it on another core or in memory.

But the real difference is industrial completeness. What a logging library faces once it ships inside a released product — losing logs in crashes, content encryption, mixed-language codebases, mobile toolchain constraints, compatibility across repeated builds and multiple versions, package-size budgets — is all covered and accounted for in BqLog. Each of the other three has its own gaps across these dimensions.

## What is the C++ fast mode, and when should I use it?

`BQ_LOG_FAST_VERBOSE` … `BQ_LOG_FAST_FATAL` are C++ only macros that write the same log entry as `log.info` and friends (the normal mode) at a fraction of the cost on the calling thread. Use them on hot paths. Categories work the same way as in the normal mode: `BQ_LOG_FAST_INFO(log, log.cat.xxx, ...)`. The trade-offs: slightly more memory, weaker IDE hints because they are macros, and each call site must keep using the log object and format string of its first call. Everywhere else, the normal mode is the simpler default. See [API Reference — fast mode](API_REFERENCE.md#fast-mode).

## Who runs it in production?

Honor of Kings (international) — a client product validated at 30M+ DAU. Recent Tencent self-developed titles such as 王者万象棋 and 洛克王国 have adopted it too.

## Which platforms and languages are covered?

- Platforms: Windows 64-bit, macOS, Linux (including embedded), iOS, Android, HarmonyOS, OpenHarmony, Unix (FreeBSD, NetBSD, OpenBSD, Solaris, …)
- Languages: C++ (C++11+), Java/Kotlin, C# (Unity, .NET), ArkTS/C++ (HarmonyOS and OpenHarmony share one package), JavaScript/TypeScript (Node.js), Python 3.7+, Go, Unreal Engine (UE4, UE5 and UE6 development builds)
- Hardware: x86, x86_64, ARM32, ARM64
- Integration: dynamic library, static library, or source

## Any platforms without prebuilt binaries?

Yes: PS5 and Switch have no prebuilt binaries for now (console toolchains come with licensing constraints). On those platforms, integrate from source and compile yourself — the core depends only on the C standard library and platform APIs, so porting is cheap.

## What about game engines?

Dedicated support, not just "it compiles": Unity, Tuanjie Engine and Unreal all have ready-made plugins/packages; the UE plugin natively understands Blueprints and UE's builtin data types (FString, FName and friends); common data types are adapted on the Unity side. Logging inside an engine feels the same as logging in your own C++ project. See [Engine Integration](ENGINE_INTEGRATION.md).

## Where is it published?

Every language has its proper channel:

- **GitHub Releases**: prebuilt packages for all platforms (xcframework, Android AAR, npm tgz, Python wheels, per-language wrapper packages, …)
- **Maven Central**: Android / Java / Kotlin
- **npm**: `@pippocao/bqlog` (Node.js / TypeScript)
- **PyPI**: `pip install bqlog`
- **ohpm**: HarmonyOS / OpenHarmony (`ohpm install bqlog`)
- **Fab**: the Unreal Engine plugin
- **Unity / Tuanjie Engine**: package import
- **Go**: source-module distribution

## Where are the performance numbers?

[docs/BENCHMARK.md](BENCHMARK.md), in two parts: total cost (throughput of the whole pipeline, 1–10 threads, 2 million entries per thread) and the cost on the logging thread itself, plus peak memory and output file size. The test code for this version lives at [`benchmark_2.6.0`](https://github.com/Tencent/BqLog/tree/benchmark_2.6.0/benchmark/cross) and you can rerun it yourself.

## What happens to my logs if the process crashes?

This is one of BqLog's headline features. In async mode the buffers live in memory-mapped files, so the data outlives the process. On restart, each mmap file is checksum-verified, half-finished pieces from the crash instant are voided, and everything else is replayed strictly in per-thread sequence order into a recovery segment of the new log file. Full walkthrough with diagrams: [part 2, section 7.3](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>).

## Can multiple languages share it in one process?

Yes — genuinely share it. C++, Java, C#, Python and TypeScript wrappers in the same process can all write to **the same** Log object and the same file. The Java, C# and TypeScript wrappers do their hot path with (almost) zero extra heap allocation. For mixed-language clients — say a game engine plus a scripting layer — nobody else offers this.

## Does it work server-side?

Yes. Client and server share the same core. What server-side cares about — throughput under high concurrency, a wide range of operating systems (Linux, the FreeBSD family, the Solaris family have all passed tests), long-running stability — is all covered. High-concurrency scenarios where logging stalls or loses entries are exactly what the queue design is built to cure.

## Does it support encryption?

Yes. The compressed file appender offers hybrid RSA+AES encryption, applied per file segment. In the benchmark, the encrypted row sits almost exactly on top of the plain-compression row.

## What's the build and deployment story?

Deliberately boring, by design: it depends only on the C standard library and platform APIs, builds from C++11 up, passes with `-Wall -Wextra -pedantic -Werror` all on, and compiles under Android `ANDROID_STL=none`. The build system is CMake with per-platform scripts. Dynamic library, static library or source integration — your choice.

## How do I read the compressed logs?

The compressed format is binary; decode it back to text with the decoder in `tools/log_decoder`. The format itself is documented in [part 1](<Article 1_Why is BqLog so fast - High Performance Realtime Compressed Log Format.MD>).

## Do I have to use the compressed format? Can I get plain text?

Yes, plain text is available through the TextFileAppender — and even that text mode is faster than every text logger in the benchmark. Compressed and text appenders can be mixed on one Log object.

## How big is the library, and how much memory does it use?

About 200 KB as an Android dynamic library. In the 10-thread benchmark case (20 million entries in total), the whole benchmark process peaks at no more than 3.1 MB on macOS; on mobile, BqLog itself generally uses around 1 MB.

## Will logs be dropped if the consumer can't keep up?

Your choice, by configuration: discard new entries, block producers, or auto-expand. Details in [part 2, §8.2](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>) and [ADVANCED_USAGE](ADVANCED_USAGE.md).

## What license?

Apache-2.0, free for commercial use. The concurrency scheme was patented before open-sourcing; the design is explained in [part 2](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>).

## How do I integrate it?

Standard environments (C++, Java, C#, Python, Node.js, …): the [Integration Guide](INTEGRATION_GUIDE.md). Game engines: [Engine Integration](ENGINE_INTEGRATION.md); Unity, Tuanjie and Unreal have ready-made plugins.
