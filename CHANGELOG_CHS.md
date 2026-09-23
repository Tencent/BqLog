# 更新日志

[English](CHANGELOG.md)

## [v2.5.0] - 2026-09-23
- **性能提升**：优化日志写入，减少无效的工作线程唤醒和 Java 参数分配。
- **Go 语言支持**：新增 Go wrapper，支持自动参数转换、category 和源码模块分发。

## [v2.4.1] - 2026-07-26
- **兼容性**：[Python wheel 要求 glibc 2.38+（#72）](https://github.com/Tencent/BqLog/issues/72)——发布产物统一改为在 Ubuntu 22.04（glibc 2.35）、Windows Server 2022 及最低支持的 BSD 版本上构建，包括 FreeBSD 13.2/13.5、OpenBSD 7.7、NetBSD 10.1、DragonFlyBSD 6.4.2、OmniOS r151054，兼容较旧的 Linux 和 BSD 系统。
- **构建/CI**：BSD 构建依赖改为自托管的不可变快照，避免上游软件源删除已停止支持系统的软件包，使发布流程可复现。

## [v2.4.0] - 2026-07-21
- **性能提升——百尺竿头，更进一步**：常规场景性能提升约 **10%～20%**，实际收益随线程数、日志格式、参数类型和输出模式而变化。
- **Bug 修复**：[轻微内存泄漏（#70）](https://github.com/Tencent/BqLog/issues/70)——为压缩文件 Appender 的内部缓存设置严格上限，解决部分场景下内存持续增长的问题。
- **Bug 修复**：修复 `log.buffer_policy_when_full` 解析错误，正确应用 `discard`、`block` 或 `expand`，不再错误回退到默认 `block`。
- **配置**：文件 Appender 新增 `write_cache_size`，可单独设置 64 KiB～4 MiB 的写缓存。
- **配置**：压缩文件 Appender 新增 `format_template_cache_max_entries` 和 `thread_info_cache_max_entries`，默认值分别为 `100000` 和 `2048`。超出缓存上限后内存仍受控，但可能重复写入模板，使压缩日志文件变大。
- **健壮性**：简化压缩 Appender 查找缓存，移除冗余的热点子表，并使用 murmur3 风格的哈希混合，避免特定结构的键集中落入同一探测链。

## [v2.3.2] - 2026-07-12
- **Bug 修复**：[解码结果丢失嵌入的空字节（#69）](https://github.com/Tencent/BqLog/issues/69)——日志解码器现在完整保留空字节。
- **Bug 修复**：修复同一日志文件反复切换不同加密模式时，多段混合加密内容的解码和恢复问题。
- **健壮性**：加强损坏日志文件的解析校验，避免异常尺寸导致的内存分配；完善文件截断与 mmap 分配校验，在建立映射前报告磁盘错误。
- **文档**：补全 Java wrapper 的 Javadoc。

## [v2.3.1] - 2026-06-16
- **Bug 修复**：[磁盘写满时崩溃（SIGBUS）或卡死（#67）](https://github.com/Tencent/BqLog/issues/67)。
- **Bug 修复**：[C# / Unity Editor 调用 fetch_and_remove_console_buffer 偶发崩溃（#66）](https://github.com/Tencent/BqLog/issues/66)——修复 native 调用期间 delegate 被 GC 回收的问题。
- **Unreal Engine**：新增 UE 5.8 支持，Fab、Prebuilt 和 Source 分发覆盖 UE 5.0～5.8。
- **Unreal Engine 6**：为当前 UE6 开发版本新增 Source 和 Prebuilt 插件包，仅通过 GitHub Releases 分发；Fab 支持等待 Epic 正式开放 UE6。

## [v2.3.0] - 2026-06-03
- **OpenHarmony 兼容性**：TypeScript wrapper 将 ES2020 的 `0n` 字面量替换为 `BigInt(0)`，同一份 ohpm `bqlog` 包同时支持 OpenHarmony 4.1+（API 11+）与 HarmonyOS NEXT。
- **恢复能力**：加强恢复过程中的数据校验，更早识别损坏数据，避免消费损坏的 ring buffer 时发生崩溃。

## [v2.2.9] - 2026-05-06
- **兼容性**：改善较旧 Clang 工具链及 MFC 等框架的兼容性。
- **兼容性**：改善所有受支持 Unreal Engine 版本的代码兼容性。

## [v2.2.8] - 2026-04-10
- **代码质量**：统一重构 SFINAE，将 `enable_if` 从返回值移到模板参数，提高可读性和兼容性。
- **Bug 修复**：[C++ 开启调用栈时崩溃（#62）](https://github.com/Tencent/BqLog/issues/62)。

## [v2.2.7] - 2026-04-01
- **Python 支持**：通过 CPython C Extension wrapper（Stable ABI）支持 Python 3.7+。
- **npm 发布**：Node.js wrapper 发布为 [`@pippocao/bqlog`](https://www.npmjs.com/package/@pippocao/bqlog)，可通过 `npm install @pippocao/bqlog` 安装。
- **PyPI 发布**：Python wrapper 发布为 [`bqlog`](https://pypi.org/project/bqlog/)，可通过 `pip install bqlog` 安装。
- **Python category 生成器**：`BqLog_CategoryLogGenerator` 新增 Python category wrapper（`.py`）输出。
- **TypeScript 双目标生成**：分别生成 Node.js 的 `_nodejs.ts`（导入 `@pippocao/bqlog`）和 HarmonyOS 的 `_ohos.ts`（导入 `bqlog`）。
- **Category 测试覆盖**：Python、Java、C# 和 TypeScript 新增 category 测试，覆盖输出、层级、格式参数和 mask 过滤。
- **包分发**：支持 npm（@pippocao/bqlog）、PyPI（bqlog）、Maven Central（com.tencent.bqlog）和 OHPM（bqlog），可分别通过 npm、pip、Gradle/Maven 和 ohpm 安装。

## [v2.1.2] - 2026-03-17
- **Bug 修复**：[GBox 沙箱中 Android 初始化后的 base_dir 不正确（#61）](https://github.com/Tencent/BqLog/issues/61)。

## [v2.1.1] - 2026-02-25
- **Bug 修复**：修复 MSVC 下 unsigned long 的处理错误。[无符号数输出为负数（#60）](https://github.com/Tencent/BqLog/issues/60)。

## [v2.1.0] - 2026-02-08

**BqLog 2.x 从底层重新构建，提供更好的性能、更广的平台支持和更完善的功能。**

### 从 1.x 到 2.x 的主要变化
- **鸿蒙支持**：新增原生 HarmonyOS 支持，包含 ArkTS 和 C++。
- **Node.js 支持**：支持 CJS 和 ESM。
- **跨平台改进**：改善兼容性、稳定性和通用性，支持更多 Unix 系统。
- **性能提升**：UTF-8 场景平均提升约 80%，UTF-16 场景（C#、Unreal、Unity）提升超过 500%。
- **Android 独立 C++**：Android 不再必须与 Java 配合使用。
- **配置变化**：移除 `is_in_sandbox`，改用 `base_dir_type`；新增 snapshot 过滤和每次启动创建新日志文件的支持。
- **混合非对称加密**：支持高性能混合加密，几乎没有额外开销。
- **游戏引擎插件**：提供 Unity、团结引擎和 Unreal Engine 插件，支持控制台输出重定向到引擎编辑器，以及 Unreal 蓝图。
- **二进制分发**：仓库不再包含二进制产物，从 2.x 起通过 [Releases 页面](https://github.com/Tencent/BqLog/releases) 获取对应平台和语言的包。
- **单条日志大小**：不再受 `log.buffer_size` 限制。
- **手动时区**：可手动指定时区。
- **废弃 raw_file**：`raw_file` Appender 在 2.x 中废弃并停止维护，请使用 `compressed_file`。
- **恢复能力转正**：提高恢复功能可靠性，从实验性功能转为正式功能。

---

## [v1.5.0] - 2025-09-22
- 开源许可证的公司主体从 THL A29 Limited 改为 Tencent。
- Android 二进制产物支持 16 KB 内存页。
- 修复写入大尺寸日志时可能出现的死循环。
- **Bug 修复**：[Android 按文档初始化后 is_valid 为 false，但仍可正常写日志（#43）](https://github.com/Tencent/BqLog/issues/43)。
- **Bug 修复**：[程序退出时崩溃（#46）](https://github.com/Tencent/BqLog/issues/46)——临时修复，计划在全新 2.x 版本中彻底解决。
- **Pull Request**：[在 jar 中保留局部变量等信息，方便直接阅读源码（#52）](https://github.com/Tencent/BqLog/pull/52)。
  **v1.5.0 是 1.x 系列的最后一个稳定版本。**

## [v1.4.9] - 2025-05-29
- **Bug 修复**：[libsystem_kernel.dylib 的 __pthread_kill 崩溃（#43）](https://github.com/Tencent/BqLog/issues/43)——修复数组越界导致的崩溃。

## [v1.4.7] - 2024-11-15
- **Bug 修复**：[Android 开启 mmap 可能冻结或崩溃（#34）](https://github.com/Tencent/BqLog/issues/34)——重新开放 mmap 恢复功能。

## [v1.4.6] - 2024-11-15
- **Bug 修复**：[Linux 下 mmap 不工作（#25）](https://github.com/Tencent/BqLog/issues/25)。
- **Bug 修复**：[静态初始化顺序问题（#26）](https://github.com/Tencent/BqLog/issues/26)。
- **Bug 修复**：[Android 上 mmap 导致卡死（#32）](https://github.com/Tencent/BqLog/issues/32)。
- **Bug 修复**：[Android 开启 mmap 可能冻结或崩溃（#34）](https://github.com/Tencent/BqLog/issues/34)。
在稳定性问题解决前，暂时关闭 mmap 恢复功能。

## [v1.4.5] - 2024-10-10
- **改进**：snapshot 参数从 API 移至配置文件，支持缓存大小、category mask 和日志等级。详见 [Snapshot 配置](./README_CHS.md#snapshot)。
- **改进**：Linux 和 Unix 二进制根据硬件架构自动放入 32 位或 64 位目录。
- **Bug 修复**：修复高并发调用 `reset_config` 时的线程安全问题，并增加对应自动化测试。

## [v1.4.4] - 2024-09-06
- **改进**：docs 目录新增两篇技术文章。
- **Bug 修复**：修复 C++20 格式化相关问题，感谢 [问题报告 #13](https://github.com/Tencent/BqLog/issues/13) 的提交者 [sdaereew](https://github.com/sdaereew)，以及 [fkxingkong](https://github.com/fkxingkong) 的 [PR #17](https://github.com/Tencent/BqLog/pull/17)。

## [v1.4.3] - 2024-09-06
- **改进**：新增 Unix 类系统支持，已通过 FreeBSD 测试。感谢 [bedwardly-down](https://github.com/bedwardly-down) 的 [提交](https://github.com/Tencent/BqLog/commit/77cfbc68fc38cceeb25ef75b6ccce3798e9c12e1)。
- **Bug 修复**：修复 C++20 格式说明符使用后布局数据残留，导致后续日志格式不正确的问题。感谢 [fkxingkong](https://github.com/fkxingkong) 的 [PR #11](https://github.com/Tencent/BqLog/pull/11)。
- **Bug 修复**：修复 include 文件中 assert 的副作用。

## [v1.4.2] - 2024-09-04
- **改进**：新增 `uninit()` API，用于程序退出前释放资源，解决部分情况下无法正常退出的问题。

## [v1.4.1] - 2024-08-30
- **改进**：除通过控制台回调被动获取日志外，新增 console buffer 缓存和主动提取接口。
- **Bug 修复**：修复 C++20 编译问题。
- **Bug 修复**：修复部分情况下 JNI_Onload 未被调用的问题。

## [v1.4.0] - 2024-08-03
- **首次发布**。
