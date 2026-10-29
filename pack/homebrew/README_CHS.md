# BqLog 的 Homebrew formula

[English](./README.md)

homebrew-core 中 `bqlog` formula 的源文件。

```bash
brew install bqlog
```

会安装静态库和动态库、头文件（含 C++20 模块 `bq_log/bq.log.cppm`）、`find_package` / pkg-config 文件，以及 `BqLog_CategoryLogGenerator`、`BqLog_LogDecoder` 两个工具。

本 formula 只安装 C/C++ 核心：C 与 C++ 接口以及命令行工具。Java、C#、Node.js、Python 的 wrapper 不在其中 —— 它们各自需要一份带对应语言选项编译的核心库，因此走各自的包管理器发行。各语言的获取渠道见[集成指南](../../docs/INTEGRATION_GUIDE_CHS.md)。

```cmake
find_package(BqLog REQUIRED)
target_link_libraries(app PRIVATE bqlog::BqLogShared)   # 或 bqlog::BqLogStatic
```

## 发布

`Formula/bqlog.rb` 里的 `url` 与 `sha256` 在仓库中是占位符：它们属于一个还不存在的 tag，所以没填的 formula 会"取包失败"，而不会装错源码。发布流程会把填好新 tag 的 url 与 sha256 的 `bqlog.rb`，连同 vcpkg 的 port，一起上传为本次发布 workflow 的 artifact：`package_manager_files_<版本>`，在 Actions 页面下载（不再挂到 GitHub Release 上）。以那一份提交到 homebrew-core，路径是 `Formula/b/bqlog.rb`。

Homebrew 不允许按路径安装 formula（`brew install ./bqlog.rb` 会报 "Homebrew requires formulae to be in a tap"），所以这个文件要先放进本地 homebrew-core 的检出目录——也就是 `brew tap --force homebrew/core` 写入的那个目录——才能验证：

```bash
export HOMEBREW_NO_INSTALL_FROM_API=1
brew install --build-from-source bqlog
brew test bqlog
brew audit --new --strict --online bqlog
brew style bqlog
```

本地可用 `bump.sh <版本> [源码包]` 填入同样的值。
