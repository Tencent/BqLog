# BqLog performance design: three articles

[简体中文](PERFORMANCE_DESIGN_CHS.md) · [Project home](../README.md)

BqLog's speed is not magic. These three articles start from a single line of log and derive the whole design step by step: take the most intuitive approach, count what it costs, and let that cost lead to the next one. Everything is written against the BqLog 2.5.0 source.

| Article | What it covers |
|---|---|
| [1: Deriving a compressed format from a line of text](<Article 1_Why is BqLog so fast - High Performance Realtime Compressed Log Format.MD>) | Squeezing the text: templates, VLQ, UTF-Mixed, segments and encryption |
| [2: From a ring buffer to an adaptive bus](<Article 2_Why is BqLog so fast - High Concurrency Ring Buffer.MD>) | Dozens of threads without a fight: fetch_add + rollback, MISO/SISO, Groups and crash recovery |
| [3: How much work can share one memory load?](<Article 3_Why is BqLog so fast - Optimizing the Compressed Log Pipeline.MD>) | Saving work on the execution path: fused copy+hash, template caches, length backfill, batched I/O |

Read them in order. Part 1 decides what the data looks like, part 2 decides how it travels, and part 3 squeezes out the waste left over along that road.

For benchmark numbers, go to [Benchmark](BENCHMARK.md); for how to configure these mechanisms in daily use, see [Advanced Usage](ADVANCED_USAGE.md).
