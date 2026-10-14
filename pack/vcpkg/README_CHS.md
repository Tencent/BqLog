# BqLog 的 vcpkg port

[English](./README.md)

这是一个 overlay port：vcpkg 用户可以直接从仓库里安装 BqLog，同时也让这个 port 在提交到
`microsoft/vcpkg` 之前能在这里先验证。

```
pack/vcpkg/ports/bqlog/
├── portfile.cmake
└── vcpkg.json
```

## 用 overlay 验证

```bash
vcpkg install bqlog --overlay-ports=pack/vcpkg/ports
```

`portfile.cmake` 里的 `SHA512` 对应 `Release_<版本>` 这个 tag archive，所以 `vcpkg.json` 换版本
时要一并刷新。填错不会是静默失败：vcpkg 会停下并打印它算出的校验值，照抄即可。

已在 macOS 上用 `arm64-osx`（静态）与 `arm64-osx-dynamic` 实测：两者都装出普通的
`lib/libBqLog.a` / `lib/libBqLog.dylib` 而不是 framework，并且通过 vcpkg toolchain 构建的
消费者能找到并运行。

`vcpkg.json` 里的 `version` 要与本目录所对应的发布版本保持一致：portfile 用它拼出 `REF`。

## 静态还是动态

vcpkg 通过 triplet 决定库形态（`x64-windows` 是动态、`x64-windows-static` 是静态），port 据此
传对应的 `BUILD_LIB_TYPE`。一个 port 无法同时安装两种形态。

没有传 `TARGET_PLATFORM`：构建时由 CMake 内置变量自动识别。

在 Apple 上还会按 triplet 传 `APPLE_LIB_FORMAT`（`a` 或 `dylib`）。不传的话，单形态构建会产出
`BqLog.framework`，vcpkg 和它要改写的 pkg-config 文件都消费不了。

## port 装了什么

头文件（含 C++20 模块接口单元 `bq_log/bq.log.cppm`）、库，以及包发现文件：
`BqLogConfig.cmake`（供 `find_package(BqLog)`，vcpkg 会把它挪到 `share/bqlog`）与
`lib/pkgconfig/bqlog.pc`（供 pkg-config）。命令行工具不在这个 port 里。

一个 vcpkg 构建只有一种库形态，因此导出的目标是 `bqlog::BqLog`；Homebrew 包同时装两种形态，
对应的是 `bqlog::BqLogShared` 与 `bqlog::BqLogStatic`。

## 提交到 vcpkg

1. 至少在 `x64-windows` 与 `x64-linux` 上验证上面的 overlay 安装。
2. fork `microsoft/vcpkg`，加入 `ports/bqlog/{portfile.cmake,vcpkg.json}`。
3. 运行 `vcpkg x-add-version bqlog` 生成版本数据库条目。
4. 提 PR。他们的 CI 会在所有 triplet 上构建，合入由他们的 reviewer 决定。
