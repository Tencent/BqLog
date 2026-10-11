# Development Guidelines

[简体中文](DEVELOPMENT_GUIDELINES_CHS.md)

These rules keep BqLog consistent across platforms, compilers and languages, and keep the log call fast. They apply to `src/`, `include/`, `wrapper/`, `plugin/`, `tools/`, the build scripts and the tests; the few exceptions are written out where they exist. If a rule has to be broken, say so in the pull request and explain why.

## 0. Comments

- Code first. A clear name and an obvious structure are worth more than a paragraph of prose.
- Comment only what the code cannot say: why a decision was taken, an invariant the reader cannot see, a platform quirk. Do not narrate what the next line does.
- Comments are in English, one or two lines.
- No commented-out code and no change history in comments; git already has both.
- Delete a comment that the code below already states, and never fix a confusing piece of code by adding a comment.

## 1. Naming

- `snake_case` for files, types, functions, variables, enum values and members: `meta_registry`, `enable_meta_records`, `run_version()`, `log_level::info`.
- Macros are `BQ_` + `UPPER_CASE` (see 4). Template parameters are upper case (`STR`, `CAT_INDEX`, `T`).
- No Hungarian prefixes, no `p_`/`m_`, no invented abbreviations. A name says what the thing is, not how it is implemented.
- A boolean reads like a statement (`is_valid()`, `need_recovery`, `meta_records_enabled_`).

## 2. Members end with `_`

- Class and struct data members end with an underscore: `log_id_`, `category_level_words_`, `meta_records_segments_`.
- Exceptions: members of exported structs and C-compatible types keep plain names, because the name is part of an ABI or a public header (`_api_string_def::str`, `_log_entry_head_def` fields).

## 3. Exported ABI and memory layout

Anything that crosses a binary/library boundary — the C API, the language wrappers, plugin ABI and every file format — must have a layout that is fixed at compile time and identical on every supported compiler:

- Fixed-width types only (`uint32_t`, `int64_t`), never `long`/`size_t`/`bool`/`unsigned` in a structure that crosses a boundary; no bitfields (use explicit fields and masks, the layout of a bitfield is compiler-dependent); enums that cross a boundary carry an explicit underlying type.
- `BQ_PACK_BEGIN` / `BQ_PACK_END` when exact packing is part of the format, `alignas(n)` when alignment is.
- A `static_assert` on `sizeof` and on the offset of the interesting fields sits next to the definition, so an accidental layout change fails to compile: see `meta_record_head`, `meta_record_entry_head` in `src/bq_log/types/meta/meta_registry.h`, and `include/bq_log/misc/bq_log_c_types.h`.
- A field that may be unaligned inside a packed structure is read and written with `memcpy`, not by dereferencing a cast pointer.
- A file format carries a magic number, a `format_version` and reserved space; a new field must not change the meaning of an existing one, and it is read defensively (the reader validates size, version and checksum before trusting a byte).
- Changing an exported layout or a file format is a compatibility break: it is stated in `CHANGELOG.md` and in the version number (the 2.7.0 note about the `bq::log` handle and the C API is the example).

## 4. Macros and platform detection

- Macros live in `include/bq_common/platform/macros.h` and start with `BQ_`. A macro that configures one component may live in that component's header (`BQ_LOG_META_SEGMENT_SIZE`), still `BQ_`-prefixed.
- Platform, compiler and architecture detection goes through the macros defined in `macros.h` — `BQ_WIN`/`BQ_WIN64`, `BQ_APPLE`/`BQ_MAC`/`BQ_IOS`, `BQ_LINUX`, `BQ_UNIX`, `BQ_POSIX`, `BQ_ANDROID`, `BQ_OHOS`, `BQ_PS`, `BQ_MSVC`/`BQ_GCC`/`BQ_CLANG`, `BQ_ARM*`/`BQ_X86*`. Do not test raw compiler macros (`_WIN32`, `__APPLE__`, `__sun`, `_MSC_VER`) and do not define a platform macro of your own.
- The only exception is a feature macro that must be defined before the first system header is included (`_GNU_SOURCE` at the top of `src/bq_common/platform/posix_misc.cpp`). Keep such a case in one place, with the reason next to it.
- Test for the availability of a feature, not for a platform, whenever that is possible: `#if defined(F_OFD_SETLK)` followed by `#if defined(LOCK_EX) && defined(LOCK_NB)` in `try_lock_instance_file` is correct on a platform we have never seen.
- Use inline functions and templates instead of function-like macros wherever the language allows it.

