# BqLog benchmarks

Every BqLog release has its own benchmark: the code, the runner scripts and the raw data behind that release's [docs/BENCHMARK.md](https://github.com/Tencent/BqLog/blob/main/docs/BENCHMARK.md). Each one lives on its own `benchmarks/<version>` branch, so the numbers of a release can always be reproduced with the code that produced them.

| BqLog version | Benchmark code and data | Results in the docs | Hardware |
|---|---|---|---|
| 2.6.0 | [`benchmarks/2.6.0`](https://github.com/Tencent/BqLog/tree/benchmarks/2.6.0) | [BENCHMARK.md](https://github.com/Tencent/BqLog/blob/Release_2.6.0/docs/BENCHMARK.md) · [简体中文](https://github.com/Tencent/BqLog/blob/Release_2.6.0/docs/BENCHMARK_CHS.md) | Apple M4 Pro, macOS; AMD Ryzen 9 9950X, Windows 11; AMD EPYC 7K62 VM, Linux |
| 2.5.0 | [`benchmarks/2.5.0`](https://github.com/Tencent/BqLog/tree/benchmarks/2.5.0) | [BENCHMARK.md](https://github.com/Tencent/BqLog/blob/3c2d1df2/docs/BENCHMARK.md) · [简体中文](https://github.com/Tencent/BqLog/blob/3c2d1df2/docs/BENCHMARK_CHS.md) (published on `main` after the 2.5.0 release) | AMD Ryzen 9 9950X, Windows 11 |

The main project lives on `main` / `develop`. This branch only carries this index.
