# Development Guidelines

[简体中文](DEVELOPMENT_GUIDELINES_CHS.md)

These rules keep BqLog consistent across platforms, compilers and languages. They apply to `src/`, `include/`, `wrapper/`, `plugin/`, `tools/`, the build scripts and the tests; the exceptions are written out where they exist.

## 0. Comments

- Keep them slim: a good name and a clear structure beat prose. Comment only what the code cannot say (a decision, an invariant, a platform quirk), not what the next line does.
- Comments are in English, one or two lines.
- No commented-out code and no change history in comments.

## 1. Code structure

Clarity, elegance and modularity come first: code is read far more often than it is written. One subject per file and per class, a narrow interface that keeps its state to itself, and a one-way layer direction (`bq_common` → `bq_log` → wrapper / plugin / tools).

## 2. Naming

- `snake_case` for files, types, functions, variables, enum values and members: `meta_registry`, `enable_meta_records`, `run_version()`, `log_level::info`.
- Macros are `BQ_` + `UPPER_CASE` (see 5). Template parameters are upper case (`STR`, `CAT_INDEX`, `T`).
- No Hungarian prefixes, no `p_`/`m_`, no invented abbreviations.
- A boolean reads like a statement (`is_valid()`, `need_recovery`, `meta_records_enabled_`).

## 3. Members end with `_`

- Class and struct data members end with an underscore: `log_id_`, `category_level_words_`, `meta_records_segments_`.
- Exceptions: members of exported structs and C-compatible types keep plain names, because the name is part of an ABI or a public header (`_api_string_def::str`, `_log_entry_head_def` fields).

## 4. Exported ABI and memory layout

Anything that crosses a binary/library boundary — the C API, the language wrappers, plugin ABI and every file format — must have a layout that is fixed at compile time and identical on every supported compiler:

- Fixed-width types only (`uint32_t`, `int64_t`), never `long`/`size_t`/`bool`/`unsigned` in a structure that crosses a boundary; no bitfields (use explicit fields and masks, the layout of a bitfield is compiler-dependent); enums that cross a boundary carry an explicit underlying type.
- `BQ_PACK_BEGIN` / `BQ_PACK_END` when exact packing is part of the format, `alignas(n)` when alignment is.
- A `static_assert` on `sizeof` and on the offset of the interesting fields sits next to the definition, so an accidental layout change fails to compile: see `meta_record_head`, `meta_record_entry_head` in `src/bq_log/types/meta/meta_registry.h`, and `include/bq_log/misc/bq_log_c_types.h`.
- A field that may be unaligned inside a packed structure is read and written with `memcpy`, not by dereferencing a cast pointer.
- A file format carries a magic number, a `format_version` and reserved space; a new field must not change the meaning of an existing one. A reader validates size, version and checksum before using the data.
- Changing an exported layout or a file format is a compatibility break: it is stated in `CHANGELOG.md` and in the version number (the 2.7.0 note about the `bq::log` handle and the C API is the example).

## 5. Macros and platform detection

- Macros live in `include/bq_common/platform/macros.h` and start with `BQ_`. A macro that configures one component may live in that component's header (`BQ_LOG_META_SEGMENT_SIZE`), still `BQ_`-prefixed.
- Platform, compiler and architecture detection goes through the macros defined in `macros.h` — `BQ_WIN`/`BQ_WIN64`, `BQ_APPLE`/`BQ_MAC`/`BQ_IOS`, `BQ_LINUX`, `BQ_UNIX`, `BQ_POSIX`, `BQ_ANDROID`, `BQ_OHOS`, `BQ_PS`, `BQ_MSVC`/`BQ_GCC`/`BQ_CLANG`, `BQ_ARM*`/`BQ_X86*`. Do not test raw compiler macros (`_WIN32`, `__APPLE__`, `__sun`, `_MSC_VER`) and do not define a platform macro of your own.
- The only exception is a feature macro that must be defined before the first system header is included (`_GNU_SOURCE` at the top of `src/bq_common/platform/posix_misc.cpp`); keep such a case in one place.
- Test for the availability of a feature, not for a platform, whenever possible: `#if defined(F_OFD_SETLK)`, then `#if defined(LOCK_EX) && defined(LOCK_NB)`, as in `try_lock_instance_file`.
- Use inline functions and templates instead of function-like macros wherever the language allows it.

## 6. No C++ standard library in the library

