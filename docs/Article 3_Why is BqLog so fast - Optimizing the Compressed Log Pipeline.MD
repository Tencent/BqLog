# Why is BqLog so fast? Part 3: How much work can share one memory load?

[简体中文](<文章3_为何BqLog如此快 - 压缩日志执行路径优化.MD>) · [Series](PERFORMANCE_DESIGN.md) · [Previous: the data bus](<文章2_为何BqLog如此快 - 高并发环形队列.MD>)

Parts 1 and 2 solved two big problems: storing less repeated content in the file, and getting messages from producers to the consumer efficiently. With both done, is compressed logging fast enough?

Run it for real and you'll find that a lot of time goes to much less glamorous places: the same format string gets scanned three times end to end; after a hash-table lookup fails, the insertion probes all over again from the top; after encoding, someone notices the length field was over-reserved by one byte, so the entire string behind it gets moved once; every producer, after writing one message, habitually notifies a background thread that is plainly already working.

Each step looks cheap on its own — a few dozen nanoseconds. But logs have one characteristic: **high-frequency repetition.** A few dozen nanoseconds inside ten million calls becomes an overhead worth taking seriously. This article invents no new file structure; it just walks the current implementation's pipeline and drags out every repeated waste, one by one.

> Written against BqLog 2.5.0. The simplified pseudo-code explains ideas; for actual branches, see the source files listed at the end.

## 1. First, write a "work bill" for one log entry

Suppose the business runs:

```cpp
log.info("player {} enters level {}", player_id, level_id);
```

On the current async compressed path, it roughly goes through:

```text
check whether level and category are enabled
compute the space needed by format and arguments
claim memory from the bus
copy format and arguments
commit the record
consumer looks up the format template and thread template
encode time delta, indices and arguments
enter the file write cache
process in batches and write out
```

Every step looks indispensable. But note: **"this result is necessary" is not the same as "a separate pass is necessary to compute it."**

For example, we need to copy the format string into the bus, and we also need its hash. By the time the copy is done, the characters are already in registers — can we compute the hash from that same set of values on the spot, instead of reading the same memory again later?

Another example: the first table lookup misses, but the probing has already discovered an empty slot that could take an insertion. Why, after writing the template, should we probe from the top all over again?

Every optimization in this article follows this pattern. And to find them, staring at total time won't help — you have to trace "where does the data come from, who reads it, and what is it used for after that". That's the order we'll follow.

## 2. The cheapest log is the one you know you won't write before serializing

If this entry's level or category is filtered out, and we only decide to skip it after assembling the string, computing the length and claiming buffer — all that earlier work is wasted.

So the C++ wrapper checks the level bitmap and the category mask before entering serialization. This check touches tiny amounts of data; compared with the variable-length content that follows, it costs almost nothing, and the earlier it runs the more it earns. Especially when one Log has several Appenders attached: first do a coarse check with the merged enable-info, so entries that clearly nobody wants are stopped at the gate; each Appender then does its own precise filtering.

One calling-semantics point must be made clear, though. Argument expressions are evaluated **before** entering the function:

```cpp
log.debug("state={}", build_expensive_state_string());
```

Even if debug is filtered out in the end, `build_expensive_state_string()` has already run. The library's early filtering cannot un-run that expression for the caller. If building an argument is expensive in itself, the caller side needs its own enabled-check first.

This is the same lesson as many later optimizations: **figure out exactly when the work happens before claiming some "zero cost" — then you'll know which segment that zero actually covers.**

## 3. Length and layout: computed once, never twice

Fixed-size arguments are easy: how many bytes an int32, double or bool occupies is settled by the type — no need to inspect contents at runtime.

Strings are the troublemakers: some objects carry a size, some give you only a zero-terminated pointer, some are literals the compiler can recognize. Degrading all of them to strlen throws away information we already had.

The wrapper's string helper layer picks a path by type: for recognizable literals, the compiler may fold the length into a constant; for objects that already have a size, take it directly; only for strings that offer nothing but a bare pointer does an actual scan happen.

The argument layout follows the same idea: `size_seq` puts the compile-time-determinable parts and the runtime-only parts together, obtains each argument's size and the total in one pass, then serializes them in order straight into the target record.

