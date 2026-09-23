# Changelog

[简体中文](CHANGELOG_CHS.md)

## [v2.5.0] - 2026-09-23
- **Performance**: Improved log-writing performance and reduced unnecessary worker wakeups and Java argument allocations.
- **Go support**: Added a Go wrapper with automatic argument conversion, category support, and source-module distribution.

## [v2.4.1] - 2026-07-26
- **Compatibility**: [Python wheel requires glibc 2.38+ #72](https://github.com/Tencent/BqLog/issues/72) — all release artifacts are now built on Ubuntu 22.04 (glibc 2.35), Windows Server 2022, and the lowest supported BSD releases (FreeBSD 13.2/13.5, OpenBSD 7.7, NetBSD 10.1, DragonFlyBSD 6.4.2, OmniOS r151054), so older Linux distros and BSD systems work out of the box.
- **Build/CI**: BSD build-time dependencies are now installed from self-hosted, immutable dependency snapshots instead of upstream package repos (which delete packages for EOL releases), making the release pipeline reproducible.

## [v2.4.0] - 2026-07-21
- **Performance — Raising the bar once again**: Typical workloads improve by approximately **10%–20%**; actual gains vary with thread count, log format, argument types, and output mode.
- **Bug fix**: [Slight memory leak #70](https://github.com/Tencent/BqLog/issues/70) — internal caches of the compressed file Appender are now strictly bounded, completely resolving the issue where memory usage could keep growing in certain cases.
- **Bug fix**: Fixed parsing of `log.buffer_policy_when_full`; the configured `discard`, `block`, or `expand` policy is now applied correctly instead of silently falling back to the default `block` policy.
- **Configuration**: Added `write_cache_size` for file Appenders, allowing the per-Appender write cache to be tuned from 64 KiB to 4 MiB.
- **Configuration**: Added `format_template_cache_max_entries` and `thread_info_cache_max_entries` for compressed file Appenders. Defaults remain `100000` and `2048`; exceeding the configured entry limit keeps memory bounded but may emit repeated templates and increase compressed file size.
- **Robustness**: Simplified the compressed-appender lookup cache (removed the redundant hot sub-table) and hardened its slot hashing with a murmur3-style full-avalanche finalizer, so the known structured key families (4-aligned thread ids, high-bits-only entropy, XOR/multiplier-cancelling keys) can no longer collapse the table into a single probe chain.

## [v2.3.2] - 2026-07-12
- **Bug fix**: [Embedded null bytes were dropped in decoder output #69](https://github.com/Tencent/BqLog/issues/69) — the log decoder now preserves embedded null bytes intact.
- **Bug fix**: Fixed decoding and recovery in the extreme case where different encryption modes are switched repeatedly within the same log file (mixed-encryption multi-segment), so such files now decode and recover correctly.
- **Robustness**: Hardened parsing of corrupted log files (guarding against unexpected-size memory allocations) and improved truncate validation and mmap allocation so disk-space errors surface before mapping access.
- **Docs**: Completed missing Java wrapper API comments (Javadoc).

## [v2.3.1] - 2026-06-16
- **Bug fix**: [Library crashes (SIGBUS) or hangs when the disk is full #67](https://github.com/Tencent/BqLog/issues/67)
- **Bug fix**: [\[C#\] \[\[Unity Editor\] fetch_and_remove_console_buffer occasionally crashes — delegate recycled by GC during native call #66](https://github.com/Tencent/BqLog/issues/66)
- **Unreal Engine**: Added UE 5.8 support — Fab, Prebuilt, and Source distributions now cover UE 5.0 – 5.8.
- **Unreal Engine 6**: Added Source and Prebuilt plugin packages for current UE6 development builds. UE6 packages are distributed through GitHub Releases only; Fab support is pending Epic's official UE6 availability.

## [v2.3.0] - 2026-06-03
- **OpenHarmony compatibility**: Replaced ES2020 `0n` BigInt literals with `BigInt(0)` in the TypeScript wrapper so the same `bqlog` package on ohpm works on **OpenHarmony 4.1+ (API 11+)** as well as HarmonyOS NEXT — no separate package needed.
- **Recovery hardening**: Strengthened data validation during recovery to detect malformed entries earlier and avoid potential crashes when consuming a corrupted ring buffer.

## [v2.2.9] - 2026-05-06
- **Compatibility**: Improved code compatibility with earlier Clang toolchains and frameworks such as MFC.
- **Compatibility**: Improved code compatibility across all supported Unreal Engine versions.

## [v2.2.8] - 2026-04-10
- **Code quality**: Refactored all SFINAE usages — moved `enable_if` from return types to template parameters for improved readability and consistency.
- **Bug fix**: [Crash when enable stack trace in C++ #62](https://github.com/Tencent/BqLog/issues/62)

## [v2.2.7] - 2026-04-01
- **Python 3.7+ support**: Added Python 3.7+ support via CPython C Extension wrapper (Stable ABI).
- **npm publishing**: Node.js wrapper is now published to npm as [`@pippocao/bqlog`](https://www.npmjs.com/package/@pippocao/bqlog), installable via `npm install @pippocao/bqlog`.
- **PyPI publishing**: Python wrapper is now published to PyPI as [`bqlog`](https://pypi.org/project/bqlog/), installable via `pip install bqlog`.
- **Python category log generator**: The `BqLog_CategoryLogGenerator` tool now generates Python category log wrappers (`.py`).
- **TypeScript dual-target generation**: The generator now produces separate TypeScript outputs for Node.js (`_nodejs.ts`, imports `@pippocao/bqlog`) and HarmonyOS (`_ohos.ts`, imports `bqlog`).
- **Category log test coverage**: Added category log test suites for Python, Java, C#, and TypeScript, covering category output, hierarchy, format parameters, and category mask filtering.
- **Package registry publishing**: BqLog is now published to npm (@pippocao/bqlog), PyPI (bqlog), Maven Central (com.tencent.bqlog), and OHPM (bqlog) — installable via npm install @pippocao/bqlog, pip install bqlog, Gradle/Maven dependency, and ohpm install bqlog respectively.

## [v2.1.2] - 2026-03-17
- **Bug fix**:  [Incorrect base_dir in GBox Sandbox environment on Android #61](https://github.com/Tencent/BqLog/issues/61)

## [v2.1.1] - 2026-02-25
- **Bug fix**: Fixed incorrect handling of unsigned long type on MSVC.  [无符号数输出为负数 #60](https://github.com/Tencent/BqLog/issues/60)

## [v2.1.0] - 2026-02-08

**BqLog 2.x is a brand-new major version, rebuilt from the ground up to deliver better performance, broader platform coverage, and more robust features.**

### 🌟 Major Changes from 1.x to 2.x
- **HarmonyOS support**: Added native HarmonyOS support, including ArkTS and C++.
- **Node.js support**: Added Node.js support (CJS and ESM).
- **Cross-platform improvements**: Improved cross-platform compatibility, stability and generality; supports more Unix systems.
- **Performance boost**: Average performance improved by ~80% for UTF-8, and by >500% for UTF-16 environments (C#, Unreal, Unity).
- **Android standalone C++**: Android no longer must be used together with Java.
- **Configuration changes**: Removed the `is_in_sandbox` config and replaced it with `base_dir_type`; added filters for snapshots and support for opening a new log file on each startup.
- **Hybrid asymmetric encryption**: Added high-performance hybrid asymmetric encryption with almost zero overhead.
- **Game engine plugins**: Provides Unity, Tuanjie Engine, and Unreal Engine plugins, making it easy to use in game engines; provides ConsoleAppender redirection to game-engine editors and Blueprint support for Unreal.
- **Binary distribution**: The repository no longer ships binaries. From 2.x on, please download platform- and language-specific packages from the [Releases page](https://github.com/Tencent/BqLog/releases).
- **Unlimited single log size**: The size of a single log entry is no longer limited by `log.buffer_size`.
- **Manual timezone**: The timezone can now be specified manually.
- **Deprecated raw_file appender**: The `raw_file` appender is deprecated and no longer maintained in 2.x; please use the `compressed_file` appender instead.
- **Recovery promoted to stable**: The Recovery feature's reliability has been improved and it has been promoted from experimental (beta) to stable (release).

---

## [v1.5.0] - 2025-09-22
- Changed the company entity of the open source License from "THL A29 Limited" to "Tencent"
- Android binary artifacts now support 16 KB page sizes
- Fixed a potential infinite loop issue when writing large-size logs
- **Bug fix**: [Help: After initializing on Android as per the documentation, is_valid always returns false, but logging is unaffected. The documentation mentions checking is_valid before use or there may be risks. How should I troubleshoot this?](https://github.com/Tencent/BqLog/issues/43).
- **Bug fix**: [The program crashed on exit, please help check the cause](https://github.com/Tencent/BqLog/issues/46), temporarily fixed in the current version. Will be completely resolved in the new 2.x version.
- **Pull Request**: [Retain local variables and other information in jar files for easier source code reading](https://github.com/Tencent/BqLog/pull/52).
  **v1.5.0 will be the final stable version before the 2.x series.**

## [v1.4.9] - 2025-05-29
- **Bug fix**: [能帮忙看一下这个crash是怎么回事吗？libsystem_kernel.dylib ___pthread_kill](https://github.com/Tencent/BqLog/issues/43), crash fix: crash caused by array overflow.

## [v1.4.7] - 2024-11-15
- **Bug fix**: [Enabling mmap on Android Devices May Cause Freezes or Crashes](https://github.com/Tencent/BqLog/issues/34), mmap recover feature is reopen.

## [v1.4.6] - 2024-11-15
- **Bug fix**: [mmap is not working on linux](https://github.com/Tencent/BqLog/issues/25)
- **Bug fix**: [static initialization order fiasco](https://github.com/Tencent/BqLog/issues/26)
- **Bug fix**: [mmap leads to hang on Android](https://github.com/Tencent/BqLog/issues/32)
- **Bug fix**: [Enabling mmap on Android Devices May Cause Freezes or Crashes](https://github.com/Tencent/BqLog/issues/34)
We have to temporarily disable the mmap recover feature until we can resolve its stability issues.

## [v1.4.5] - 2024-10-10
- **Improvement**: The configuration for the snapshot feature has been migrated from the API to the configuration file, supporting configurations for buffer size, category mask, and levels. For details, refer to [Snapshot Configuration](./README.md#snapshot).
- **Improvement**: The Linux and Unix binaries will be automatically placed in the 32-bit and 64-bit directories based on the hardware architecture.
- **Bug fix**: Here, we found that in cases of high-concurrency `reset_config` operations, some thread safety issues may arise. We have fixed these issues and added corresponding checks for this scenario in the automated test cases.

## [v1.4.4] - 2024-09-06
- **Improvement**: Add 2 tech articles to docs folder.
- **Bug fix**: Fix the bugs about `C++ 20 format`, Thanks the [issue report](https://github.com/Tencent/BqLog/issues/13) from [sdaereew](https://github.com/sdaereew).  Thanks to the contribution from [fkxingkong](https://github.com/fkxingkong), [see pull request](https://github.com/Tencent/BqLog/pull/17).

## [v1.4.3] - 2024-09-06
- **Improvement**: Support to Unix like OS, which has passed the test on FreeBSD. Thanks to the contribution from [bedwardly-down](https://github.com/bedwardly-down), [see commit](https://github.com/Tencent/BqLog/commit/77cfbc68fc38cceeb25ef75b6ccce3798e9c12e1).
- **Bug fix**: Fix a bug in the layout, which would cause data residue after using `C++ 20 format specifications`, leading to incorrect formatting of subsequent logs.  Thanks to the contribution from [fkxingkong](https://github.com/fkxingkong), [see pull request](https://github.com/Tencent/BqLog/pull/11).
- **Bug fix**: fix the side effects of `assert` in the include files.

## [v1.4.2] - 2024-09-04
- **Improvement**: An `uninit()` API has been added for calling before the program exits, to avoid issues where the program cannot exit normally in some cases. For details, see the API documentation.

## [v1.4.1] - 2024-08-30
- **Improvement**: In addition to passively intercepting the output of console appenders through console callbacks, it is also possible to cache the output of console appenders through a console buffer and actively retrieve it via API.
- **Bug fix**: Fix the compiling issue with C++ 20.
- **Bug fix**: Fix the issue where JNI_Onload sometimes is not called.

## [v1.4.0] - 2024-08-03
- **First Release**