- `src/` and `include/` must not use the C++ standard library at all, because the library has to build in none-STL environments: no `<vector>`, `<string>`, `<map>`, `<algorithm>`, `<memory>`, `<thread>`, `<atomic>`, no `std::` container, algorithm or type alias.
- Such an environment has no libc++/libstdc++ at all. Android builds one with `-DANDROID_STL=none`, which turns on `BQ_NO_LIBCPP` in `src/CMakeLists.txt`; embedded and console toolchains are the same case.
- Use the base library instead: `bq::array`, `bq::string`, `bq::hash_map`, `bq::unique_ptr`, `bq::atomic`, `bq::thread`, the traits in `include/bq_common/types/type_traits.h`, and the pieces in `src/bq_common/platform/no_lib_cpp_impl.h` (`operator new`, `__cxa_guard_*`, …).
- The C standard library and the platform APIs are fine: `<stdint.h>`, `<string.h>`, `<stdio.h>`, `<time.h>`, pthreads, Win32, JNI and so on.
- Tests, benchmarks, demos and tools are exempt — they are not part of the shipped binary.

## 7. Module boundaries

- `bq_common` is the foundation library: containers, platform abstraction, memory map, files, encryption, property/config. It knows nothing about logging, and logging code must not leak into it.
- `bq_log` includes `bq_common` through `bq_common/bq_common.h` (source/static integration) or `bq_common/bq_common_public_include.h` (the public include used when the library is linked). Do not include an inner `bq_common/...` header from logging code.
- `bq_common` is normally integrated as source into the module that needs it; a new file must be picked up by the module's own `CMakeLists.txt` (`COLLECT_SRC_FILES` in `CMake_utils.txt`) and must not require a change in somebody else's build.
- Outside this repository the only supported C++ entry point is `bq_log/bq_log.h`; everything else under `include/bq_log/` is internal and may change. Other languages use the published wrapper (`wrapper/python`, `wrapper/java`, `wrapper/go`, `wrapper/typescript`, `wrapper/csharp`), which is their API and their import.

## 8. Reuse and one subject per file

- Search before writing: containers, hashes, string helpers, file helpers, platform calls and type traits already exist in `bq_common` and must not be re-implemented inside a module.
- Each file owns one subject; new code belongs to the file that already owns it. All SIMD helpers live in `src/bq_common/platform/simd.h`, all platform macros in `macros.h`, the atomic wrappers in `bq_common/platform/atomic/`, the configuration parser in `bq_common/utils/property*`.
- Platform-specific code is one file per platform behind one interface (`win64_misc.cpp`, `posix_misc.cpp`, `android_misc.cpp`, `java_misc.cpp`), not `#if` branches spread over generic code.
- Prefer a small, single-purpose helper over a flag with two behaviours.

## 9. New features need tests

- A feature and a bug fix come with coverage in the same commit:
  - `test/cpp/` for the C++ library, registered in `test/cpp/main.cpp` with `TEST_GROUP_BEGIN`/`TEST_GROUP`/`TEST_GROUP_END` (the class itself lives in a `test_*.h`); `test/cpp/bq_common_test/` for `bq_common`;
  - `test/multi_instance/` (CTest cases `multi_instance_*`, one per `CASES` entry in `run.py`: `instance_assignment`, `two_logs`, `recovery_off`, `crash_recovery`, `two_instances`, `legacy_coexistence`, `dual_runtime_plugins`, `child_lock`, `flushed_recovery`) for anything multi-process, multi-instance or crash recovery;
  - the wrapper test suite (`test/python`, `test/java`, `test/go`, `test/typescript`, `test/csharp`) for a wrapper change.
- The tests must pass in every C++ standard the CI builds (11, 14, 17, 20, 23) and on every platform it builds; code behind `BQ_CPP_20` (the `fast_*` interfaces) needs a test that runs in C++20 as well.
- Tests are deterministic, clean up after themselves, and do not depend on the order they run in or on the machine being fast.

## 10. Multi-language wrappers stay consistent

- The wrappers expose the same API and the same behaviour; the only allowed differences are the ones the language forces (Go exported names must start with an upper-case letter, Java/Kotlin follow their package and camelCase conventions, Python uses keyword arguments).
- Same names, same arguments, same defaults, same semantics — including the log name/instance/index rules, and one shared `Log` object per process across languages.
- A new API or configuration item is added to every wrapper, or the gap is documented in `docs/API_REFERENCE.md`.

## 11. Commits must pass CI

- A commit lands only when the repository workflows pass: `AutoTest` (`.github/workflows/test.yml`, every platform and every C++ standard) and `Build`.
- Run the matching local script (`build/test/...`, `build/lib/...`) before pushing.

## 12. License header

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
- A new file is added to a build target (and to `format_code.sh`) in the same commit.

## 13. Performance

- Hot-path code (the log call, the buffer claim, the fast path, the appender, the clock) is written for the lowest instruction and branch count.
- A change that touches a hot path must not regress the benchmark (`benchmark/cpp`, see `docs/BENCHMARK.md`).

## 14. Formatting and tooling

- Run `format_code.sh` (or `format_code.bat`) before committing; the style is the `.clang-format` in the repository root.
- Build scripts and CI live in `build/` and `.github/workflows/`; a new platform or a new test target is added there, not in an ad-hoc local script.
- Text that ships to users (README, CHANGELOG, docs) has an English and a `_CHS` version; keep both in sync in the same commit.