Say the arguments are an int32, a double and a string of length n. All we need is: fixed head + a few fixed argument areas + n + alignment cost. Done in one pass. **No building a temporary object chain or temporary byte array per argument, then gluing everything into a bus message at the end.**

This kind of optimization produces no "amazing algorithm" diagram, but it cuts the number of layers a common call must pass through. The core idea is one sentence: **known lengths and known types should travel all the way to the final write position — don't lose them midway and re-guess them one layer down.**

## 4. Compute the hash while copying

An async log must guarantee that the data stays usable after the original format string dies — so the data must be copied into the bus. The compressed Appender must recognize identical formats — so it needs the hash.

The most intuitive way to write it:

```cpp
memcpy(destination, source, length);
hash = hash_bytes(destination, length);
```

Or move the second step to the consumer thread. Moving threads just changes who does the work — the second read isn't reduced at all.

The current `__api_log_write_begin` uses a fused function for UTF-8/UTF-16 format strings that have a direct address:

```cpp
head->format_hash = bq::util::bq_memcpy_with_hash(
    handle.format_data_addr, format_str_data, format_str_bytes_len);
```

To judge whether it's truly fused, don't stop at the function name — look inside the loop.

![Load, register and store paths of copy and hash](img/performance/fused-copy-hash.png)

<p align="center"><em>Figure 1: one pass of the 32-byte main loop yields four 64-bit values; the same values are stored to the destination and fed into the CRC. Below, the tail overlap at length 45.</em></p>

Its body simplifies to:

```cpp
// Illustration: the production implementation has length branches, hardware selection and tail handling
load_8_bytes(v0, source + 0);
load_8_bytes(v1, source + 8);
load_8_bytes(v2, source + 16);
load_8_bytes(v3, source + 24);

store_8_bytes(destination + 0, v0);
store_8_bytes(destination + 8, v1);
store_8_bytes(destination + 16, v2);
store_8_bytes(destination + 24, v3);

h0 = crc_update(h0, v0);
h1 = crc_update(h1, v1);
h2 = crc_update(h2, v2);
h3 = crc_update(h3, v3);
```

The key is not that "hash and memcpy sit on adjacent lines" — it's that they **genuinely use the same v0..v3**. With the compiler keeping those values in registers, the hashing half doesn't need to issue another whole round of loads over the same string. One memory read, two products.

Also, a fixed 8-byte memcpy is not a call into some complex library function: the compiler usually lowers it into a few simple loads and stores for the target platform, and along the way avoids unaligned typed-pointer access in source.

### 4.1 Why not keep accumulating into one CRC state?

If all 64-bit words updated the same state:

```text
h = crc(h, v0)
h = crc(h, v1)
h = crc(h, v2)
h = crc(h, v3)
```

Each step depends on the previous one's result, so even if the processor could execute other instructions in parallel, this dependency chain just has to wait. That's why there are four independent states: four dependency chains interleave, and get mixed into one 64-bit result at the end.

This is not "one vector CRC instruction processing all bytes at once" — it's instruction-level parallelism: within the same loop, some state updates can be prepared while others haven't finished. Hardware CRC and the software fallback are implemented separately, and fused copy and hash-only share the same core logic.

### 4.2 To shed a few tail branches, allow a little re-reading

What if the length isn't a multiple of 32? One way: after the main loop, handle the remaining 16, 8, 4, 2, 1 bytes layer by layer — a pile of branches. The other way is far more decisive: take the last 32 bytes counting back from the end of the whole string.

At length 45, the main loop handles [0,32) and the tail handles [13,45); the middle [13,32) gets read twice. Copying to the same destination offsets changes nothing, and the hash algorithm treats this overlap as part of its definition. A few bytes of re-reading in exchange for deleting a string of branches — worth it.

So the precise claim of this optimization is: **the extra full-length pass is eliminated, and the values already loaded for each block are reused.** It's not "every byte is read exactly once at any length", and certainly not "memory bus traffic necessarily halves" — the original second pass probably hit L1 anyway. But it genuinely reduces load instructions, loop control and access pressure; how much it earns, only measurement can say.

### 4.3 The computed hash must travel with the message

If the producer computes it but the consumer doesn't know, it was wasted — the consumer will compute it again. So the 40-byte log head reserves a `format_hash` field, published along with the record.

