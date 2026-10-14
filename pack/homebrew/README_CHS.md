# BqLog 的 Homebrew formula

[English](./README.md)

`pippocao/homebrew-bqlog` 这个 tap 所用的 formula 与 bump 脚本。tap 仓库里放的是副本；
本目录是可评审的源头，formula 的改动跟着它所打包的代码一起评审。

## formula 装了什么

只有一个 formula，`brew install bqlog` 就覆盖了全部用法：

| | 内容 |
|---|---|
| 库 | `libBqLog.a` 与 `libBqLog.dylib`（两种形态），导出为 `bqlog::BqLogShared` 与 `bqlog::BqLogStatic` |
| 头文件 | `include/bq_log/` 与 `include/bq_common/`，含 C++20 模块接口单元 `bq_log/bq.log.cppm` |
| 包发现 | `BqLogConfig.cmake` 与 `bqlog.pc` |
| 命令行工具 | `BqLog_CategoryLogGenerator` 与 `BqLog_LogDecoder`，装在 `bin/` |

工具会把库源码编进可执行文件，放在同一个 formula 里只多几秒编译；若拆成第二个 formula，等于
为两个小命令把整个库再编一遍。

从源码构建，本次不发布 bottle，因此 `brew install` 会现场编译。

## 覆盖平台

macOS（Apple 芯片与 Intel）与 Linux（glibc，含 WSL2 里的 Ubuntu）。

iOS、Android、HarmonyOS、Windows 原生，以及 BSD / Solaris 系列不在覆盖范围内，这些继续
使用 Release 页面的预编译包或各自的包管理器。

## 建 tap

```bash
brew tap-new pippocao/bqlog
cp -r pack/homebrew/Formula "$(brew --repository)/Library/Taps/pippocao/homebrew-bqlog/"
```

然后把 tap 仓库提交并推上去。

## 安装

```bash
brew tap pippocao/bqlog
brew install bqlog
```

## 发新版本时更新 formula

1. 按常规流程发版，它会创建 `Release_<版本>` 这个 tag。
2. 把 formula 指向该版本并刷新校验值：

```bash
bash pack/homebrew/bump.sh 2.7.0
```

3. 跑下面的验收检查，把 `Formula/` 复制进 tap 并推送。

`bump.sh` 需要下载 tag archive 来算校验值，所以要在发版完成之后再跑。

## 发布顺序

tag 是所有环节的锚点：tarball 的校验值与 `brew test-bot` 的预演都基于它，所以顺序不能乱。

1. 给「将要进入 main 的那个 commit」打 tag 并推送。**此时先不要合并 main。**
2. 用 `bump.sh` 把 formula 指向该 tag，再按下面的验收检查验证 tap。
3. 把分支合并进 main。优先 fast-forward：产生合并提交会让 release tag 落在不是 main 顶端的
   commit 上。
4. 跑 `brew test-bot` 预演官方 CI，包含 bottle 构建。

由此有两条规则：

- tag 一旦推送即固定。若验证过程中发现需要改代码，就发下一个补丁版本，不要移动 tag。
- `release.yml` 对已存在的 tag 会静默跳过创建，所以不要用打算废弃的 tag 去跑发布流程。

## 验收检查

```bash
brew install --build-from-source ./Formula/bqlog.rb
brew audit --strict --online --new bqlog
brew test bqlog
```

`brew install --build-from-source` 就是用户实际走的路径。`brew audit` 是 `homebrew-core`
的检查项，在这里通过，意味着以后可以直接迁移到官方仓库。

## 面向 homebrew-core

`brew audit --strict --online --new bqlog` 与 `homebrew-core` 的检查项一致，`brew test-bot`
则完整预演官方 CI（含 bottle 构建）。两者合起来就能回答「会不会被收录」，而无需真的提 PR。

向上游提交之前：

- Release tag 必须存在于本仓库；他们的 CI 从 tag 构建，不从 fork 构建。
- `livecheck` 必须能解析出结果，这也是 formula 覆盖默认值的原因：tag 是 `Release_<版本>`，
  不是 Homebrew 默认假设的 `v<版本>`。不覆盖的话 audit 会失败，自动 bump 机器人也永远发现不了
  新版本。
- formula 同时提供命令行工具与库，一次安装覆盖两类使用者。工具只多几秒编译，比另开一个
  formula（为两个小命令把整个库再编一遍）划算。

官方合入的 formula 不能像 tap 那样撤回 —— 移除要先 deprecate 再 disable。验证请用
`brew audit` 与 `brew test-bot`，不要用真实 PR 去试。

## 使用安装好的库

```cmake
find_package(BqLog REQUIRED)
target_link_libraries(app PRIVATE bqlog::BqLogShared)   # 或 bqlog::BqLogStatic
```

```bash
pkg-config --cflags --libs bqlog
```

## C++20 模块

模块接口单元随头文件一起安装，不预编译 BMI：它与编译器、标准、宏定义绑定，由使用方编译
一次。

```cpp
import bq.log;
```

```cmake
find_package(BqLog REQUIRED)

target_sources(app PRIVATE FILE_SET CXX_MODULES
    BASE_DIRS ${BQLOG_INCLUDE_DIR}
    FILES ${BQLOG_MODULE_FILE})
target_link_libraries(app PRIVATE bqlog::BqLogShared)
```

这需要 CMake 3.28 或更新版本，以及一个真正实现 C++20 模块、且模块依赖扫描可用的编译器
（`clang-scan-deps`）。这是硬性要求：如果编译器接受 `-std=c++20` 却解析不了 `export module`，
它会在接口单元上直接报错，而不是跳过。include 入口没有这个要求——库的其余部分用任何受支持的
编译器构建即可。

两点约束，见[集成指南](../../docs/INTEGRATION_GUIDE_CHS.md)：

- `BQ_LOG_FAST_*` 是宏，无法导出；模块方式下快速日志写作 `log.fast_info<"...">(args...)`。
- 模块必须与工程其余部分用同一个编译器、同一个标准、同一套宏定义编译。