## 5. No C++ standard library in the library

- `src/` and `include/` must not use the C++ standard library: no `<vector>`, `<string>`, `<map>`, `<algorithm>`, `<memory>`, `<thread>`, `<atomic>`, `<memory>` and no `std::` container, algorithm or type alias.
- The reason is size and portability: the same sources are built with `BQ_NO_LIBCPP` (set by `src/CMakeLists.txt` when the Android build uses `-DANDROID_STL=none`) so that the shipped binary stays small — the Android library is about 200 KB — and so the code also builds on toolchains where libc++ is not available.
- Use the base library instead: `bq::array`, `bq::string`, `bq::hash_map`, `bq::unique_ptr`, `bq::atomic`, `bq::thread`, the traits in `include/bq_common/types/type_traits.h`, and the pieces in `src/bq_common/platform/no_lib_cpp_impl.h` (`operator new`, `__cxa_guard_*`, …).
- The C standard library and the platform APIs are fine: `<stdint.h>`, `<string.h>`, `<stdio.h>`, `<time.h>`, pthreads, Win32, JNI and so on.
- Tests, benchmarks, demos and tools are exempt — they are not part of the shipped binary.

## 6. Module boundaries

- `bq_common` is the foundation library: containers, platform abstraction, memory map, files, encryption, property/config. It knows nothing about logging, and logging code must not leak into it.
- `bq_log` includes `bq_common` through `bq_common/bq_common.h` (source/static integration) or `bq_common/bq_common_public_include.h` (the public include used when the library is linked). Do not include an inner `bq_common/...` header from logging code.
- `bq_common` is normally integrated as source into the module that needs it; a new file must be picked up by the module's own `CMakeLists.txt` (`COLLECT_SRC_FILES` in `CMake_utils.txt`) and must not require a change in somebody else's build.
- Outside this repository the only supported C++ entry point is `bq_log/bq_log.h`; everything else under `include/bq_log/` is internal and may change. Other languages use the published wrapper (`wrapper/python`, `wrapper/java`, `wrapper/go`, `wrapper/typescript`, `wrapper/csharp`), which is their API and their import.

## 7. Reuse and one subject per file

- Search before writing: containers, hashes, string helpers, file helpers, platform calls and type traits already exist in `bq_common` and must not be re-implemented inside a module.
- Each file owns one subject; new code belongs to the file that already owns it. All SIMD helpers live in `src/bq_common/platform/simd.h`, all platform macros in `macros.h`, the atomic wrappers in `bq_common/platform/atomic/`, the configuration parser in `bq_common/utils/property*`.
- Platform-specific code is one file per platform behind one interface (`win64_misc.cpp`, `posix_misc.cpp`, `android_misc.cpp`, `java_misc.cpp`), not `#if` branches spread over generic code.
- Prefer a small, single-purpose helper over a flag with two behaviours.

## 8. New features need tests

- A feature and a bug fix come with coverage in the same commit:
  - `test/cpp/` for the C++ library, registered in `test/cpp/main.cpp` with `TEST_GROUP_BEGIN`/`TEST_GROUP`/`TEST_GROUP_END` (the class itself lives in a `test_*.h`); `test/cpp/bq_common_test/` for `bq_common`;
  - `test/multi_instance/` (CTest cases `M1`…`M9` in `run.py`) for anything multi-process, multi-instance or crash recovery;
  - the wrapper test suite (`test/python`, `test/java`, `test/go`, `test/typescript`, `test/csharp`) for a wrapper change.
