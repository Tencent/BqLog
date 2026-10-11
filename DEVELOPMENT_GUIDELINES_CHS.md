# 开发规范

[English](DEVELOPMENT_GUIDELINES.md)

这些约定用于保证 BqLog 在不同平台、不同编译器、不同语言绑定之间保持一致，并保证日志调用路径的性能。适用于 `src/`、`include/`、`wrapper/`、`plugin/`、`tools/`、构建脚本和测试；少数例外在各条中写明。确实需要破例时，请在 PR 里说明原因。

## 0. 注释

- 代码优先。一个好名字、清晰的结构，胜过一整段说明性文字。
- 只注释代码说不出来的东西：某个决策的原因、读者看不到的不变量、平台差异。不要复述下一行在做什么。
- 注释用英文，一到两行。
- 不保留被注释掉的代码，不在注释里写改动历史，git 已经有这些信息。
- 代码读不懂时，先改代码，而不是加注释去圆。

## 1. 命名

- 文件名、类型、函数、变量、枚举值和成员一律 `snake_case`：`meta_registry`、`enable_meta_records`、`run_version()`、`log_level::info`。
- 宏用 `BQ_` + 全大写（见第 4 条）；模板参数用大写（`STR`、`CAT_INDEX`、`T`）。
- 不用匈牙利前缀，不用 `p_`/`m_`，不自己造缩写。名字要说明"它是什么"，而不是"它怎么实现"。
- 布尔值读起来要像一句判断：`is_valid()`、`need_recovery`、`meta_records_enabled_`。

## 2. 成员变量以 `_` 结尾

- 类和结构体的数据成员以 `_` 结尾：`log_id_`、`category_level_words_`、`meta_records_segments_`。
- 例外：导出结构体和 C 兼容类型的成员保持原样，因为这些名字属于 ABI 或公开头文件（`_api_string_def::str`、`_log_entry_head_def` 的字段）。

## 3. 导出 ABI 与内存布局

凡是跨二进制/库边界的东西——C API、语言绑定、插件 ABI、以及所有文件格式——布局必须在编译期固定，并且在所有支持的编译器上完全一致：

- 只用定长类型（`uint32_t`、`int64_t`）；跨边界的结构体里不用 `long`/`size_t`/`bool`/`unsigned`；不用位域（位域布局依赖编译器，需要时用显式字段加掩码）；跨边界的枚举必须显式指定底层类型。
- 格式要求精确紧凑时用 `BQ_PACK_BEGIN` / `BQ_PACK_END`；对齐有意义时用 `alignas(n)`。
- 在定义旁边写 `static_assert`，检查 `sizeof` 和关键字段的偏移，任何不小心的布局变动都直接编译失败：参考 `src/bq_log/types/meta/meta_registry.h` 里的 `meta_record_head`、`meta_record_entry_head`，以及 `include/bq_log/misc/bq_log_c_types.h`。
- 紧凑结构体里可能不对齐的字段，用 `memcpy` 读写，不要直接解引用强转指针。
- 文件格式必须带 magic、`format_version` 和预留空间；新增字段不能改变已有字段的含义，读取时必须先校验尺寸、版本和校验和再相信任何一个字节。
- 改动导出布局或文件格式属于兼容性破坏：必须在 `CHANGELOG.md` 里写明并体现到版本号上（2.7.0 里关于 `bq::log` 句柄和 C API 的说明就是例子）。

## 4. 宏与平台判断

- 宏定义放在 `include/bq_common/platform/macros.h`，以 `BQ_` 开头。只用于配置某个组件的宏可以放在该组件自己的头文件里（如 `BQ_LOG_META_SEGMENT_SIZE`），同样以 `BQ_` 开头。
- 平台、编译器、架构判断只能使用 `macros.h` 里定义的宏：`BQ_WIN`/`BQ_WIN64`、`BQ_APPLE`/`BQ_MAC`/`BQ_IOS`、`BQ_LINUX`、`BQ_UNIX`、`BQ_POSIX`、`BQ_ANDROID`、`BQ_OHOS`、`BQ_PS`、`BQ_MSVC`/`BQ_GCC`/`BQ_CLANG`、`BQ_ARM*`/`BQ_X86*`。不要直接判断裸平台宏（`_WIN32`、`__APPLE__`、`__sun`、`_MSC_VER` 之类），也不要自己定义平台宏。
- 唯一例外是必须在第一个系统头文件之前定义的特性宏（如 `src/bq_common/platform/posix_misc.cpp` 顶部的 `_GNU_SOURCE`）。这类情况要集中在一处，并在旁边写明原因。
- 尽量判断"能力"而不是"平台"：`try_lock_instance_file` 里先 `#if defined(F_OFD_SETLK)`，再 `#if defined(LOCK_EX) && defined(LOCK_NB)`，这样在没见过的新平台上也是正确的。
- 语言能表达的地方，用内联函数和模板，不要用函数式宏。