The consumer uses it directly when it's non-zero; when it's zero, it falls back to the plain hash-only path. Concatenated call stacks, wrappers filling in their own formats, certain encoding conversions — these paths may not be able to offer a direct address at begin time, so the fallback computation has its reason to exist.

Coverage differs across language wrappers too: Go's fused native interface hands the format address to begin; JNI, NAPI and friends have to be examined according to how each one obtains its data. Also, a genuinely zero hash falls into the recompute path — don't misread that as "this log is gone".

One last word on what this hash is: it's a 64-bit runtime key combined from multiple CRC states — not a cryptographic digest, and not a standard CRC64 just because of the name; the CRC instruction paths of different architectures have to be checked separately. It is never written into the file — reading a file does not depend on whatever hash the original machine computed.

## 5. Template lookup: figure out the access pattern first, then decide what the hash table looks like

With the format hash computed, the next step is finding the template index. The laziest way is a generic hash map: look up, insert if absent.

A generic container solves many problems, but log templates have their own temperament: **a few hot spots repeat endlessly, consecutive records often come from the same thread, and the value is just an integer index.** There's no need to start from the generic container's most complete path every time.

So the current Appender uses two levels: a tiny L1 first, then a bounded L2. The format L1 has 256 slots; the thread L1 has 64. L1 is direct-mapped: mix the key to pick a slot, compare the full key, and a hit yields the index; different keys colliding on one slot just fall back to L2 and then update L1 on the way out. The vast majority of hot lookups finish in one step.

On the thread side there's an even lazier check than L1: **is this thread ID the same as last time?** If yes, reuse the last thread template index directly. The SISO batch from part 2 happens to produce long runs of same-thread records, so this check earns real money.

The format key combines the string hash, category and level:

```cpp
key = format_hash ^ ((uint64_t(category_index) << 32) | level);
```

The same string used under different categories or levels must yield different templates. That's the template's logical composition — looking up by string content alone won't do.

![Two-level cache, split arrays and the insert token](img/performance/template-cache.png)

<p align="center"><em>Figure 2: L2's keys and values are two contiguous arrays; the empty-slot position found during lookup can be handed to the later insert for reuse.</em></p>

## 6. One less pointer chase, one less repeated probe

### 6.1 Why keys and values are stored separately

L2 uses open addressing with linear probing; the key is a uint64 and the value a uint32. Written as a plain struct array, alignment padding pushes each entry to 16 bytes; split into `keys[]` and `values[]`, and the body is 12 bytes per slot.

Splitting has another benefit: no per-entry node allocation, and probing visits adjacent slots, preserving locality. The cost is that a slot's key and value live in two arrays — whether that's faster depends on the actual access and cache behavior — but at least the memory bill is crystal clear.

That bill is not small: with the default format limit of 100000 entries and about 50% load factor, the array capacity rounds up to a power of two — possibly 262144 slots, about 3 MiB of body. And that's just this one L2 table, excluding L1, the allocator, the old and new tables coexisting during a resize, and other Logs' caches. Multiplying "12 bytes per entry" by the entry count underestimates the memory.

### 6.2 The lookup already walked to an empty slot — why should the insert walk again?

Suppose the key's home is slot 3:

```text
slot 3: occupied by another key
slot 4: occupied by another key
slot 5: empty
```

When the lookup fails, we've already seen with our own eyes that 5 is free. But a generic interface has only `find(key)` and `insert(key, value)` — so after the Appender writes the template into the file and comes back to insert, that same probing happens all over again.

The current `find` returns an `insert_token`: a slot number plus a `table_revision`. If the table hasn't grown, shrunk or otherwise changed in between and the slot is still empty, `insert` lands directly on the token; if the revision doesn't match, it honestly searches again.

It looks like carrying along just two small integers, but on a steady miss path it eliminates an entire repeated probe. This kind of optimization breaks no theoretical limit — **it just refuses to throw away information the previous step already earned.**

### 6.3 Why the slot hash needs another round of mixing

The hash of the format content and the hash used for slot selection are not the same thing. Inputs may be aligned thread IDs, or may put category values in the high bits — take the low bits straight as the mask, and certain inputs pile up in the same small region.

