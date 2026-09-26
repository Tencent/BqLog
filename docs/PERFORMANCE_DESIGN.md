# BqLog performance design: three articles

[简体中文](PERFORMANCE_DESIGN_CHS.md) · [Project home](../README.md)

This series was rewritten against BqLog 2.5.0, source baseline `424948bd`, checked on 2026-09-25. Each article starts with an intuitive approach, explains its costs, and derives the current structures step by step. Parts 1 and 2 retain their historical filenames to preserve links.

| Article | Focus |
|---|---|
| [1: Deriving a compressed format from a line of text](<Article 1_Why is BqLog so fast - High Performance Realtime Compressed Log Format.MD>) | Repetition, templates, item bits, memory versus file bytes, segments, encryption and recovery |
| [2: From a ring buffer to an adaptive bus](<Article 2_Why is BqLog so fast - High Concurrency Ring Buffer.MD>) | SPSC, CAS/fetch-add rollback, coherence costs, LP/HP, Groups and ordering |
| [3: How much work can share one memory load?](<Article 3_Why is BqLog so fast - Optimizing the Compressed Log Pipeline.MD>) | Redundant work, fused copy/hash, CRC chains, cache probes, backfill, batching and encryption cost |

Implementation mechanisms, existing measurements and proposed experiments are distinguished. Public numbers are not guarantees for every platform, language, Unity/UE frame rate or failure mode.

Sixteen diagrams were redrawn from the author's original figures and Excalidraw sketches against current source. Adjacent fields represent adjacent bytes. Bits, offsets, cursors, pointers and indices are explicit; ABI-dependent positions use symbols. Both editions explain the shared English technical labels in the surrounding text.

- [File container and records](img/performance/compressed-file-v10.png)
- [Item type bit and length](img/performance/compressed-item-bits.png)
- [Templates and arguments](img/performance/compressed-templates.png)
- [One record in memory and on disk](img/performance/compressed-record-example.png)
- [UTF-Mixed bytes](img/performance/compressed-utf-mixed.png)
- [Segment encryption layout and costs](img/performance/encryption-segments.png)
- [Logical and physical ring cursors](img/performance/ring-basics.svg)
- [MISO memory and publication](img/performance/miso-memory.svg)
- [Fetch-add rollback counterexample](img/performance/miso-rollback.svg)
- [SISO memory and batching](img/performance/siso-memory.svg)
- [Data bus](img/performance/adaptive-log-bus.svg)
- [Block lifetime](img/performance/block-lifecycle.svg)
- [Context and cross-path ordering](img/performance/bus-order.svg)
- [Copying and hashing](img/performance/fused-copy-hash.png)
- [Template caches](img/performance/template-cache.png)
- [Write cache and backfill](img/performance/write-cache-backfill.png)

Edit the SVGs directly or regenerate them with `node docs/img/performance/draw_diagrams.mjs` using the included drawing source.

The original drawing is design context, not the specification. Code determines layout, synchronization and encryption behavior. In particular, the current `rsa_aes_xor` implementation differs from directly AES-encrypting every payload; Parts 1 and 3 make that distinction explicit.
