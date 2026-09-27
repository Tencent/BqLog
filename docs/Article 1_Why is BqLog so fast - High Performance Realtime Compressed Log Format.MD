# Why is BqLog so fast? Part 1: Deriving a compressed log format from a line of text

[简体中文](<文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>) · [Series](PERFORMANCE_DESIGN.md) · [Next: the data bus](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD>)

Production logs need enough detail to diagnose failures without slowing the application or filling its storage. Mobile devices make these constraints particularly visible.

One common approach writes text first and compresses the closed file later. That saves archive space, but the writer still pays for formatting, copying and the initial file write. BqLog uses the structure of a log call to reduce that work before writing the file.

We will start with an ordinary log line, identify its repeated content, and use that to build the file format. The same reasoning leads to the layout for appending, recovery and encryption.

This article describes BqLog 2.5.0.

## 1. The cost of producing a text log

Consider an order system:

```cpp
log.info("New order, order ID:{}, price:{}, username:{}",
         32422144, 324.42, "张三");
```

The resulting text might be:

```text
2026-09-25 12:00:00.001 [Info] [Shop.Order] [Tid-2025 Worker25]
New order, order ID:32422144, price:324.42, username:张三
```

In the sample line, the bracketed fields [Info], [Shop.Order] and [Tid-2025 Worker25] are the level, the category and the thread identifier in that order, and the body is assembled from a format string and arguments. Producing this line requires converting the timestamp, integer and floating-point value to text, joining them with the fixed strings, and writing the result. Different string encodings may also require conversion. Moving formatting to a background thread still leaves that work to be done.

The next order arrives:

```text
2026-09-25 12:00:00.002 [Info] [Shop.Order] [Tid-2025 Worker25]
New order, order ID:32422145, price:174.45, username:李四
```

Only the time and three arguments changed, yet the fixed text was assembled and written again. At ten million records per day, repeating 50 bytes per record accounts for roughly 500 MB. Those bytes also pass through copies and I/O.

The call already supplies a useful boundary: **the format string is fixed; the arguments vary.** The file can preserve that boundary instead of storing the complete sentence each time.

## 2. Store templates and arguments

When a format first appears, store it and assign an index. Later records can refer to that index:

```text
Template 0: New order, order ID:{}, price:{}, username:{}

Record A: template 0, 32422144, 324.42, 张三
Record B: template 0, 32422145, 174.45, 李四
```

Formatting moves to the reader. Many diagnostic logs are never opened, so their text never needs to be produced at all. When someone does read them, the decoder combines the template and arguments. That work can run on a developer's machine or in an analysis service, reducing the load on the application device.

Level, category, thread ID and thread name also repeat. However, the same format may be used by many threads. Putting every field in one template would duplicate the format string whenever the thread changed.

BqLog therefore keeps two template types:

- **Format template:** level, category index and format string.
- **Thread template:** thread ID and thread name.

Each record references both. Changing the thread can reuse the format template, and changing the format can reuse the thread template.

Categories are known when the Log object (a logger instance) is created, so the file header area can store the category list. Format templates only need a category index. Categories, formats, threads and individual records are thus stored at the frequencies at which they change.

## 3. Distinguishing templates from records

New formats and threads can appear at any time, so format templates, thread templates and log records are interleaved in the file.

Separate files would require keeping their relationships consistent. Within one file, how does the reader know where one item ends? Format strings and argument lists have variable sizes. A terminator would require scanning and escaping, since binary arguments can contain any byte. With a length, the decoder knows where the item ends and the next one begins.

BqLog stores a sequence of **data items**, each with a type, a length and a body.

![Data item type bit and variable-length length field](img/performance/compressed-item-bits.png)

<p align="center"><em>Figure 1: an item header and body, with the bit layouts for two length cases. Adjacent fields are adjacent in the file.</em></p>

A fixed four-byte length would be simple, but most records are short. Variable-length integer encoding reduces this overhead.

### 3.1 Small integers use fewer bytes

BqLog uses a prefix-based VLQ encoding. For unsigned integers, its shape is:

```text
1 byte:  1xxxxxxx
2 bytes: 01xxxxxx xxxxxxxx
3 bytes: 001xxxxx xxxxxxxx xxxxxxxx
...
9 bytes: 00000000 + 8 data bytes
```

The position of the first 1 in the prefix identifies the encoded width. Longer encodings start counting from the first value that shorter encodings cannot represent:

| Bytes | Starting value | Range |
|---|---:|---|
| 1 | 0 | 0–127 |
| 2 | 128 | 128–16511 |
| 3 | 16512 | 16512–2113663 |

