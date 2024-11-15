# Changelog
## [v1.4.6] - 2024-11-15
- **Bug fix**: [mmap is not working on linux](https://github.com/Tencent/BqLog/issues/25)
- **Bug fix**: [static initialization order fiasco](https://github.com/Tencent/BqLog/issues/26)
- **Bug fix**: [mmap leads to hang on Android](https://github.com/Tencent/BqLog/issues/32)

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