Currently, slot selection runs through Murmur3's fmix64 finalizer first, then takes the mixed high bits. The purpose is plain: spread information from every input bit into the slot computation, reducing the clustering of these structured keys. It doesn't promise that "no adversarial input can ever build a long probe chain" — attack-proofing is not the topic here.

Also distinguish two events: "different keys colliding on the same slot" and "different formats producing the identical full 64-bit key". The former is handled correctly by comparison and probing; the latter currently gets no second verification against the original format string. That's a hashing assumption of the implementation — "64 bits is big" is not a collision proof. It's discussed together with performance because skipping the string comparison is part of the deal.

## 7. When the cache is full, sometimes you shouldn't accept the new entry right away

Imagine a cache that holds 100 templates, while the logs cycle through 101 of them in order. If every miss immediately evicts an old entry, then after one full cycle, the entry you're about to need has just been swapped out — the cache worked the whole time for nothing. A more brutal scenario: one scan floods in tens of thousands of dynamic formats that appear exactly once, and the hot entries used every day all get squeezed out.

The cache is not the single source of truth — the file is — so there's no need to guarantee every new template permanent residence. When L2 is full, the current design **accepts only one out of every 64 new insertion attempts**, with a round-robin position picking the victim. A scan crashing the party can then displace at most a scattered few of the hot set.

"Not accepting" here only means **not kept in L2**: the format template and the log still get written to the file, and L1 may retain the recent mapping. Next time the forgotten format shows up, at worst a fresh copy of the template is written — the file grows a little, but not a single log is lost.

This is another three-way trade: memory ceiling, CPU cost, file size. Accepting everything thrashes; accepting nothing duplicates; sampled acceptance gives a controllable notch between the two. It's not LRU, and it doesn't claim to be best for every kind of hot-set migration — but for this scenario, it's enough.

The resize posture is also worth a mention: allocate the new array first, migrate existing entries, then release the old one; if allocation fails, keep the old content and carry on. A cache allocation failure must not turn into an "all existing index state lost" accident. Of course, the transient memory peak of old and new coexisting during migration has to be budgeted too.

## 8. UTF-16 optimization: don't treat every character as the hardest kind

Part 1 described the UTF-Mixed format: narrow the ASCII first, and when content unsuitable for fast processing appears, store a UTF-16 suffix. This section shows how it actually runs.

The scalar version reads four UTF-16 characters into one 64-bit value at a time, and checks each character's high byte and bit 7 with a mask:

```text
v & 0xFF80FF80FF80FF80
```

A zero result means all four characters are in ASCII range, so just extract the low bytes and pack them. One load does both the test and the packing — the same old "once it's read in, do a little more" idea.

SIMD stretches the same idea over more characters: load several vectors at once, merge them to test for non-ASCII, and on the high-probability path narrow and store directly; blocks that really do contain complex characters fall back to the general handling. SSE, AVX2 and NEON use different instructions, but the goal is the same.

![The byte-level trade-off of UTF-Mixed](img/performance/compressed-utf-mixed.png)

<p align="center"><em>Figure 3: whether a full transcoding is worth it depends on the resulting byte count and the conversion work at hand — never on the compression ratio alone.</em></p>

Log format strings usually have long fixed English prefixes, so the fast path earns easily. When Chinese or other non-ASCII appears very early, UTF-Mixed may keep a long UTF-16 suffix — that's not a bug; that's exactly what "choosing to convert less" means.

Don't miss one more layer of savings: after a template hit, the same format is not re-converted for every single log. But argument strings change every time and still have to be processed — the template-reuse effect must not be stretched over the arguments' heads.

One cold-path fact about appending to old files: to restore the original UTF-16 format's hash, the UTF-Mixed form has to be expanded into a temporary buffer and re-hashed. There really is extra reading here, but it happens at file-open-and-scan time, a cold path. **Mixing cold paths and the per-record hot path into one discussion makes any optimization talk pointless.**

## 9. Variable-length encoding can skip a pass too: write the body first, fill in the length last

Part 1 explained that an item's head carries the body length. The intuitive approach: compute the final length, write the head, then write the body. But the final length depends on integer VLQ and string encodings, and knowing it precisely in advance usually means walking all the arguments once first.

If you then encode for real, the same set of decisions gets made twice.

