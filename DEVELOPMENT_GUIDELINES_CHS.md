# 开发规范

[English](DEVELOPMENT_GUIDELINES.md)

这些约定用于保证 BqLog 在不同平台、不同编译器、不同语言 wrapper 之间保持一致。适用于 `src/`、`include/`、`wrapper/`、`plugin/`、`tools/`、构建脚本和测试；例外情况在各条中写明。

## 0. 注释

- 尽量精简：好名字和清晰的结构胜过文字说明。只注释代码说不出来的东西（原因、不变量、平台差异），不复述下一行在做什么。
- 注释用英文，一到两行。
- 不保留被注释掉的代码，不在注释里写改动历史。

## 1. 代码结构

清晰、优雅、模块化优先：代码被读的次数远多于被写的次数。一个文件、一个类只负责一个主题，接口要窄且把状态留在内部，分层方向单向（`bq_common` → `bq_log` → wrapper / plugin / tools）。

## 2. 命名

- 文件名、类型、函数、变量、枚举值和成员一律 `snake_case`：`meta_registry`、`enable_meta_records`、`run_version()`、`log_level::info`。
- 宏用 `BQ_` + 全大写（见第 5 条）；模板参数用大写（`STR`、`CAT_INDEX`、`T`）。
- 不用匈牙利前缀，不用 `p_`/`m_`，不自己造缩写。
- 布尔值读起来要像一句判断：`is_valid()`、`need_recovery`、`meta_records_enabled_`。

## 3. 成员变量以 `_` 结尾

- 类和结构体的数据成员以 `_` 结尾：`log_id_`、`category_level_words_`、`meta_records_segments_`。
- 例外：导出结构体和 C 兼容类型的成员保持原样，因为这些名字属于 ABI 或公开头文件（`_api_string_def::str`、`_log_entry_head_def` 的字段）。

## 4. 导出 ABI 与内存布局

凡是跨二进制/库边界的东西——C API、语言 wrapper、插件 ABI、以及所有文件格式——布局必须在编译期固定，并且在所有支持的编译器上完全一致：

- 只用定长类型（`uint32_t`、`int64_t`）；跨边界的结构体里不用 `long`/`size_t`/`bool`/`unsigned`；不用位域（位域布局依赖编译器，需要时用显式字段加掩码）；跨边界的枚举必须显式指定底层类型。
- 格式要求精确紧凑时用 `BQ_PACK_BEGIN` / `BQ_PACK_END`；对齐有意义时用 `alignas(n)`。
- 在定义旁边写 `static_assert`，检查 `sizeof` 和关键字段的偏移，任何不小心的布局变动都直接编译失败：参考 `src/bq_log/types/meta/meta_registry.h` 里的 `meta_record_head`、`meta_record_entry_head`，以及 `include/bq_log/misc/bq_log_c_types.h`。
- 紧凑结构体里可能不对齐的字段，用 `memcpy` 读写，不要直接解引用强转指针。
- 文件格式必须带 magic、`format_version` 和预留空间；新增字段不能改变已有字段的含义。读取前先校验尺寸、版本和校验和。
- 改动导出布局或文件格式属于兼容性破坏：必须在 `CHANGELOG.md` 里写明并体现到版本号上（2.7.0 里关于 `bq::log` 句柄和 C API 的说明就是例子）。

## 5. 宏与平台判断

- 宏定义放在 `include/bq_common/platform/macros.h`，以 `BQ_` 开头。只用于配置某个组件的宏可以放在该组件自己的头文件里（如 `BQ_LOG_META_SEGMENT_SIZE`），同样以 `BQ_` 开头。
- 平台、编译器、架构判断只能使用 `macros.h` 里定义的宏：`BQ_WIN`/`BQ_WIN64`、`BQ_APPLE`/`BQ_MAC`/`BQ_IOS`、`BQ_LINUX`、`BQ_UNIX`、`BQ_POSIX`、`BQ_ANDROID`、`BQ_OHOS`、`BQ_PS`、`BQ_MSVC`/`BQ_GCC`/`BQ_CLANG`、`BQ_ARM*`/`BQ_X86*`。不要直接判断裸平台宏（`_WIN32`、`__APPLE__`、`__sun`、`_MSC_VER` 之类），也不要自己定义平台宏。
- 唯一例外是必须在第一个系统头文件之前定义的特性宏（如 `src/bq_common/platform/posix_misc.cpp` 顶部的 `_GNU_SOURCE`），这类情况要集中在一处。
- 尽量判断"能力"而不是"平台"：`try_lock_instance_file` 里先 `#if defined(F_OFD_SETLK)`，再 `#if defined(LOCK_EX) && defined(LOCK_NB)`。
- 语言能表达的地方，用内联函数和模板，不要用函数式宏。

## 6. 库内禁用 C++ 标准库

- `src/` 和 `include/` 里不允许使用 C++ 标准库，因为库必须能在 none-STL 一类环境里构建：不用 `<vector>`、`<string>`、`<map>`、`<algorithm>`、`<memory>`、`<thread>`、`<atomic>`，也不使用任何 `std::` 容器、算法或类型别名。
- 这类环境根本没有 libc++/libstdc++：Android 用 `-DANDROID_STL=none` 构建时会在 `src/CMakeLists.txt` 里打开 `BQ_NO_LIBCPP`；嵌入式与主机平台工具链同理。
- 用基础库替代：`bq::array`、`bq::string`、`bq::hash_map`、`bq::unique_ptr`、`bq::atomic`、`bq::thread`，`include/bq_common/types/type_traits.h` 里的 traits，以及 `src/bq_common/platform/no_lib_cpp_impl.h` 里补齐的 `operator new`、`__cxa_guard_*` 等。
- C 标准库和平台 API 可以用：`<stdint.h>`、`<string.h>`、`<stdio.h>`、`<time.h>`、pthread、Win32、JNI 等。
- 测试、benchmark、demo、工具不受此限制，它们不进产物。

