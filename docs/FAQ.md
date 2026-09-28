# Frequently Asked Questions (FAQ)

[简体中文](FAQ_CHS.md)

## What is BqLog?

An open-source, industrial-grade logging library from Tencent, with a C++ core. It exists to break the "impossible triangle" of shipped builds: you want full logging, but can't pay for it with performance, package size or storage. This is no toy project: it runs in production in Honor of Kings (international) — a client product with over 30 million DAU — and other recent Tencent titles such as 王者万象棋 and 洛克王国 have adopted it since.

## How is BqLog different from spdlog, quill or fmtlog?

On raw performance: spdlog's strength is its ecosystem, quill's is call-site latency, fmtlog's is lean text logging — and BqLog leads on total throughput and memory footprint in the same benchmark scenario ([numbers here](BENCHMARK.md)).

But the real difference is industrial completeness. What a logging library faces once it ships inside a released product — losing logs in crashes, content encryption, mixed-language codebases, mobile toolchain constraints, compatibility across repeated builds and multiple versions, package-size budgets — is all covered and accounted for in BqLog. Each of the other three has its own gaps across these dimensions.

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

[docs/BENCHMARK.md](BENCHMARK.md): 1–10 threads, 2 million entries per thread, 4 format arguments each — throughput, peak memory and output file size. The test code is in benchmark/ and you can rerun it yourself.

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

About 200 KB as an Android dynamic library. In the 10-thread benchmark case (20 million entries, three appenders active), BqLog's own peak memory is 2–3 MB; on mobile it's generally around 1 MB. One comparison object in the same run peaks in the GB range.

## Will logs be dropped if the consumer can't keep up?

Your choice, by configuration: discard new entries, block producers, or auto-expand. Details in [part 2, §8.2](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>) and [ADVANCED_USAGE](ADVANCED_USAGE.md).

## What license?

Apache-2.0, free for commercial use. The concurrency scheme was patented before open-sourcing; the design is explained in [part 2](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>).

## How do I integrate it?

Standard environments (C++, Java, C#, Python, Node.js, …): the [Integration Guide](INTEGRATION_GUIDE.md). Game engines: [Engine Integration](ENGINE_INTEGRATION.md); Unity, Tuanjie and Unreal have ready-made plugins.