The current Appender instead: conservatively estimates a range, claims it from the write cache, **leaves room for the head, and writes the body directly**. When encoding finishes, wherever the pointer stopped is the true length — go back and fill in the length field.

![Completion boundary, reserved head and length backfill in the write cache](img/performance/write-cache-backfill.png)

<p align="center"><em>Figure 4: green marks fully completed records, blue the body being encoded. finished_cursor only advances to the end of a complete record.</em></p>

An interesting small problem pops up here: the estimated length VLQ needs two bytes, but after real encoding one byte suffices. Now what?

The most honest approach is shifting the entire body left by one byte. But re-reading and re-writing a whole string just to save one byte is a losing deal however you count it. So the UTF-Mixed argument path allows leaving a `00` placeholder; the decoder knows this rule and skips it on read.

Note this is not a license to insert `00` anywhere, and it can't be confused with legitimate NUL characters inside strings. It happens only at specific reserved-length differences, handled explicitly by the corresponding decoder.

If the estimate for a format template doesn't satisfy the head-length condition, the reservation may also be revoked and redone. So we can't boast "encoding never makes a second pass" — the accurate statement is: **in the common case, there's no need to measure precisely first and then rewrite everything.**

`mark_write_finished()` is responsible for keeping another easily-confused pair apart: "where we've written up to" versus "how much complete data is ready to flush". Recovery or flush must never treat a half-written item as a complete log.

## 10. Cache alignment and batched I/O: amortizing the last stretch of cost

One log entry is a few dozen bytes; calling the file-write interface per entry makes fixed costs painfully visible. The Appender's write cache encodes many records contiguously and outputs them in batches.

`write_cache_size` tunes the target cache, currently 64 KiB–4 MiB, normalized to powers of two. A bigger cache reduces write calls, but occupies more memory and means more "not yet flushed" data — throughput isn't the only thing to look at.

One especially large record can still push a temporary allocation past the target, so this config is not an absolute hard memory ceiling either. And it lives on a different level from the bus's LP/HP capacities from part 2 — tuning one doesn't automatically tune the other.

Alignment does real work here too. The encrypted payload is XORed with the mask block by corresponding offsets: if source data and mask are relatively aligned, whole-block SIMD loops come easily; otherwise a pile of head-and-tail handling follows. The Appender adjusts the write cache's padding according to the file offset, rather than naively assuming "if the buffer's start address is aligned, everything's fine forever".

Of course, adjusting padding may itself trigger a memmove. So such operations should happen at necessary boundaries, not bounce back and forth per record. Performance comes from removing repetition along the whole path — not from a codebase where memcpy and memmove can never be seen.

## 11. Why encryption looks cheap: at what frequency does the expensive work happen?

The current implementation neither runs RSA per log entry nor feeds the payload through AES entry by entry. Part 1 drew the segmented structure; here we read it as a cost model:

```text
total cost ≈ segments × per-segment initialization cost
           + payload bytes × steady transform cost
           + batches × per-I/O fixed cost
```

Segment initialization includes wrapping the AES key with RSA and AES-processing the 32 KiB mask; afterwards the payload is XORed in place, batch by batch. With long segments and many messages, initialization amortizes away; with frequent short segments, maybe not.

![Fixed per-segment cost vs per-batch cost of segmented encryption](img/performance/encryption-segments.png)

<p align="center"><em>Figure 5: replace algorithm names with "execution count × work per execution", and it becomes visible why segment granularity and batch size change the result.</em></p>

In the public single-thread case, compression takes 79 ms and encrypted compression 83 ms; in the 10-thread case it's 759 ms vs 995 ms. Quoting only the first pair to claim "encryption is nearly free" is cherry-picking — concurrency, caches, consumption speed, data scale and one-time work all shift the proportions.

The security properties also deserve precise wording: this is a custom RSA/AES/repeating-mask scheme, with no AEAD authentication tag, and mask reuse is certainly not a one-time pad. A cheap hot path doesn't come with the guarantees of authenticated encryption; if you need tamper-proofing, add a separate mechanism.

## 12. Waking a thread costs time too

One more commonly ignored overhead: a producer finishes writing and habitually notifies the consumer. At low single-thread frequencies you see no problem; at high multi-thread frequencies, dozens of producers may repeatedly contend over the same notification state and mutex — while the consumer was already working and never needed the shout.