## 7. 模块边界

- `bq_common` 是与日志逻辑无关的基础库：容器、平台抽象、内存映射、文件、加密、配置解析。它不感知日志，日志逻辑也不要渗进去。
- `bq_log` 引用 `bq_common` 只允许通过 `bq_common/bq_common.h`（源码/静态整合）或 `bq_common/bq_common_public_include.h`（链接库时的公开包含）。日志代码不要直接包含 `bq_common/...` 的内部头文件。
- `bq_common` 通常以源码方式整合进使用它的模块；新增文件要由该模块自己的 `CMakeLists.txt` 收集（`CMake_utils.txt` 里的 `COLLECT_SRC_FILES`），不要要求改动别人的构建。
- 仓库之外，C++ 唯一支持的入口是 `bq_log/bq_log.h`；`include/bq_log/` 下其余头文件都是内部的，可以改动。其他语言使用发布出去的 wrapper（`wrapper/python`、`wrapper/java`、`wrapper/go`、`wrapper/typescript`、`wrapper/csharp`），wrapper 就是它们的 API 和 import。

## 8. 复用与文件职责

- 先搜再写：容器、哈希、字符串工具、文件工具、平台调用、类型 traits，`bq_common` 里都已经有了，不要在模块内部重复实现。
- 一个文件只负责一个主题，新代码放到已经负责该主题的文件里。所有 SIMD 指令都写在 `src/bq_common/platform/simd.h`，所有平台宏都写在 `macros.h`，原子封装在 `bq_common/platform/atomic/`，配置解析在 `bq_common/utils/property*`。
- 平台相关代码一个平台一个文件、共用一套接口（`win64_misc.cpp`、`posix_misc.cpp`、`android_misc.cpp`、`java_misc.cpp`），不要把 `#if` 散落在通用代码里。
- 宁可写一个小而单一用途的辅助函数，也不要用一个标志位承载两种行为。

## 9. 新功能必须有测试

- 功能和问题修复必须在同一个提交里带上覆盖：
  - C++ 库的测试放 `test/cpp/`，在 `test/cpp/main.cpp` 里用 `TEST_GROUP_BEGIN`/`TEST_GROUP`/`TEST_GROUP_END` 注册（测试类本身写在 `test_*.h`）；`bq_common` 的测试放 `test/cpp/bq_common_test/`；
  - 多进程、多实例、崩溃恢复相关的测试放 `test/multi_instance/`（CTest 用例 `multi_instance_*`，与 `run.py` 里 `CASES` 的每一项一一对应：`instance_assignment`、`two_logs`、`recovery_off`、`crash_recovery`、`two_instances`、`legacy_coexistence`、`dual_runtime_plugins`、`child_lock`、`flushed_recovery`）；
  - wrapper 改动要在对应的 wrapper 测试里覆盖（`test/python`、`test/java`、`test/go`、`test/typescript`、`test/csharp`）。
- 测试必须能在 CI 构建的所有 C++ 标准（11、14、17、20、23）和所有平台上通过；`BQ_CPP_20` 才有的代码（`fast_*` 系列接口）要有 C++20 下会执行的测试。
- 测试要确定性、自己清理现场，不依赖执行顺序，也不依赖机器足够快。

## 10. 多语言 wrapper 保持一致

- 各语言 wrapper 暴露相同的 API 和相同的行为；允许的差异只限于语言本身的硬性要求（Go 导出名必须大写开头，Java/Kotlin 遵循包名与 camelCase 习惯，Python 用关键字参数等）。
- 名称、参数、默认值、语义都要一致，包括日志名/实例号/索引规则，以及"同一进程内多语言共享同一个 Log 对象"这一行为。
- 新增 API 或配置项要同步到所有 wrapper；做不到时，在 `docs/API_REFERENCE.md` 里写明差异。

## 11. 提交必须通过 CI

- 只有仓库的 workflow 全绿才能合入：`AutoTest`（`.github/workflows/test.yml`，覆盖所有平台和所有 C++ 标准）和 `Build`。
- 推送前先跑对应的本地脚本（`build/test/...`、`build/lib/...`）。

## 12. License 声明

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
- 新文件要在同一个提交里加入某个构建目标（并加到 `format_code.sh`）。

## 13. 性能

- 热路径代码（日志调用、申请缓冲区、fast 路径、appender、时钟）按指令数和分支数最少来写。
- 动了热路径的改动不能让 benchmark 结果变差（`benchmark/cpp`，见 `docs/BENCHMARK.md`）。

## 14. 格式化与工具

- 提交前跑 `format_code.sh`（Windows 用 `format_code.bat`），风格由仓库根目录的 `.clang-format` 决定。
- 构建脚本和 CI 放在 `build/` 与 `.github/workflows/`；新增平台或新增测试目标要落到那里，而不是写一个临时的本地脚本。
- 面向用户的文本（README、CHANGELOG、docs）都有英文版和 `_CHS` 版，同一个提交里同步维护。

