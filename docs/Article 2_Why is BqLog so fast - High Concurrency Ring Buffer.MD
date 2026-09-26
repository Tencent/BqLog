# Why is BqLog so fast? Part 2: From a ring buffer to an adaptive data bus

[简体中文](<文章2_为何BqLog如此快 - 高并发环形队列.MD>) · [Series](PERFORMANCE_DESIGN.md) · [Previous: compressed files](<Article 1_Why is BqLog so fast - High Performance Realtime Compressed Log Format.MD>) · [Next: the execution path](<Article 3_Why is BqLog so fast - Optimizing the Compressed Log Pipeline.MD>)

In part 1 we turned logs from text into templates plus arguments, cutting formatting and storage costs. But how that data travels from business threads to the logging thread decides performance just as much. Picture this: compressing one entry takes a few nanoseconds, yet dozens of threads are all queued up in front of the same lock — everything we saved earlier gets lost right there.

This article is about how that road gets built. We'll start from the simplest ring buffer, add real requirements one at a time — multiple threads, variable-length data, threads coming and going — and end up at BqLog's current adaptive data bus (`log_buffer`). It's not about swapping out one atomic instruction and calling it a day; it's about building different roads for threads with different temperaments.

> Written against BqLog 2.5.0. MISO and SISO are the project's naming habits: MISO means multiple producers and a single consumer (Multi In, Single Out — commonly called MPSC); SISO means single producer, single consumer (Single In, Single Out — SPSC). The alignment constant `BQ_CACHE_LINE_SIZE` in the code is 128 bytes, abbreviated as C below. It's the library's own layout choice and does not imply every CPU's hardware cache line is 128 bytes.

## 1. Before we start: the report card

For readers who haven't seen the older articles, some background first. There are quite a few logging libraries famous for performance:

#### Java
- [Log4j2](https://github.com/apache/logging-log4j2)
- [Logback](https://github.com/qos-ch/logback)
#### C#
- [NLog](https://github.com/NLog/NLog)
- log4net
#### C++
- [SpdLog](https://github.com/gabime/spdlog)
#### Multi-language
- [mars-xlog](https://github.com/Tencent/mars)

Heroes don't ask about pedigree — whatever language a library is written in, we compared them under identical scenarios and test cases, each in its fastest mode (e.g. async). Overall, Log4j2 is the toughest rival. So how does BqLog stack up against it? (Test: 1–10 threads, 2 million entries per thread, 4 format arguments each; environment: MacBook Pro, Apple M4 Pro, 48 GB)

#### Total time per log entry with 4 arguments (milliseconds)

|                              | 1 thread | 2 threads | 3 threads | 4 threads | 5 threads | 6 threads | 7 threads | 8 threads | 9 threads | 10 threads |
|------------------------------|-------|-------|-------|-------|-------|-------|-------|-------|-------|--------|
| BqLog Compress(C++)          | 79    | 99    | 115   | 170   | 222   | 311   | 324   | 372   | 484   | 759    |
| BqLog Compress+Encrypt(C++)  | 83    | 106   | 134   | 189   | 223   | 328   | 337   | 390   | 532   | 995    |
| BqLog Text(C++)              | 202   | 417   | 648   | 910   | 1167  | 1425  | 1718  | 1969  | 2281  | 2718   |
| spdlog                       | 434   | 1366  | 3133  | 4779  | 6228  | 9241  | 10829 | 11348 | 11197 | 12003  |
| Log4j2 (Java)                | 946   | 1841  | 2422  | 3685  | 5542  | 5245  | 5775  | 5786  | 8048  | 8752   |

At 10 threads, BqLog compressed mode finishes in 759 ms while Log4j2 takes 8752 ms — a gap of **more than 11x**. And look at the encrypted row: with encryption on the whole time, it's still less than a third slower than plain compression; how that trick works was covered in part 1. Test cases and configurations for both sides: [Benchmark](https://github.com/Tencent/BqLog/blob/main/README.md#-benchmark-results).

A bunch of measures work together behind this:

- 1. Cache-line isolation to avoid `False Sharing`
- 2. Efficient use of the memory model (`Memory Order`), with professional CPU profilers like AMD uProf to find the memory-access bottlenecks
- 3. Merging IO operations
- 4. Avoiding runtime format
- 5. Improving CPU cache hit rates
- 6. And more…

Covering each of them properly would take several more articles. This one focuses on the juiciest piece: BqLog's self-built high-concurrency data channel. We'll first see how it moves `CAS (Compare And Swap)` off the hot path, then how it goes one step further so that high-frequency threads don't even have to compete for an atomic at all. This scheme was patented before BqLog went open source.

## 2. Prerequisites: the ring buffer

- ***This section introduces Linux's `kFifo` and the `LMAX Disruptor` for beginners; if you already know them, jump straight to [section 3](#3-bqlogs-first-axe-the-miso-queue)***
- ***This article focuses on concurrency control itself; memory barriers and memory orders are deliberately glossed over — there are plenty of good articles about them online***

### 2.1 Linux kFifo: one man, one gun, no fighting

`kFifo` is a circular queue (FIFO) implementation provided by the Linux kernel, widely used in kernel modules, drivers and inter-device communication. Note: **the classic kFifo only works for one reader thread and one writer thread** — no more concurrency than that.

![kFifo's ring structure](img/performance/ring-kfifo.png)

<p align="center"><em>Figure 1: kFifo's ring — <code>In</code> and <code>Out</code> each have exactly one owner.</em></p>

It's built on a ring buffer, with two pointers, `in` and `out`, managing writes and reads: writing pushes `in` forward, reading pushes `out` forward, and a pointer reaching the end of the buffer wraps back to the beginning — hence the "ring". In the figure above, the red slots 1 to 8 hold data that's been written but not yet read: `Out` points at the oldest entry, `In` at the next free slot.

The pseudo-code:

```c
/// Called by the write thread
unsigned int kfifo_in(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int l;

    l = min(len, fifo->size - fifo->in + fifo->out);  // check how much space is left
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buf, l);  // write
    memory_barrier();  // ensure that when the reader sees the new fifo->in, the data before it is already written
    fifo->in += l; // update fifo->in so the reader knows where the newest data ends
    return l;
}

/// Called by the read thread
unsigned int kfifo_out(struct kfifo *fifo, void *buf, unsigned int len)
{
    unsigned int l;

    l = min(len, fifo->in - fifo->out); // check how much data is in the queue
    memcpy(buf, fifo->buffer + (fifo->out & (fifo->size - 1)), l);  // read
    memory_barrier();  // ensure that when the writer sees the new fifo->out, the data before it has been fully read
    fifo->out += l;  // update fifo->out so the writer knows this area can be overwritten
    return l;
}
```

The essence of this design: **`in` is touched only by the writer, `out` only by the reader.** Each variable has exactly one owner, which eliminates the race of two threads modifying the same variable at the root. Synchronization relies solely on memory barriers for ordering — no locks, no CAS. That's why it's fast.

But the beautiful "chase me" picture falls apart with multiple producers. Look at the `memcpy` in `kfifo_in`: if two threads write at the same time, they read the same `in` value and will copy data to the same address. Collision.

### 2.2 LMAX Disruptor: grabbing numbers with CAS

The `LMAX Disruptor` is one of the most highly regarded high-performance concurrency frameworks of recent years, from the LMAX exchange in the UK. It's also Log4j2's default message queue, with code and docs open sourced on Github: [LMAX-Exchange/disruptor](https://github.com/LMAX-Exchange/disruptor).

Its body is a ring too, but what each slot holds is not the data itself — it's a reference to a `Log Entry` object, which in turn drags along a string of objects it references:

![The LMAX Disruptor's ring structure](img/performance/ring-disruptor.png)

<p align="center"><em>Figure 2: the Disruptor's ring — slots hold <code>Log Entry</code> references, not the data itself.</em></p>

Its answer to multi-producer writing is `CAS (Compare-And-Swap)`. CAS does three things: check whether the current value of a memory address equals the expected value; if yes, update it to the new value; if not, some other thread beat you to it — this attempt fails, try again.

Let's try retrofitting `kfifo_in` with CAS for concurrent writes:

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    unsigned int old_in, new_in, free_space;

    do {
        // 1. get the current in pointer
        old_in = fifo->in;

        // 2. compute remaining space in the ring_buffer
        free_space = fifo->size - (fifo->in - fifo->out);
        if (len > free_space) {
            return 0;  // not enough space, write fails
        }

        // 3. compute the new in position
        new_in = old_in + len;

        // 4. try to update in via CAS
        // if in is still old_in, the update to new_in succeeds; otherwise retry
    } while (!__sync_bool_compare_and_swap(&fifo->in, old_in, new_in));

    // 5. space safely claimed, start writing data
    write_data_to(fifo->in/*to*/, buf/*from*/, len/*size*/);

    return len;
}
```

CAS guarantees that at any moment only one thread can move `in` from the old value to the new one; the losers re-read the cursors and try again. Compared to locking, it skips lock contention and context switches, massively improving concurrent performance.

But this brings a new problem. kFifo is "write the data first, then update `in`", so `in` naturally marks where the data has been written up to; the Disruptor switched to "claim the space first, then write the data", and `in` loses that meaning. See the figure:

![Disruptor Alloc](img/Disruptor_Alloc.png)

<p align="center"><em>Figure 3: the trouble with claim-first-write-later — A's and C's blocks are done (hatched), B hasn't finished, so <code>In</code> can no longer serve as the readable boundary.</em></p>

ThreadA, ThreadB and ThreadC each claimed a region; A and C have flushed their data in, B hasn't finished. Using `in` as the readable boundary now would simply be wrong. So the Disruptor allocates an extra piece of memory marking, in order, whether each slot has been fully written, and the consumer verifies the corresponding mark for every slot it reads. In Java, where objects are all references, one mark happens to be one pointer in size, so this is natural; in environments that copy data straight into the queue, the scheme needs some adaptation.

## 3. BqLog's first axe: the MISO queue

### 3.1 CAS is not as good as it looks

CAS is the standard tool of concurrent programming, but it has a problem that can't be designed away: **it wins by competing, so somebody has to lose.** The losing thread re-reads, re-computes and retries. Under heavy contention, a thread may lose several rounds in a row — the system doesn't deadlock, but that thread's progress is badly stalled, and overall throughput and latency suffer.

So is there an atomic operation where everyone succeeds on the first try and never retries? Yes: `fetch_add`.

### 3.2 fetch_add: grab the number first, check the ticket later

`fetch_add` does something extremely simple: add a value to a variable and return the **old** value from before the addition, all atomically. It has no concept of "failure" — no matter how many threads pile in, each one walks away with a result that belongs to it alone.

Rewriting the concurrent write with it, the code is suspiciously short:

```c
unsigned int kfifo_in_concurrent(struct kfifo *fifo, const void *buf, unsigned int len)
{
    // 1. claim memory directly: what you get is [from, from+len)
    unsigned int from = __sync_fetch_add(&fifo->in, len);

    // 2. this range is exclusively ours now, just write
    write_data_to(from/*to*/, buf/*from*/, len/*size*/);
    return len;
}
```

Suppose `in` starts at 0, and threads A, B, C concurrently claim 10, 5 and 15 bytes. Afterwards their starting positions might be:

- **A**: 0, **B**: 10, **C**: 15, with `in` ending at 30

Or:

- **B**: 0, **C**: 5, **A**: 20, with `in` still ending at 30

See the figure:

![fetch_add's allocation outcomes](img/performance/fetch-add-cases.png)

<p align="center"><em>Figure 4: fetch_add outcomes — order is uncertain, but every thread gets an exclusive range. The <code>Overflow!!</code> in the middle and the moving limit at the bottom are the next section's story.</em></p>

The order is uncertain, but everyone got an exclusive range, and nobody had to retry. (Ignore the middle and bottom of this figure for now — they're for the "overflow" and "rollback" stories in the next section.) Compare the temperaments of the two approaches: CAS says "look carefully whether anyone is competing, move only when it's provably safe, and redo the whole thing if the move fails"; fetch_add says "grab your number first and deal with any problem afterwards". BqLog chose the latter — **no stack of checks on the normal path; just `fetch_add` directly, and roll back only when there's genuinely no room.**

### 3.3 The fatal flaw of fetch_add, and the rollback algorithm

If fetch_add is this simple and fair, why is every message queue on the market still using CAS? Because it has a fatal flaw: it only makes the claiming action atomic — **it doesn't guarantee there's enough capacity.**

Back to that figure, and this time stare at the middle: the `Limit` sits at 25, but all four threads decide there's room for themselves and `fetch_add` together, driving `In` straight to 40 — everything past 25 (the tail of C and the whole of D) is out of bounds; the red `Overflow!!` is the scene of the accident. The memory the last threads claimed doesn't actually exist.

The bottom of the figure shows the other situation: the consumer pushes `Out` forward to 15, the limit moves along to 45, and now `In`(33) falls inside the legal range. Keep this "the limit moves" detail in mind — the rollback logic needs it.

CAS doesn't have this problem, because the final update only succeeds if `in` is exactly what it was when the space check was made; anyone touching it in between causes a failure and a retry.

To keep fetch_add's fixed cost while still getting correct results, `bq::miso_ring_buffer` designed a rollback mechanism. The pseudo-code for allocation:

```c
void* bq::miso_ring_buffer::alloc(size_t len)
{
    // 1. compute remaining space (using this thread's cached out, which usually avoids touching the shared variable)
    size_t free_space = this->size_ - (this->in_ - this->out_cache_);
    if (len > free_space) {
        return nullptr;  // obviously not enough space, fail fast
    }

    // 2. grab a number first: claim memory; what you get is len bytes starting at from
    size_t from = __sync_fetch_add(&this->in_, len);

    // 3. check the ticket afterwards: is this claimed range actually valid?
    while (from + len > this->out_ + this->size_)
    {
        // 4. genuinely out of bounds; must roll back
        size_t expected_in = from + len;
        if (__sync_bool_compare_and_swap(&this->in_, expected_in, from))
        {
            // if in_ equals from + len, roll it back to from; otherwise spin and retry the rollback
            return nullptr;  // rollback succeeded, report not enough space
        }
        yield(); // give up the time slice; don't burn compute like crazy
    }

    // 5. the memory is valid, return it (either it was valid when claimed, or the consumer freed space while we were trying to roll back)
    return to_addr(from);
}
```

Sharp-eyed readers may object: isn't there still a CAS in that rollback? Isn't it slow? Mind the precondition for a rollback: the queue is already full. At that point the system's bottleneck has already become "the business needs a bigger queue, or we wait for the consumer to free space" — the cost of that CAS is irrelevant by comparison. **Good steel goes on the blade: the hot path finishes in one fetch_add; the cold path is allowed to spin.**

Now the key question: why must the rollback use CAS, instead of just `fetch_add(&this->in_, -len)` to subtract what each thread added? The difficulty is that once `in` has been pushed over the limit, no producer knows what value it should be rolled back to — who subtracts, who doesn't, by how much?

An example, from this figure:

![Rollback 1](img/rollback_1.png)

<p align="center"><em>Figure 5: A, B and C have grabbed their numbers; B's and C's ranges run past the limit.</em></p>

Before this round of allocation, `in` is 1000 and the remaining space only allows writing up to 1012. A, B and C claim simultaneously, stretching `in` to 1030; B and C find their ranges over the limit and prepare to roll back individually. Suppose we took the naive "just subtract your own length" approach, and thread B went first — the following chain of accidents could happen:

- 1. Thread B runs `fetch_add(&in, -5)`, bringing `in` down to 1025;
- 2. The consumer thread D suddenly digests a big chunk of data, so space now allows writing up to 1040;
- 3. Thread B immediately retries its 5-byte allocation, gets the range 1025–1030, and starts copying data in — `in` is now 1030;
- 4. Thread C enters the rollback flow, suddenly realizes its 1015–1030 range is actually fine now, and starts writing directly.

The resulting crash site:

![Rollback 2](img/rollback_2.png)

<p align="center"><em>Figure 6: the naive rollback's crash site — B subtracts its own length, allocates again, and its data ends up colliding with C's.</em></p>

Thread B's and thread C's data overlap.

So the correct rollback posture is: **let `in` retreat one range at a time, from the back; each thread is only responsible for rolling back the range it claimed, and only after confirming it holds the current tail** (the CAS checks `in == from + len`); if you're not the tail, wait. If, while waiting, enough space appears, no rollback is needed — just write. This way C first retreats 1030 to 1015, then B retreats 1015 to 1010 — orderly.

The real implementation lives here:  
[miso_ring_buffer.h](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/buffer/miso_ring_buffer.h)  
[miso_ring_buffer.cpp](https://github.com/Tencent/BqLog/blob/main/src/bq_log/types/buffer/miso_ring_buffer.cpp)

### 3.4 Claiming a slot isn't finishing the write: block status

Back to the problem the Disruptor ran into in section 2.2: claim first, write later, and `in` can no longer mean "where the data has been written to". A grabbed its slot first but got scheduled out by the OS; B grabbed a later slot and has already finished. If the consumer only looked at `in`, it would read A's half-baked data.

The Disruptor's answer is a completion flag per slot. BqLog's MISO is similar, but coarser and cheaper: the whole ring is divided into **blocks of 128 bytes (C)**, and the first block of each chunk carries an 8-byte head:

```text
block_num: 3 B | status: 1 B | data_size: 4 B | payload ...
```

`status` has just three values: `unused` (no data), `used` (write finished, readable), `invalid` (this range is void, skip it). The producer writes `block_num` at claim time, copies the data, then sets the status to `used` with release semantics; the consumer reads `used` with acquire before touching the payload, and `unused` means there's nothing beyond. The `fetch_add` used for claiming can itself be the weakest relaxed ordering, because publishing the data is the status bit's job, not the cursor's.

![The MISO ring's layout and the chunk head](img/performance/miso-layout.png)

<p align="center"><em>Figure 7: the whole MISO ring with its <code>Cursor In</code>/<code>Cursor Out</code> on top; the middle unrolls the ring — memory is cut into 128-byte-aligned blocks, data is chunks packed end to end, and the consumer reads straight through; bottom right is a close-up of the chunk head, three fields adding up to 8 bytes.</em></p>

`invalid` is for the ring's tail. A chunk's data must be contiguous in memory; if the space left at the tail can't hold the whole chunk, that tail range is marked `invalid`, the consumer skips it, and the data starts over from the beginning of the ring. This way the upper layer always gets one contiguous piece of memory and never has to deal with "one chunk split in two".

Why must the allocation granularity be 128 bytes? A 61-byte internal log, plus the 16-byte context on the LP path (covered later) and the 8-byte chunk head, totals 85 bytes — and still occupies a full 128-byte block; the leftover space is indeed wasted. But here's what we buy with it: **different producers always write to different cache lines.** You write yours, I write mine, nobody steps on anyone's cache line. That trade is worth it — and the next section explains exactly why.

Two more load-reducing tricks:

- The write thread keeps a thread-local (TLS) copy of the read cursor, uses the cached value for capacity checks, and only reads the real shared `read_cursor_` when space runs short. The cached value is only ever stale, and being stale only underestimates free space — at worst one wasted refresh, never an overwrite of unconsumed data.
- The consumer doesn't publish `read_cursor_` after every entry either; it waits until a quarter of the ring has accumulated, or the buffer reads empty, then batch-marks statuses `unused` and publishes the read cursor. The synchronization cost is amortized over a big batch of data.

Put kFifo's and MISO's synchronization moves side by side, and you can see exactly what multiple producers cost:

![kFifo vs MISO: the synchronization flows](img/performance/sync-compare.png)

<p align="center"><em>Figure 8: kFifo vs MISO, synchronization side by side. Everything MISO adds sits on the "claim" and "give back" moves; the data copy itself has zero synchronization.</em></p>

kFifo publishes twice in its life: the writer releases `In`, the reader releases `Out`. MISO, because many threads write at once, adds a few moves: acquire-read `Out` before claiming (to get the real capacity), release-publish `In` after the `fetch_add` grab, and release the block status after the data is copied; the consumer, for its part, acquire-reads the block status, and only after a whole batch flips statuses and publishes `Out`. See the pattern? **Every synchronization happens at "claim" and "return" — the data copying itself contains not a single line of synchronization.**

At this point MISO has already left traditional queues a street behind. But it's not the end of the road.

## 4. The motive for the second axe: the data storm between CPUs

MISO eliminated locks and CAS retries, yet push the benchmark to dozens of threads and the performance curve still bends down. The bottleneck isn't in the code logic — it's in the CPU's hardware structure.

In a modern multi-core CPU, every core has its own L1 cache, and data moves between cores in units of cache lines. To keep everyone's view of memory consistent, the cores run a cache-coherence protocol (MESI, for example). Skip the protocol details; just remember one consequence: **when multiple cores modify data in the same cache line simultaneously, the "write ownership" of that line gets passed frantically between cores** — you grab it, I grab it back — and every hand-off costs dozens to hundreds of clock cycles. Early CPUs carried these messages on a ring bus; once core counts grew, on-chip networks like 2D-Mesh took over — but however the interconnect evolves, the cost of moving a cache line between cores stays.

This phenomenon has a vivid name: the **data storm**. The Linux kernel's MCS SpinLock exists precisely to treat it — every waiting CPU spins on its own memory instead of everyone hammering the same lock variable.

![Ring Bus, 2D-Mesh and the MCS SpinLock](img/performance/cpu-data-storm.png)

<p align="center"><em>Figure 9: the interconnect moved from the ring bus to 2D-Mesh NoCs, but the cost of passing a cache line between cores never went away. The MCS SpinLock's answer: everybody spins on their own lock variable.</em></p>

Back to MISO: no matter how many producers there are, the target of their `fetch_add` is **the same `write_cursor_`**. That cache line is the single most contested ground of the whole arena — however many threads write logs, that's how many times it gets grabbed between cores. `fetch_add` only removes retries at the software level; the cache-line battle underneath is fought for every single write.

Some may ask: why not just lay the cursors out separately? We did — MISO already puts `write_cursor_` and `read_cursor_` in different cache lines (that's avoiding False Sharing: two unrelated variables happening to share one cache line and dragging each other down). But false sharing can be separated; **true sharing can't**: `write_cursor_` is inherently one single piece of data modified by all producers — no amount of padding changes that.

To go one step further, only one direction remains: **high-frequency producers shouldn't be modifying the same location at all.**

## 5. One SISO per thread — but do the math first

### 5.1 What a SISO is

Since the root of contention is everyone sharing one write cursor, give every high-frequency thread its own queue: the `siso_ring_buffer`, single producer, single consumer (Single In, Single Out). The write cursor is modified only by that thread, the read cursor only by the consumer — ordinary writes don't involve even one atomic competition.

The SISO's memory layout also pushes "isolation" to the extreme. Its head has four regions of C bytes each:

1. Consumer-local state: cached read position, known write position and capacity;
2. Producer-local state: local write position, cached read position;
3. The shared read cursor;
4. The shared write cursor.

![The SISO head's four aligned regions and the data area](img/performance/siso-head.png)

<p align="center"><em>Figure 10: the SISO head separates read-side and write-side state into four C-byte regions; the data area behind it is used in 8-byte units.</em></p>

Both sides normally play with their own local caches; only when "out of space" or "out of data" do they read the other side's shared cursor. With only one writer, the SISO doesn't need MISO's status bits either: the chunk head is just 8 bytes (`block_num` and `data_size`, 4 bytes each), and the allocation granularity shrinks from 128 bytes to 8 — writing alone, there's no neighbor to defend against, so no need for a big isolation belt.

The consumer reads a SISO in batches: take the write cursor at batch start as the end, read everything in that range in one go, return it all at once, then move on to poll the next queue. This amortizes synchronization, and gives the consumer natural chances to "go check the next shop" — otherwise one high-frequency thread writes happily and the consumer keeps orbiting it while other threads' logs pile up unread.

### 5.2 But the memory bill doesn't work out

So why not give every thread a SISO? Do the math: 1000 threads, 64 KiB each — that's 62.5 MiB of data area alone, management structures not included. And in reality, a huge number of threads write one or two entries every once in a while. Handing each of them a dedicated buffer is pure waste.

So BqLog's actual structure is a mixed doubles team:

```text
Low-frequency threads: everyone shares one MISO (called LP, the Low Performance path — "low" only relative to HP; it's still a high-performance queue)
High-frequency threads: each "thread × Log" gets its own SISO (called HP, the High Performance path)
```

Who decides high vs low frequency? Each thread's TLS records its own write frequency for each Log; by default, writing 1000 entries per second promotes you to HP, and falling below demotes you back to LP. The check is deliberately crude — just count calls over a second. It only needs to answer "is it worth giving this producer a dedicated buffer", not to be precise.

## 6. Why not just one big SISO array?

Now the requirement is clear: one SISO per high-frequency thread. The most naive implementation is one big array, with each thread occupying a slot. BqLog didn't do that; instead it built the seemingly much more complicated structure of Groups and the Free/Stage/Used lists. Why?

Imagine the array approach's two fatal weaknesses:

1. **Threads come and go dynamically.** As a game runs, new threads start logging at any moment, and threads exit at any moment. Inserting or removing a slot in the array — while the consumer thread is in the middle of traversing that array — means locking the whole thing: every new thread arrival makes everyone pause. The more frequently threads change, the more absurd the pauses.
2. **Memory can't be pre-allocated as a whole.** Growing the array means realloc or moving data, while the producers currently writing are still holding pointers to the old addresses.

And there's a deeper constraint: **recovery after a crash.** BqLog can use memory-mapped files (mmap) to keep buffer contents on disk, so logs that never got consumed can be salvaged after a crash and restart. To recover, you need to verify "was this memory left behind by that same Log?", and to discard only the broken parts. With one giant array where all threads are mixed together, there's no way to verify or recycle in pieces.

BqLog's answer is to separate these concerns: **memory is organized in Groups; ownership is managed with linked lists.**

### 6.1 Group: one contiguous piece of mmap memory

![The full log_buffer picture: Block, Group, Group List](img/performance/log-buffer-structure.png)

<p align="center"><em>Figure 11: unfolded layer by layer — every block starts with a 128-byte management area (Next index, ABA mark, misc) with a SISO sitting right behind it; a Group holds N blocks managed as Used/Stage/Free lists; Groups are strung into a list that the single consumer thread patrols. The lower half is the complete map of synchronization points: a burst marks wherever inter-thread synchronization happens — CAS on the Free/Stage heads, plus one RW lock on each Group node. You might ask: does CAS still count as lock-free? Yes — nobody waits, nobody holds a lock, a loser just retries on the spot; the real cost is the cache line pinging between cores (the MESI coherence protocol), which is exactly what the burst marks. Other than these, there are no locks.</em></p>

A Group is one contiguous block of memory (usually one mmap file) holding a fixed N blocks — 16 on desktop platforms, 2 on mobile. Each block starts with a C-byte management area (list linkage, list type, misc data), followed in place by a SISO object and its data area. One block is one high-frequency thread's dedicated queue.

Allocating the whole thing at once pays off immediately:

- 16 SISOs arrive with a single mmap, instead of every thread asking the system for memory on its own;
- each Group corresponds to an independent mmap file whose head stores a checksum computed from the Log's configuration. Recovery verifies Group by Group: is this file mine? If yes, pick up the data inside; if not, or it's corrupted, discard just this file — the other good Groups are unaffected. The array approach can't do this "throw away only what's broken".

Groups are then strung into a linked list. Traversal uses **hand-over-hand read-write spin locks**: each node carries one RW lock; you lock a node only while you're on it, and release it as you move on. A new Group is always inserted at the list head, requiring only the head pointer's lock — **a consumer traversing the middle of the list doesn't even notice.** Deleting a Group locks only the predecessor node. That's how "adding or removing a batch of thread resources without pausing the whole world" is achieved.

![Hand-over-hand RW locks on the Group list](img/performance/group-list-rwlock.png)

<p align="center"><em>Figure 12: every Group node carries an RW lock guarding "the next node". In the picture Group 3 is being recycled and unlinked: only its predecessor gets locked; read traversals elsewhere on the list carry on as usual.</em></p>

When an entire Group goes idle, it's not freed immediately either — it rests in an object pool first, and only gets deleted after nobody uses it for a second. If a thread's burst passes and it comes back, it can pick up ready-made memory again.

### 6.2 Three lists: Free, Stage, Used

Inside a Group, the 16 blocks are managed with three linked lists. Note that list nodes need no extra allocation: the blocks are laid out contiguously, and each block's management area stores a **16-bit index** pointing at the next block, with FFFF meaning none. Using indices instead of pointers saves space, and has another benefit prepared for recovery: when an mmap is remapped, the base address changes and pointers are all dead — but indices stay valid.

![The Free, Stage and Used lists inside a Group](img/performance/group-three-lists.png)

<p align="center"><em>Figure 13: Used is touched by the consumer alone; Stage and Free are lock-free stacks. Any thread claiming a block pops one off Free's head, initializes it and pushes it onto Stage; on patrol the consumer takes the fresh blocks off Stage and links them into Used; retired blocks go back onto Free's head. Mind the note at the bottom right — the Free list must beware of the ABA problem.</em></p>

The three lists' division of labor:

- **Free**: ownerless blocks. A producer promoting to HP picks one up here. Multiple threads claim concurrently; it's a lock-free stack with CAS at the head.
- **Stage**: after a producer claims a block and finishes initializing it, it pushes the block here — "on the shelf". Also a lock-free stack with head CAS.
- **Used**: the blocks the consumer is polling. **Only the consumer thread ever touches this list**, so it's a perfectly ordinary linked list without even one atomic operation.

Why not cut Stage and let producers insert straight into Used? Because the consumer is traversing Used. A producer inserting nodes while the consumer walks the list would require very complicated coordination to stay safe. With Stage as a hand-over zone, the relationship is clean: **producers only put things on the shelf; the consumer, on its rounds, takes the new blocks off Stage in a batch and links them into Used at its own pace.** One only writes, the other only moves — neither disturbs the other.

This is the most elegant part of the whole structure. Look at a thread's lifecycle again:

- **A new thread arrives**: CAS-pop a block from Free's head, initialize it, CAS-push it onto Stage's head. It never touches anything the consumer is traversing — not a single lock.
- **A thread exits or gets demoted**: the producer marks its own block; next time the consumer comes around and reads the mark, it finishes consuming the remaining data, unlinks the block from Used and pushes it back onto Free. Removal doesn't lock the whole world either.

Of course, lock-free stacks have the classic ABA trap: the head goes from A to B and back to A, and CAS sees an unchanged value and lets you through — even though the list was manipulated in between. The fix here splits the 32-bit atomic value in half: the low 16 bits hold the block index, and the high 16 bits hold an ABA mark incremented on every operation. If the value returns to A but the mark moved, the trickery is exposed.

### 6.3 Don't get me wrong: once a block is claimed, the hot path has nothing

It's worth emphasizing: all the list operations above happen only at **low-frequency moments** — promotion, demotion, exit, recycling. A high-frequency thread that already holds a block spends its daily writes entirely on its own SISO: it touches neither Free/Stage/Used nor anything beyond its own shared cursors. The management complexity is amortized over the buffer's lifecycle, not charged to each log entry. That's how "looks complicated" and "runs fast" can be true at the same time.

## 7. Walking through three typical scenarios

The structure is all on the table, but talk is cheap. Let's walk through the three most confusing scenarios from start to finish: how a new thread gets in, what happens when an old thread bounces between high and low frequency, and how recovery works after a crash. After these three walks, all the separate parts above assemble into one machine.

### 7.1 Scenario one: a new high-frequency thread joins

Some business thread has been logging furiously; the counter in its TLS notices this second's writes have crossed the threshold (`high_frequency_threshold_per_second`, 1000 entries by default), so it's time to promote it from LP to HP and issue it a dedicated SISO. The whole process:

![A new high-frequency thread joining](img/performance/case-new-thread.png)

<p align="center"><em>Figure 14: a new high-frequency thread claiming its block — pop one off Free, initialize it, push it onto Stage; the consumer picks it into Used on patrol.</em></p>

Four steps on the producer side:

- 1. **Pop a block off some Group's Free list.** Free is a lock-free stack; one CAS at the head does it. Finding the right Group means walking the Group list with hand-over-hand read locks — read locks are shared, so there's basically no contention; only when every Group's blocks have all been claimed and a brand-new Group must be inserted at the list head do we take a brief write lock on the head pointer, which a consumer working mid-list never even notices.
- 2. **Initialize.** Build the SISO in place inside the block, and write this queue's context into the management area's misc: version, sequence number, TLS identity. This 16-byte little thing is the protagonist of the next scenario — remember it.
- 3. **Push it onto the Stage list.** Again one CAS at the head.
- 4. Done. Go back to writing logs.

The consumer side is even lazier — it does nothing at all. It patrols as usual, and when it strolls up to this Group it pops a block off Stage, verifies the context, and links it onto the Used tail — Used is its own private list, plain linked-list operations without even one atomic instruction.

See what Stage is for now? It's the hand-over windowsill between producer and consumer. Producers only put things up; the consumer only takes things down; each list has exactly one writer, so nobody ever waits for anybody.

### 7.2 Scenario two: one thread bouncing between low and high frequency

This scenario hides the most easily overlooked pit in the whole design: **ordering**.

Look at this thread's day: first, in its low-frequency phase, it writes log A into the shared MISO; then it gets diligent, promotes to HP, claims a block and writes B and C; later it calms down, demotes, and writes D back into LP.

![The ordering problem when a thread switches between LP and HP](img/performance/case-frequency-switch.png)

<p align="center"><em>Figure 15: one thread writing A, B, C, D across LP/HP/LP — each queue is ordered internally; the cross-queue order is stitched back by the context sequence numbers.</em></p>

Every queue is internally ordered, but nobody automatically manages order across queues. If the consumer visits the HP block first, B and C jump ahead of A; if it handles the shared queue first, D may slip in front of B and C. Once logs go out of order, investigating a problem becomes hell.

Serialize with locks? That would tear up the road we just built. BqLog's answer is to issue each "phase" a household registration — the 16-byte context:

![The context's byte structure and the cross-phase sequence number](img/performance/context-order.png)

<p align="center"><em>Figure 16: the 16-byte context, byte by byte; each phase of the same producer carries one sequence number.</em></p>

The context holds a version, a thread-finished mark, an external-reference mark, a sequence number (seq) and the TLS identity. On the LP path every record carries one; on the HP path each block stores one, shared by all the messages in that block.

Why does the sequence number only track phases of "the same thread on the same Log", instead of issuing a global one? A global sequence means all threads go back to grabbing the same atomic variable — isn't that the old data-storm road all over again? A local sequence is enough, because the consumer only needs to answer one question: **has this thread's next-phase data arrived yet?**

The consumer keeps an "expected seq" per thread (TLS), with exactly one rule: if the number doesn't match, set the data aside for now. Stumbled upon an HP block with seq=1 while still expecting 0? Leave it, and go find seq=0 elsewhere first; once A is consumed the expectation becomes 1, then come back for that block; the block's messages can be read gradually, but only when the producer has completely abandoned the block and the consumer has read it dry is seq=1 finally closed and D allowed on stage.

There's a subtle point that must be kept straight: **"temporarily read empty" and "the producer will never write here again" are two different things.** The consumer reading the B/C block empty at some moment doesn't mean that block is empty for life — the producer might be writing a third message into it right now. So neither demotion nor exit is "pulling the network cable"; it's leaving a note: the producer marks its own block "removed", and a thread exit writes a "finished" mark into the context. On patrol the consumer reads the mark, drains the remaining data, then unlinks the block from Used and pushes it back onto Free. No locks anywhere in promotion or demotion — just marks and conventions. What the consumer needs is not an empty queue, but a clear statement.

By the way, this also explains a detail from part 1: why log time deltas use ZigZag encoding that allows negatives — data from different threads enters the file interleaved, so timestamps were never strictly increasing to begin with.

### 7.3 Scenario three: crashed — how do we recover?

The last scenario is where the Group structure faces its real exam. BqLog can put its buffers in memory-mapped files (mmap), so after a crash the logs that never made it to disk are still lying in those files. After a restart, how do we fish them back — in order?

![The crash-recovery flow](img/performance/case-recovery.png)

<p align="center"><em>Figure 17: crash recovery in three acts — the scene stays in the mmap files, restart verifies each file and voids half-finished pieces, then replays strictly by seq.</em></p>

Three acts.

**The moment of the crash.** The shared MISO is one mmap file; each Group is its own mmap file; oversize logs get their own file too. The in-memory Used/Stage/Free lists, the contexts and the unconsumed data all stay in the files exactly as they were.

**After the restart.** First, checksum every mmap file: if it matches, it was left behind by the previous self; if it doesn't, or the file is broken, throw away just that one file — the others are unaffected. That's the dividend of one file per Group: discard only what's broken. Then bump our own version by one (say 7 to 8) and scan the old data: build a minimum-seq table for each TLS household, and invalidate same-version leftovers (the half-finished writes from the crash instant) on sight. (How many generations back we accept is limited — `MAX_RECOVERY_VERSION_RANGE` is currently 2; anything older is simply not recognized as kin.) One detail shows real craftsmanship: blocks are linked by 16-bit indices, not pointers, because after remapping the base address has changed and every pointer is a wild one, while the indices stay valid; the TLS address stored in the context is likewise used only as the key for "which data stream this was" — never dereferenced.

**Replay.** Consume the old data strictly in each thread's seq order and hand it to the Appender's recovery segment — the segmented file structure from part 1, where recovered data goes into its own segment, undisturbed by new logs. Once the old accounts are settled, live writing under version 8 officially begins.

Now look back: why isn't a Group a simple array? Because an array can't answer these questions: which file is broken, which data stream belongs to whom, what about the entry half-written at the crash. Three lists plus the context household registration — that's what lets recovery reconnect the broken pipeline.

## 8. Two addenda: oversize logs, and what if it fills up

### 8.1 Oversize logs take a bypass — but order doesn't

Occasionally a log arrives that's bigger than the regular rings can hold (say, a big memory dump). No need to enlarge all the resident buffers for it. BqLog has a dedicated oversize buffer for the real payload, while the LP holds only a parent record marked external-ref — which still takes part in the same sequence queue.

On commit, the external payload is committed first, then the parent record is published. The consumer sees the parent record, locates the payload by version, TLS identity and seq, verifies it, and reads. If allocating the external payload fails, the already-reserved parent slot must be turned into "skippable" and the seq corrected — otherwise that hole would make all following messages wait forever for a predecessor that will never show up.

### 8.2 When the consumer can't keep up

If production outpaces consumption for long enough, any finite buffer fills up eventually. No atomic trick can bypass that — it's conservation of capacity. The only choices are:

- `discard`: new messages fail to write and are dropped;
- `block`: wake the consumer, producers wait and retry;
- `expand`: when LP is tight, move to HP; when HP is tight, add blocks — trade more space for time.

Note that expand is not a realloc of a ring in use — the addresses in the two sides' hands never suddenly go invalid; what grows is "threads can switch to another road". So the config item `log.buffer_size` is just one piece of the whole structure, not the process-wide memory ceiling of the logging system; HP count, Group granularity, oversize logs, Appender caches and mmap all count separately.

And the consumer doesn't get kicked awake for every single message either. Producers only request a wakeup when space is short or running low, and `awake_flag` merges duplicate requests; the background thread works while there's work, and waits on notification or timeout only when idle.

## 9. Wrapping up

From a plain circular array to here, what this data bus actually saves maps to concrete locations:

- Contiguous rings: per-entry node allocation is gone;
- Cursor caching and batch return: most shared-variable accesses are gone;
- MISO's fetch_add + rollback: CAS retries are off the hot path, and allocation completes in fixed steps;
- HP SISO: high-frequency threads stop competing for even one shared write cursor — the root of the data storm is gone;
- Group + Free/Stage/Used: lock-free thread add/remove and crash recovery hold at the same time;
- Context sequence numbers: local order connects the paths, without demanding a global sequence;
- Oversize: rare big records are lifted out of resident capacity.

None of this is a free lunch either: LP's 128-byte alignment wastes space on small records, HP costs extra thread-local memory, the Group management paths still have locks, and back-pressure makes threads wait. Whether it pays off depends on your log frequency, thread distribution and message sizes. But for the scenarios we face — dozens to hundreds of threads, tens of millions of entries per second — this structure is the best answer we've got so far.

The next article changes perspective: after a log entry enters the bus, along the pipeline of copying, hashing, table lookup, encoding and I/O, what repeated waste remains — and how it gets squeezed out, bit by bit.

## Continue with the source

- [log_buffer.h](../src/bq_log/types/buffer/log_buffer.h), [log_buffer.cpp](../src/bq_log/types/buffer/log_buffer.cpp): TLS, context, switching order, oversize logs and recycling.
- [miso_ring_buffer.cpp](../src/bq_log/types/buffer/miso_ring_buffer.cpp), [siso_ring_buffer.h](../src/bq_log/types/buffer/siso_ring_buffer.h), [siso_ring_buffer.cpp](../src/bq_log/types/buffer/siso_ring_buffer.cpp): reservation, publishing, ring-tail handling, batch reads.
- [group_list.cpp](../src/bq_log/types/buffer/group_list.cpp), [block_list.h](../src/bq_log/types/buffer/block_list.h): contiguous allocation and index lists.
- [bq_log_api.cpp](../src/bq_log/api/bq_log_api.cpp), [log_worker.cpp](../src/bq_log/log/log_worker.cpp): back-pressure and wakeups.
- [test_log_buffer.h](../test/cpp/test_log_buffer.h), [test_miso_ring_buffer.h](../test/cpp/test_miso_ring_buffer.h), [test_siso_ring_buffer.h](../test/cpp/test_siso_ring_buffer.h): the corresponding tests.