The current API requests a wakeup only when space is short or running low, and `awake_flag` merges duplicate requests. The background thread keeps consuming while there's work, and only when idle does it wait for a notification or timeout. So per-entry commits no longer each correspond to a system notification.

There's a throughput/latency trade here too: a low-frequency message doesn't necessarily enter the file immediately, and the background idle wait and the Appender's flush interval have to be read together. "Fewer notifications" does not imply "more real-time", and no wait parameter should be taken as a promise on tail latency.

Incidentally, thread IDs and thread names are also cached via TLS, so not every log entry pays for a system call; the consumer's "last thread" shortcut index further feeds on batch-read locality. The reuse of small states runs through the whole pipeline.

## 13. How do you know an optimization really works, and isn't just clever-looking?

See a fused copy and declare "surely 2x faster"; see SIMD and declare "double the vector width, double the speed"; see a hash-map hit and declare "constant time means free" — all of these judgments are too careless.

The reliable approach is shrinking the controlled experiment until it answers exactly one question:

| Change to verify | Control setup | Also check |
|---|---|---|
| fused copy+hash | same hash algorithm, fused vs copy-then-hash | same data, lengths, alignment, hot/cold cache |
| four CRC chains | single-chain vs multi-chain on identical input | codegen, result definition, processor differences |
| cache layout | same capacity and input, L1/L2 paths | CPU, hit rate, memory and template duplication |
| token reuse | same miss sequence, whether probe position is reused | correct invalidation after resize and modification |
| UTF-Mixed | English, Chinese, mixed content | time, size and decode correctness |
| batched write-out | same output and drop policy, varying cache size | I/O, RSS, call latency and flush time |

End-to-end benchmarks should also count how many entries actually got decoded in the end. If one queue drops logs when full by default and another blocks by default, comparing the speed of "producing calls" can easily mistake "wrote less content" for "faster implementation".

test_utils.h already contains copy/hash consistency checks under various lengths and offsets; test_bounded_hash_cache.h covers token invalidation, capacity and allocation failure; test_compressed_cache.h ties cache behavior to actually decodable output. They help us confirm the optimizations didn't change the results — but the isolated performance contribution of each change still needs the controlled experiments above.

## 14. Closing: the main thread is actually quite plain

Looking back over the whole article, not a single step demanded reducing work to zero.

The format string still has to be stored, the hash still computed, the template still looked up, the file still written. What changed is only one thing: **try never to pay twice for the same result.**

If the length is already known, don't scan again; if the data is already loaded, do the copy and the hash in the same breath; if the lookup already walked to an empty slot, hand that position to the insert; if the template already exists, don't convert and rewrite it; if the data must hit the disk, accumulate it into proper batches; if the consumer is already working, don't keep kicking it.

Meaningful performance optimization is usually not writing one isolated function to perfection, but letting adjacent steps share each other's results. BqLog's format, data bus and execution path have to be read together to understand how these gains stack up, layer upon layer.

## Continue with the source

- [bq_log_impl.h](../include/bq_log/misc/bq_log_impl.h), [bq_log_wrapper_tools.h](../include/bq_log/misc/bq_log_wrapper_tools.h): early filtering, length and argument layout.
- [util.cpp](../src/bq_common/utils/util.cpp): fused copy-hash, CRC multi-chains and UTF-Mixed.
- [bq_log_api.cpp](../src/bq_log/api/bq_log_api.cpp), [bq_log_go.cpp](../src/bq_log/api/bq_log_go.cpp): format_hash, native calls and bus commit.
- [appender_file_compressed.cpp](../src/bq_log/log/appender/appender_file_compressed.cpp), [bounded_hash_cache.h](../src/bq_log/types/bounded_hash_cache.h): cache, token, encoding and backfill.
- [appender_file_base.cpp](../src/bq_log/log/appender/appender_file_base.cpp), [appender_file_binary.cpp](../src/bq_log/log/appender/appender_file_binary.cpp): write cache, relative alignment, segmentation and encryption.
- [test_utils.h](../test/cpp/bq_common_test/test_utils.h), [test_bounded_hash_cache.h](../test/cpp/test_bounded_hash_cache.h), [test_compressed_cache.h](../test/cpp/test_compressed_cache.h): the corresponding correctness checks.
