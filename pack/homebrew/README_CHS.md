# BqLog 的 Homebrew formula

[English](./README.md)

homebrew-core 中 `bqlog` formula 的源文件。

```bash
brew install bqlog
```

会安装静态库和动态库、头文件（含 C++20 模块 `bq_log/bq.log.cppm`）、`find_package` / pkg-config 文件，以及 `BqLog_CategoryLogGenerator`、`BqLog_LogDecoder` 两个工具。

```cmake
find_package(BqLog REQUIRED)
target_link_libraries(app PRIVATE bqlog::BqLogShared)   # 或 bqlog::BqLogStatic
```

## 发布

`Formula/bqlog.rb` 里的 `url` 与 `sha256` 在仓库中是占位符：它们属于一个还不存在的 tag，所以没填的 formula 会"取包失败"，而不会装错源码。发布流程会把填好新 tag 的 url 与 sha256 的 `bqlog.rb` 挂到 GitHub Release 上，以那一份提交到 homebrew-core 即可：

```bash
brew install --build-from-source ./bqlog.rb
brew test bqlog
brew audit --new --strict --online bqlog
```

本地可用 `bump.sh <版本> [源码包]` 填入同样的值。