For example, 128 is the first value in the two-byte range, so it is encoded as `40 00`. Similarly, 16512 becomes `20 00 00`.

This VLQ is not the common LEB128, which stores seven bits per byte with the top bit as a continuation flag. BqLog's prefix layout keeps the first bit of any multi-byte encoding at 0; [section 3.2](#32-reuse-a-length-prefix-bit-for-the-item-type) puts that spare bit to use. The range offsets, prefix and byte order are defined in [log_utils.h](../src/bq_log/utils/log_utils.h).

### 3.2 Reuse a length-prefix bit for the item type

At the top level, an item is either a template or a log record. The template body distinguishes format templates from thread templates. The top-level type therefore needs one bit: 0 for a template, 1 for a record.

A separate type byte adds noticeable overhead to short records. A multi-byte VLQ starts with a zero bit, which can hold the type instead.

The length 128 is encoded as `40 00`. For a log record, setting its first bit produces `C0 00`; type and length still occupy two bytes.

The one-byte length 5 is `85`, whose first bit is already needed by the length prefix. Here the type gets a separate byte: `80` for the record type followed by `85`, giving `80 85`.

The decoder reads the type from the first bit, then checks the lower seven bits. If they are all zero, the length starts in the next byte. Otherwise it clears the type bit and decodes the length from the current byte. Bodies of at least 128 bytes can share the first byte with the type; shorter bodies use an explicit type byte.

The current body length is a uint32, so the item header occupies 2–5 bytes. The length excludes the header itself, both during backfilling and decoding.

## 4. Inside the templates

![Contiguous layouts of templates, records and arguments](img/performance/compressed-templates.png)

<p align="center"><em>Figure 2: the layout expands from templates to individual arguments. Colors identify field roles; byte counts describe their encodings.</em></p>

A format template body contains:

```text
subtype (1 B) | level (1 B) | category_index (VLQ) | format bytes
```