## 5. 库内禁用 C++ 标准库

- `src/` 和 `include/` 里不允许使用 C++ 标准库：不用 `<vector>`、`<string>`、`<map>`、`<algorithm>`、`<memory>`、`<thread>`、`<atomic>`，也不使用任何 `std::` 容器、算法或类型别名。
- 原因是体积和可移植性：同一份源码要在 `BQ_NO_LIBCPP` 下构建（`src/CMakeLists.txt` 在 Android 使用 `-DANDROID_STL=none` 时打开），这样产物体积才能小——Android 动态库约 200 KB——同时也能在没有 libc++ 的工具链上编译。
- 用基础库替代：`bq::array`、`bq::string`、`bq::hash_map`、`bq::unique_ptr`、`bq::atomic`、`bq::thread`，`include/bq_common/types/type_traits.h` 里的 traits，以及 `src/bq_common/platform/no_lib_cpp_impl.h` 里补齐的 `operator new`、`__cxa_guard_*` 等。
- C 标准库和平台 API 可以用：`<stdint.h>`、`<string.h>`、`<stdio.h>`、`<time.h>`、pthread、Win32、JNI 等。
- 测试、benchmark、demo、工具不受此限制，它们不进产物。

## 6. 模块边界

- `bq_common` 是与日志逻辑无关的基础库：容器、平台抽象、内存映射、文件、加密、配置解析。它不感知日志，日志逻辑也不要渗进去。
- `bq_log` 引用 `bq_common` 只允许通过 `bq_common/bq_common.h`（源码/静态整合）或 `bq_common/bq_common_public_include.h`（链接库时的公开包含）。日志代码不要直接包含 `bq_common/...` 的内部头文件。
- `bq_common` 通常以源码方式整合进使用它的模块；新增文件要由该模块自己的 `CMakeLists.txt` 收集（`CMake_utils.txt` 里的 `COLLECT_SRC_FILES`），不要要求改动别人的构建。
- 仓库之外，C++ 唯一支持的入口是 `bq_log/bq_log.h`；`include/bq_log/` 下其余头文件都是内部的，可以改动。其他语言使用发布出去的绑定（`wrapper/python`、`wrapper/java`、`wrapper/go`、`wrapper/typescript`、`wrapper/csharp`），绑定就是它们的 API 和 import。

## 7. 复用与文件职责

- 先搜再写：容器、哈希、字符串工具、文件工具、平台调用、类型 traits，`bq_common` 里都已经有了，不要在模块内部重复实现。
- 一个文件只负责一个主题，新代码放到已经负责该主题的文件里。所有 SIMD 指令都写在 `src/bq_common/platform/simd.h`，所有平台宏都写在 `macros.h`，原子封装在 `bq_common/platform/atomic/`，配置解析在 `bq_common/utils/property*`。
- 平台相关代码一个平台一个文件、共用一套接口（`win64_misc.cpp`、`posix_misc.cpp`、`android_misc.cpp`、`java_misc.cpp`），不要把 `#if` 散落在通用代码里。
- 宁可写一个小而单一用途的辅助函数，也不要用一个标志位承载两种行为。

## 8. 新功能必须有测试