- The tests must pass in every C++ standard the CI builds (11, 14, 17, 20, 23) and on every platform it builds; code behind `BQ_CPP_20` (the `fast_*` interfaces) needs a test that runs in C++20 as well.
- Tests are deterministic, clean up after themselves, and do not depend on the order they run in or on the machine being fast.
- Size is part of the contract too: `test/symbol_size/` measures the shipped libraries.
- A test that is hard to write is usually telling you the interface is wrong.

## 9. Multi-language wrappers stay consistent

- The wrappers expose the same API and the same behaviour; the only allowed differences are the ones the language forces (Go exported names must start with an upper-case letter, Java/Kotlin follow their package and camelCase conventions, Python uses keyword arguments).
- Same names, same arguments, same defaults, same semantics — including the log name/instance/index rules, and one shared `Log` object per process across languages.
- A new API or configuration item is added to every wrapper, or the gap is documented in `docs/API_REFERENCE.md`.

## 10. Commits must pass CI

- A commit lands only when the repository workflows pass: `AutoTest` (`.github/workflows/test.yml`, every platform and every C++ standard) and `Build`.
- Run the matching script locally first (`build/test/...`, `build/lib/...`), it is much faster than the CI round trip.
- Commit messages are short and say what changed (`Category log wrapper: build in C++11 and C++14 again`); no emoji, no tool/AI mentions, no co-author trailers.
- One commit, one thing: do not mix a rename with a behaviour change, and do not mix a fix with a refactor.

## 11. License header

- Every source file starts with the license header, copied from an existing file:
  ```c
  /* Copyright (C) 2026 Tencent.
   * BQLOG is licensed under the Apache License, Version 2.0.
   * You may obtain a copy of the License at
   *
   *     http://www.apache.org/licenses/LICENSE-2.0
   *
   * Unless required by applicable law or agreed to in writing, software
   * distributed under the License is distributed on an "AS IS" BASIS,
   * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   */
  ```
- A new file is added to a build target (and to `format_code.sh`) in the same commit; a file that no build compiles is dead code.

## 12. Performance

- Hot-path code (the log call, the buffer claim, the fast path, the appender, the clock) is written for the lowest instruction and branch count: do compile-time work at compile time, do not allocate, do not lock where an atomic is enough, keep the data the call site needs in one cache line, use `bq_forceinline` where it measurably helps.
- A change that touches a hot path must not regress the benchmark: run `benchmark/cpp` before and after (methodology and reference numbers in `docs/BENCHMARK.md`) and put the numbers in the pull request.
- Prefer the cheapest correct primitive: the `fetch_add` claim with an explicit rollback that replaced the CAS retry loop, or a disabled level that costs one load and one bit test.
- Slow paths (startup, recovery, decoding, configuration) are allowed to be readable rather than clever.

## 13. Formatting and tooling

- Formatting is not a matter of taste: run `format_code.sh` (or `format_code.bat`) before committing; the style is the `.clang-format` in the repository root.
- Build scripts and CI live in `build/` and `.github/workflows/`; a new platform or a new test target is added there, not in an ad-hoc local script.
- Text that ships to users (README, CHANGELOG, docs) has an English and a `_CHS` version; keep both in sync in the same commit.

## Review checklist

- No new comment that only repeats the code; the comments that remain are in English.
- Names are `snake_case`, members end with `_`, macros are `BQ_` and live in `macros.h`.
- No new raw platform macro; no new C++ standard library use in `src/`+`include/`.
- Anything crossing a boundary has pinned layout plus `static_assert`s; a format or ABI change is called out in the CHANGELOG.
- Tests cover the new behaviour and pass in C++11/14/17/20/23.
- The benchmark did not get worse if a hot path was touched.
- The commit passes `AutoTest` and `Build`; the message says what changed.