Subtype 0 identifies a UTF-8 format; subtype 2 identifies a UTF-16 format stored as UTF-Mixed, discussed in [section 6](#6-utf-16-balancing-encoding-work-and-output-size). The format occupies the rest of the body, so it needs neither a separate string length nor a terminating zero.

Format template indices are implicit: the first definition is index 0, the second is index 1, and so on. Writer and decoder assign indices in the same order. The decoder can then access templates by array index.

A thread template has a different layout:

```text
subtype=1 (1 B) | thread_template_index (VLQ) | thread_id (VLQ) | thread_name
```

Its index is explicit because a later run may redefine thread information. An unencrypted file can be reopened and appended to, but the previous run's thread IDs and names cannot simply be reused. A new thread template can replace the definition at an index; the decoder updates its mapping as it reads, and subsequent records use that definition.

**A file template index and a slot in the writer's cache are different things.** The cache may evict a mapping. If that format appears again, the writer can emit another template with a new file index. Existing records keep referring to the old index. Eviction affects future reuse and file size.

The format hash serves the writer's lookup. The decoder uses file indices, so the hash is not stored in the file.

## 5. Compress the fields that change

After extracting the templates, a record body contains:

```text
time delta | format template index | thread template index | arg0 | arg1 | ...
```

Template indices usually begin small, making them suitable for VLQ. Timestamps and arguments have their own encodings.

### 5.1 Large timestamps, small deltas

An epoch timestamp in milliseconds needs a 64-bit representation, or eight bytes per record. Consecutive records are often only a few milliseconds apart, and several may share the same millisecond. Once the previous timestamp is known, the next record can store the difference.

For timestamps 1000, 1002 and 1001 ms, the deltas are 1000, 2 and -1. The first record uses zero as its baseline; subsequent records use the preceding record.

How does the third record come to precede the second? When threads interleave, the order in which records are consumed can differ from the order in which their timestamps were taken; that is part 2's subject. Before encoding, the consumer clamps timestamps to be non-decreasing, which blocks such regressions at the cost of rewriting an out-of-order record's time to the previous value. Within one run, the deltas in the file do not go negative. Clamping cannot help in one case: when an unencrypted file is appended to, the time baseline is recovered from the old file, and a record from the new run that meets a system-clock rollback would precede that baseline. The format must allow negative deltas without letting them take up space.

ZigZag maps small signed values to small unsigned values:

```text
signed:    0  -1   1  -2   2  -3   3
unsigned:  0   1   2   3   4   5   6
```

VLQ then encodes the result. Both +1 and -1 remain small, avoiding the many high bits set in a negative two's-complement value.

### 5.2 Preserve numeric values without formatting them

Each argument starts with a type byte that tells the decoder how to read its value:

| Argument | Content after the type byte |
|---|---|
| null | No content |
| pointer | 8 bytes |
| bool, char, int8, uint8 | 1 byte |
| char16, char32, uint16, uint32, uint64 | Unsigned VLQ |
| int16, int32, int64 | ZigZag + VLQ |
| float / double | Original 4 / 8 bytes |
| UTF-8 string | Byte length as VLQ + content |
| UTF-16 string | Length and content after conversion to UTF-Mixed |

Floating-point values retain their four- or eight-byte binary representation. Their bit patterns behave differently from integers, and producing decimal text also involves formatting precision. That conversion can wait until reading.

Types such as bool and int8 already occupy one byte; variable-length encoding would not make them smaller.

Records also omit the argument count. After reading each typed argument, the decoder checks whether it has reached the end of the item.

## 6. UTF-16: balancing encoding work and output size

C#, Java and Unreal often use UTF-16. An ASCII character has a zero high byte, so storing an English letter in two bytes wastes space. Converting every UTF-16 string to UTF-8, however, requires more decisions for Chinese characters, surrogate pairs and mixed text. Some characters even take more bytes in UTF-8.

UTF-Mixed narrows the initial ASCII portion to one byte per character. When the fast conversion stops, it writes an `FF` marker and copies the remaining UTF-16 code units as a suffix.

![Original and encoded bytes of UTF-Mixed](img/performance/compressed-utf-mixed.png)

<p align="center"><em>Figure 3: with the scalar split shown here, abc中x shrinks from 10 UTF-16 bytes to 8 bytes. The final x stays in the UTF-16 suffix.</em></p>

That suffix includes any later ASCII characters. The writer does not keep switching back to save individual bytes. A SIMD (single-instruction-multiple-data) implementation may stop before a whole block containing a non-ASCII character, so valid split positions can differ.

The example would occupy only seven bytes in UTF-8, compared with eight in UTF-Mixed. BqLog accepts that extra byte to reduce conversion work: the suffix can be copied in bulk without converting each character.

Within the known string length, the decoder finds `FF`, copies the prefix and converts the suffix to UTF-8. The marker switches encodings; it does not terminate the string. Part 3 explains the SIMD implementation of the ASCII conversion.

## 7. Why the same record has different memory and file layouts

So far we have looked only at the file format. Now put it back into the full pipeline: BqLog is an asynchronous log. Business threads (producers) do not write files directly; they put records into an in-memory queue, the bus. A background thread (the consumer) takes them out and hands them to an Appender, the component that owns a particular output, for encoding and writing. That channel is part 2's subject.

With such a compact file format, why does the producer not generate it directly?

The Appender assigns file template indices, and different outputs may have different template sets. Encoding file records on producers would require access to those shared dictionaries. Variable-length fields would also make locating arguments in memory more expensive.

BqLog therefore keeps a fixed header and suitable alignment in bus records, then lets the consumer produce the compact file representation. Consider:

```cpp
log.info("hp={}", int32_t(42));
```

![The same record in memory and in a seven-byte file item](img/performance/compressed-record-example.png)

<p align="center"><em>Figure 4: format hp={}, thread name Main. The file representation assumes existing templates and a zero time delta.</em></p>

The memory header is 40 bytes, holding the full timestamp, category, thread ID, format hash and lengths. The five format bytes occupy eight bytes after four-byte alignment. The int32 argument has a type area and a value area, followed by thread extension information. This internal record totals 61 bytes. The LP path adds a context and a ring chunk header — LP is the low-frequency path introduced in part 2, where they are the main subject.

The file item can be:

```text
80 85 | 80 | 80 | 80 | 0B D4
type/len5  Δt=0 fmt0 thread0 int32(42)
```

`0B` identifies int32. ZigZag maps 42 to 84, encoded as `D4`. The whole item occupies seven bytes. File headers and templates are shared across records. The memory layout supports efficient production and consumption; the file layout reduces stored bytes.

## 8. Why the file needs segments

A restart loses the template tables and encryption state that live in memory. Without append support, one crash of an encrypted log would lose its unfinished data, or reopening would mean decrypting and rewriting the whole old file; both cost far more than the segment structure.

**Appending compressed records requires the existing template indices and time baseline.** Seeking to the end of the file is only part of the operation.

### 8.1 Restore the state needed for appending

Suppose an old file defines format templates 0, 1 and 2. Reusing one requires finding its existing index; a new format must start at index 3. Since records store time deltas, the writer also needs the last timestamp.

Restarting numbering at zero without reading the file would make the decoder interpret new indices using old definitions, potentially selecting the wrong format.

For an unencrypted compressed file, the writer scans the existing items on open, restores format mappings, the next format index and the last timestamp, then appends. Thread information is rebuilt for the current run. UTF-16 formats are stored as UTF-Mixed, so scanning must also recover their original representation to reconstruct the lookup hash.

### 8.2 An encrypted writer has no private key

In encrypted mode, the client is configured with a public key. The private key is neither distributed with the client nor stored in the log file. The public key protects new encryption material but cannot decrypt old content.

The writer therefore cannot scan old templates, recover the last timestamp or retrieve old encryption material as it does for an unencrypted file.

It starts a fresh compressed stream with new templates, a new time baseline and new encryption material. This requires no private key on the writer and no decryption and rewriting of the old file.

Segments describe data ranges with their own headers and optional encryption material. The segment header tells the decoder how to decrypt that range; template indices belong to the compressed stream. For example, a later segment can still reference template 7 defined earlier, while using new encryption material.

Segments within one file can reuse the templates written before them; the encrypted-restart flow is a different scenario. In the encrypted restart path of 2.5.0, the new compressed stream goes into a newly numbered file. The template table and timestamp reset with it. Segments within an existing file connect recovered data to that file's stream.

![File, segment and header byte layouts](img/performance/compressed-file-v10.png)

<p align="center"><em>Figure 5: the three bars form one continuous file. S0, S1 and S2 are absolute file offsets.</em></p>

The file header occupies eight bytes: four for version, one for format and three for padding. The compressed format currently uses version=10 and format=2.

Each segment begins with a 12-byte header:

```text
next_seg_pos: 8 B | seg_type: 1 B | enc_type: 1 B | padding: 2 B
```

`next_seg_pos` is the absolute offset of the next segment, or UINT64_MAX for the last one. The decoder can determine the range without scanning every item. Restricting reads to that range also prevents crossing a segment header with the wrong encryption state.

`seg_type` distinguishes normal data, Appender cache recovery and data bus recovery. A whole range gets its source designation once. The first segment's payload also stores 52 bytes of metadata and the category definitions; later segments reuse that file-level information.

A segment header does not automatically clear the template dictionary. Recovery can thus give a batch fresh encryption material while retaining its references to earlier templates.

### 8.3 Append a segment, then link it

The writer checks the segment chain, writes the new segment header and encryption material at the end, then updates the previous last segment's `next_seg_pos`. The existing payload stays in place.

This resembles appending to a singly linked list, with file offsets taking the place of memory pointers. The offsets remain meaningful after reopening the file, regardless of virtual addresses.

Chain checks, header writes and the associated I/O happen at file creation, recovery or state transitions. Subsequent records share that setup cost.

### 8.4 Recover each representation where it belongs

At a crash, some logs may still be raw records in the bus. Others may already be compressed items in the Appender's write cache, waiting to finish reaching the file. This cache is backed by a memory-mapped file (mmap), so its contents can still be inspected after a crash; [section 7.3](<Article 2_Why is BqLog so fast - From Ring Buffer to Adaptive Data Bus.MD#73-recovery-after-a-crash>) of part 2 builds on this property.

Suppose the old file defines format template 7, and a cached record references it. That record's time delta also uses the preceding record as its baseline. These bytes belong to the old stream and must retain those meanings.

Recovery appends a segment to the verified original file, prepares new encryption material and writes the complete cached items. The decoder has the private key and learns template 7 and the time baseline from the old segments. At the new segment, it switches encryption material and continues. The writer does not need to decrypt the old file or re-encode the cached records.

A bus record still contains the original format and arguments. The same order log here has no dependency on file template 7 yet. On an encrypted restart, it can enter a new file and receive a new template index starting at zero. The consumer processes recovered records before records from the current run.

![Preserving old template indices or building a new stream during recovery](img/performance/recovery-streams.png)

<p align="center"><em>Figure 6: cached items referencing template 7 return to the old file; raw bus records can be re-encoded with template 0 in a new file.</em></p>

Segments preserve encoding work already completed. The template state to use depends on whether the log stopped in the bus or in the Appender cache.

## 9. Pay encryption setup costs per segment

Running a public-key operation for every log would multiply its fixed cost by the record count. Copying an entire buffer before encrypting it would add another read and write of the payload.

The current implementation separates preparation from payload processing:

1. At segment creation, generate an AES-256 key and IV (initialization vector), protect the key with RSA-2048, and encrypt a 32 KiB mask with AES-CBC.
2. During writing, XOR the payload in the Appender's batch buffer with the repeating mask, indexed by file position.

![Per-segment preparation and per-batch payload processing](img/performance/encryption-segments.png)

<p align="center"><em>Figure 7: the upper section runs once per segment; the lower section transforms each payload batch in place.</em></p>

Encryption material occupies 33048 bytes: an eight-byte public-key fingerprint, 256 bytes of RSA ciphertext, a 16-byte IV and 32768 bytes of encrypted mask. For a 1 KiB payload, this fixed material exceeds the payload itself. With tens of MiB per segment, it is shared across much more data. Segment creation frequency therefore matters alongside processing cost per byte.

The payload uses absolute file offsets:

```text
mask_index = file_offset & (32768 - 1)
data[i] ^= mask[(file_offset + i) & (32768 - 1)]
```

Since 32768 is a power of two, a bitwise AND implements the modulo. Each batch derives its position independently. SSE, AVX2 or NEON can load data and mask blocks, XOR them and store the result.

The Appender adjusts write-cache padding to keep the data address and mask position relatively aligned. After an incomplete write, the unwritten cache portion must be restored to a state that can be processed again; the next flush must not mistake transformed bytes for plaintext.

The main work of per-batch processing is the XOR loop. The protection level is limited: this is an obfuscation layer that stops the logs from being read directly. The payload does not go through AES batch by batch; it is XORed with an AES-protected mask that repeats every 32 KiB, so known plaintext can reveal a mask segment. The format has no AEAD (authenticated encryption) tag: it does not detect tampering and cannot stop a motivated attacker. Applications needing those guarantees must add a mechanism.

## 10. Where writing and reading do their work

A record reaching the compressed Appender follows this path:

```text
format hash, level, category
  → look up format template; emit a definition on a miss
  → look up thread template; emit a definition on a miss
  → encode time delta with ZigZag / VLQ
  → encode template indices and arguments
  → backfill item length
  → mark the record complete in the write cache
```

UTF-Mixed and variable-length integers get their final sizes during encoding. Reserving space, writing the body and backfilling the length avoids a separate exact-measurement pass. Part 3 examines this step.

Reading reverses the process: open the container, restore segment state and load categories; append format definitions to an array and update thread mappings; reconstruct record times and arguments, then use the common layout code to produce text. The frequent write path no longer builds a complete text line for every call.

The reader takes on the corresponding costs. Binary files require a decoder, and starting in the middle may leave the dictionary or time baseline unavailable. The writer's cache can evict entries, but the file's template history keeps growing; the decoder needs that history. File rotation affects both disk use and the cost of opening and decoding logs.

Looking back, the format keeps doing one thing: it finds the repeated work that no log record can avoid and moves it to where it happens rarely. Formatting moved to read time, encryption preparation to segment creation, and state reconstruction to opening the file.

## 11. Measure the complete file and write path

In the public README's four-million-record size test, BqLog text output is 283 MB and compressed output is 45 MB, less than one sixth of the text size. The ratio depends on the data. Unique formats or large, high-entropy arguments reduce template reuse.

Write benchmarks also need their context: thread count, final flush, segment initialization, and whether the output is text or compressed. The design removes identifiable work; measurements show how much that matters for a particular workload. The benchmark table at the start of part 2 gives one set of measurements of the time saved on the write path.

Part 2 returns to memory: how multiple producers deliver these records to the consumer.

## Continue with the source

- [appender_file_compressed.cpp](../src/bq_log/log/appender/appender_file_compressed.cpp): templates, time deltas, argument encoding and item backfill.
- [appender_file_binary.h](../src/bq_log/log/appender/appender_file_binary.h), [appender_file_binary.cpp](../src/bq_log/log/appender/appender_file_binary.cpp): file header, segments, encryption material and recovery.
- [appender_decoder_compressed.cpp](../src/bq_log/log/decoder/appender_decoder_compressed.cpp), [appender_decoder_base.cpp](../src/bq_log/log/decoder/appender_decoder_base.cpp): the corresponding decode process.
- [log_utils.h](../src/bq_log/utils/log_utils.h), [util.cpp](../src/bq_common/utils/util.cpp): VLQ, ZigZag and UTF-Mixed.
- [test_log_appender.h](../test/cpp/test_log_appender.h), [test_compressed_cache.h](../test/cpp/test_compressed_cache.h): tests for segmentation, recovery and template cache.
