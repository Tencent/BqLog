# Changelog

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