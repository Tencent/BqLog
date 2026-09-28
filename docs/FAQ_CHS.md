# 常见问题（FAQ）

[English](FAQ.md)

## BqLog 是什么？

一个开源的工业级日志库，C++ 为核心，腾讯出品。它要解决的矛盾是：发行版本里日志想全开，又怕拖性能、占包体、占存储——所谓"不可能三角"。它不是玩具库：王者荣耀国际服（Honor of Kings）在生产环境使用，客户端侧有 3000 万以上 DAU 的产品验证，《王者万象棋》《洛克王国》等近两年的自研游戏也已接入。

## BqLog 和 spdlog、quill、fmtlog 有什么区别？

单看性能：spdlog 强在生态和易用，quill 强在调用点延迟，fmtlog 强在轻量文本，BqLog 在同场景 benchmark 里总吞吐和内存占用都领先（[数字在这](BENCHMARK.md)）。

但真正的差别在工业级的完整度。一个日志库写进发行版之后要面对的问题——崩溃丢日志、内容加密、多语言混编、移动端的编译约束、多版本多次编译的兼容性、包体预算——这些全部都在 BqLog 的覆盖和考虑范围内。另外三家在这些维度上都有各自的缺口。

## 谁在生产环境用它？

王者荣耀国际服（Honor of Kings）——客户端侧 3000 万以上 DAU 的产品验证。《王者万象棋》《洛克王国》等近两年的腾讯自研游戏也已引入。

## 支持哪些平台和语言？

- 平台：Windows 64-bit、macOS、Linux（含嵌入式）、iOS、Android、HarmonyOS、OpenHarmony、Unix（FreeBSD、NetBSD、OpenBSD、Solaris 等）
- 语言：C++（C++11 起）、Java/Kotlin、C#（Unity、.NET）、ArkTS/C++（HarmonyOS 与 OpenHarmony 共用同一包）、JavaScript/TypeScript（Node.js）、Python 3.7+、Go、Unreal Engine（UE4、UE5 及 UE6 开发构建）
- 硬件架构：x86、x86_64、ARM32、ARM64
- 集成方式：动态库、静态库、源码

## 有没有发布二进制不支持的平台？

有：PS5 和 Switch 暂时没有预编译二进制（主机平台的工具链有授权限制），这两个平台用源码集成方式自行编译即可，核心代码只依赖 C 标准库和平台 API，移植成本很低。

## 对游戏引擎的支持如何？

专项支持，不是"能编过"就算完：Unity、团结引擎、Unreal 都有现成插件/包；UE 插件对蓝图和 UE 内置数据类型（FString、FName 等）有天然支持；Unity 侧对常用数据类型做了适配。引擎内写日志和自己项目里写 C++ 是一个手感。见[引擎集成指南](ENGINE_INTEGRATION.md)（[中文](ENGINE_INTEGRATION_CHS.md)）。

## 有哪些发布渠道？

各语言都有自己的正规渠道：

- **GitHub Releases**：全平台预编译包（xcframework、Android AAR、npm tgz、Python wheel、各语言 wrapper 包等）
- **Maven Central**：Android / Java / Kotlin
- **npm**：`@pippocao/bqlog`（Node.js / TypeScript）
- **PyPI**：`pip install bqlog`
- **ohpm**：HarmonyOS / OpenHarmony（`ohpm install bqlog`）
- **Fab**：Unreal Engine 插件
- **Unity / 团结引擎**：package 导入
- **Go**：源码模块分发

## 性能数据在哪？

[docs/BENCHMARK.md](BENCHMARK.md)：1–10 线程、每线程 200 万条、每条带 4 个格式化参数，吞吐、峰值内存、输出文件体积三组数据。测试代码在 benchmark/ 目录，可以自己跑。

## 崩溃后日志怎么办？

这是 BqLog 的主打能力之一。异步模式下缓冲区放在内存映射文件（mmap）里，进程没了，数据还在。重启后逐文件校验完整性，作废崩溃瞬间写了一半的半成品，剩下的按每个线程的序号严格依序回放进新日志文件的恢复段。原理和图解在系列文章[第二篇的 7.3 节](<文章2_为何BqLog如此快 - 从环形队列到自适应数据总线.MD>)。

## 多种语言能混用吗？

能，而且是真混用：同一进程里，C++、Java、C#、Python、TypeScript 的封装可以访问**同一个** Log 对象，日志进同一个文件。Java、C#、TypeScript 封装的热路径上几乎没有额外堆分配。做混合语言客户端（比如游戏引擎 + 脚本层）的，这一点别家给不了。

## 服务器能用吗？

能。客户端和服务器是同一套核心。服务器侧关心的高并发吞吐、各种操作系统（Linux、FreeBSD 系、Solaris 系都跑过测试）、长稳运行，都在覆盖范围内。高并发下日志丢失或卡顿的场景，正是这套队列设计要治的病。

## 支持日志加密吗？

支持，压缩文件 Appender 可以开 RSA+AES 混合加密，按文件段做。从 benchmark 数字看，开着加密的行和不加密的行几乎贴在一起。

## 编译和部署上有什么讲究？

几乎没有讲究，这是刻意设计的结果：只依赖 C 标准库和平台 API，C++11 起步，`-Wall -Wextra -pedantic -Werror` 全开能过编译，Android `ANDROID_STL=none` 模式可编。构建用 CMake，各平台脚本齐全。动态库、静态库、源码三种集成方式都支持。

## 压缩日志怎么读？

压缩格式是二进制，用仓库里的解码器还原成文本：`tools/log_decoder`。格式细节见[第一篇](<文章1_为何BqLog如此快 - 高性能实时压缩日志格式.MD>)。

## 一定要用压缩格式吗？能写普通文本吗？

能。TextFileAppender 输出普通文本，benchmark 里文本模式也快于所有参评的文本日志库。压缩和文本可以挂在同一个 Log 上混用。

## 包体和内存占用多大？

Android 动态库约 200 KB。benchmark 的 10 线程用例（共 2000 万条日志、挂三个 Appender）里，BqLog 自身峰值内存 2–3 MB；移动端场景一般 1 MB 上下。对比对象里有到 GB 级的。

## 消费者写不过来时会丢日志吗？

看配置：可以丢弃新日志（discard）、让生产者等待（block），或者自动扩容（expand）。细节见[第二篇的 §8.2](<文章2_为何BqLog如此快 - 从环形队列到自适应数据总线.MD>)和 [ADVANCED_USAGE](ADVANCED_USAGE_CHS.md)。

## 开源协议是什么？

Apache-2.0，可自由商用。并发方案在开源前申请了专利，设计原理见[第二篇](<文章2_为何BqLog如此快 - 从环形队列到自适应数据总线.MD>)。

## 怎么接入我的项目？

标准环境（C++、Java、C#、Python、Node.js 等）看[集成指南](INTEGRATION_GUIDE.md)（[中文](INTEGRATION_GUIDE_CHS.md)）；游戏引擎看[引擎集成](ENGINE_INTEGRATION.md)（[中文](ENGINE_INTEGRATION_CHS.md)），Unity 和 Unreal 有现成插件。
