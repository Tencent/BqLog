# BqLog 的 vcpkg port

[English](./README.md)

microsoft/vcpkg 中 `bqlog` port 的源文件。

```bash
vcpkg install bqlog
vcpkg install bqlog[tools]   # 同时安装命令行工具
```

静态库还是动态库由 triplet 决定。会安装头文件（含 C++20 模块 `bq_log/bq.log.cppm`）、库以及 `find_package` / pkg-config 文件；`tools` feature 会把 `BqLog_CategoryLogGenerator` 和 `BqLog_LogDecoder` 装到 `tools/bqlog/`。

```cmake
find_package(BqLog CONFIG REQUIRED)
target_link_libraries(main PRIVATE bqlog::BqLog)
```

## 发布

`ports/bqlog/` 里的 `version` 与 `SHA512` 在仓库中是占位符：它们属于一个还不存在的 tag，所以没填的 port 会"取包失败"，而不会装错源码。发布流程会把填好新 tag 版本号与 SHA512 的 `vcpkg_port_bqlog.tar.gz` 挂到 GitHub Release 上。解压那一份到 microsoft/vcpkg fork 的 `ports/bqlog/`，运行 `vcpkg x-add-version bqlog` 后提 PR。

本地可用 `bump.sh <版本> [源码包]` 填入同样的值。