- 功能和问题修复必须在同一个提交里带上覆盖：
  - C++ 库的测试放 `test/cpp/`，在 `test/cpp/main.cpp` 里用 `TEST_GROUP_BEGIN`/`TEST_GROUP`/`TEST_GROUP_END` 注册（测试类本身写在 `test_*.h`）；`bq_common` 的测试放 `test/cpp/bq_common_test/`；
  - 多进程、多实例、崩溃恢复相关的测试放 `test/multi_instance/`（`run.py` 里的 CTest 用例 `M1`…`M9`）；
  - 绑定改动要在对应的绑定测试里覆盖（`test/python`、`test/java`、`test/go`、`test/typescript`、`test/csharp`）。
- 测试必须能在 CI 构建的所有 C++ 标准（11、14、17、20、23）和所有平台上通过；`BQ_CPP_20` 才有的代码（`fast_*` 系列接口）要有 C++20 下会执行的测试。
- 测试要确定性、自己清理现场，不依赖执行顺序，也不依赖机器足够快。
- 体积也是契约的一部分：`test/symbol_size/` 用于度量发布出去的库。
- 测试很难写，通常说明接口设计得不对。

## 9. 多语言绑定保持一致

- 各语言绑定暴露相同的 API 和相同的行为；允许的差异只限于语言本身的硬性要求（Go 导出名必须大写开头，Java/Kotlin 遵循包名与 camelCase 习惯，Python 用关键字参数等）。
- 名称、参数、默认值、语义都要一致，包括日志名/实例号/索引规则，以及"同一进程内多语言共享同一个 Log 对象"这一行为。
- 新增 API 或配置项要同步到所有绑定；做不到时，在 `docs/API_REFERENCE.md` 里写明差异。

## 10. 提交必须通过 CI

- 只有仓库的 workflow 全绿才能合入：`AutoTest`（`.github/workflows/test.yml`，覆盖所有平台和所有 C++ 标准）和 `Build`。
- 提交前先跑对应的本地脚本（`build/test/...`、`build/lib/...`），比等 CI 快得多。
- commit 信息要短、直接说明改了什么（例如 `Category log wrapper: build in C++11 and C++14 again`）；不用 emoji，不提工具/AI，不加 co-author trailer。
- 一个提交只做一件事：改名不要混行为改动，修 bug 不要混重构。

## 11. License 声明

- 每个源码文件开头都要有 license 声明，直接从现有文件复制：
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
- 新文件要在同一个提交里加入某个构建目标（并加到 `format_code.sh`）；没有任何构建会编译的文件就是死代码。

## 12. 性能

- 热路径代码（日志调用、申请缓冲区、fast 路径、appender、时钟）要按"指令数和分支数最少"来写：能编译期做的就编译期做，不分配内存，能用原子就不用锁，调用点需要的数据尽量落在同一条 cache line，`bq_forceinline` 只在实测有效时使用。
- 动了热路径就不能让 benchmark 变差：改动前后各跑一次 `benchmark/cpp`（方法与参考数据见 `docs/BENCHMARK.md`），把数据贴到 PR 里。
- 选择最便宜的正确做法：比如用 `fetch_add` 申请加显式回滚替代 CAS 重试；关闭的日志级别只花一次访存和一次位测试。
- 非热路径（启动、恢复、解码、配置解析）优先可读性，不必炫技。

## 13. 格式化与工具

- 格式不是个人喜好：提交前跑 `format_code.sh`（Windows 用 `format_code.bat`），风格由仓库根目录的 `.clang-format` 决定。
- 构建脚本和 CI 放在 `build/` 与 `.github/workflows/`；新增平台或新增测试目标要落到那里，而不是写一个临时的本地脚本。
- 面向用户的文本（README、CHANGELOG、docs）都有英文版和 `_CHS` 版，同一个提交里同步维护。

## Review 检查表

- 没有新增"复述代码"的注释；保留的注释是英文的。
- 命名是 `snake_case`，成员以 `_` 结尾，宏是 `BQ_` 且放在 `macros.h`。
- 没有新增裸平台宏；`src/`+`include/` 里没有新增对 C++ 标准库的使用。
- 跨边界的东西布局固定并有 `static_assert`；格式或 ABI 变更在 CHANGELOG 里写明。
- 新行为有测试覆盖，且在 C++11/14/17/20/23 下都能通过。
- 动了热路径的话，benchmark 没有变差。
- 提交能通过 `AutoTest` 和 `Build`；commit 信息说清了改了什么。
