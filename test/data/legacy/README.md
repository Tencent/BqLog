# Legacy format samples (v9, v10)

Real log files written by old releases, with the decoded text those releases' own
`BqLog_LogDecoder` produced for them. `test_legacy_decode` reads every sample with the current
standalone decoder and compares byte for byte, so "the standalone tool still decodes the old
formats" is a test result rather than a claim.

| | written by | format | encryption |
|---|---|---|---|
| `v9/` | Release_2.3.1 (`eafdb24b`) | 9 | plaintext and `rsa_aes_xor` |
| `v10/` | Release_2.7.3 (`86e52321`), one sample from Release_2.6.0 | 10 | plaintext and `rsa_aes_xor` |

Each directory has a `manifest.json` recording the source tag and commit, the build command, the
segment-type sequence, and the SHA-256 of both the sample and its golden output.

The largest sample here is 107 KB and `make_samples.py` refuses to write one above 200 KB, so this
directory stays in the repository as plain files. Keep it that way: nothing here should ever be big
enough to need Git LFS.

## The key

`legacy_key.pem` / `legacy_pub.txt` are the RSA-2048 pair the unit tests already carry. They exist
so these files can be regenerated and decoded in CI, and they protect nothing: every sample here is
a file that was deliberately made public as part of this repository.

## How the samples were made

1. `git worktree add <dir> <tag>`, then copy `legacy_gen_main.cpp` to `<dir>/tools/legacy_gen/main.cpp`
   next to a copy of `<dir>/tools/log_decoder/CMakeLists.txt` with the project renamed to
   `BqLog_LegacySampleGen`. It uses only API that every tag in scope shares.
2. Build it (`cmake ... -DBQ_WERROR=OFF && make -j8`), and build `<tag>/tools/log_decoder` as well.
3. `make_samples.py` writes the samples, decodes each one with that tag's own decoder, and fills in
   the manifest:

   ```
   ./make_samples.py --tag Release_2.3.1 --commit eafdb24b \
       --gen <build>/BqLog_LegacySampleGen --decoder <build>/BqLog_LogDecoder \
       --out test/data/legacy --version 9 --build-command "..." \
       --delay-lo 4000 --delay-mid 20000 --delay-hi 55000
   ```

   The delay arguments select where in the write/flush cycle the generator is killed. A crash with
   entries still queued in the log buffer produces `recovery_by_log_buffer`; a crash with entries
   the worker has moved into the appender's write cache but not flushed produces
   `recovery_by_appender`. The two are searched separately because one run rarely shows both.

Every sample is written with `TZ=Asia/Shanghai`, four categories (`Default`, `CategoryA`,
`中文类别`, `CategoryB`), one UTF-8 and one UTF-16 format template, and a record sequence whose
index appears in the text, so the decoded output is both readable and checkable.

## What these samples already established

- A clean run never produces a single-segment file: the first segment is created when the file is
  opened and carries the payload metadata, and the first log item opens another one. The manifests
  record the sequences that were actually produced rather than the ones a format description would
  suggest.
- The record layer did not change between v9 and v10. The same record sequence written by 2.3.1 and
  by 2.7.3 decodes to identical text once timestamps, thread ids and pointer values are normalised
  (`v9/enc.expected.txt` vs `v10/enc.expected.txt`). The source of `appender_file_compressed.cpp`
  and `layout.cpp` differs a lot between the two tags, but the bytes and their meaning do not.
- Neither release decodes the other's files: `BqLog_LogDecoder` 2.7.3 rejects a v9 file with
  "unsupported binary log format version, tools version:10, log file version:9", and 2.3.1 does the
  same in the other direction. The tool shipped with 2.8.0 is the first one that reads all three.

## Not produced

`Release_2.1.0` and `Release_2.2.9` are not in this repository (the oldest tag is `Release_2.3.0`),
so the v9 samples come from 2.3.1 only. Both write format 9; the samples still exercise the version
gate, the segment chain, and both encryption heads.
