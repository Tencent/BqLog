# Go 源码模块发布

开发代码继续位于 `wrapper/go`，发布模块由工具自动生成。只有
`.github/workflows/release.yml` 的发布任务会推送 `go_dist` 分支。
普通 Build / AutoTest 不发布 Go 模块，本地打包工具也不进行网络推送。

Release workflow 提供 `go_only` 开关。启用时只执行 Go 打包、验证、
发布，跳过完整 Build、常规 GitHub Release 和其他语言包发布。
Go 验证仍必须全部通过，版本直接从该提交的 BqLog version.cpp 读取。

## 安装和版本

Go 模块版本与 BqLog 版本一致。例如 BqLog 2.4.1 对应：

| 项目 | 值 |
|---|---|
| 模块路径 | `github.com/Tencent/BqLog/go/v2` |
| Go 版本 | `v2.4.1` |
| Git 标签 | `go/v2.4.1` |
| 发布分支 | `go_dist` |
| 模块目录 | 发布分支的 `go/` |

Go 规范要求 v2 及以后的模块路径带主版本后缀，因此不能在保持
`2.4.1` 版本号的同时省略 `/v2`。将来 BqLog v3 对应 `/v3`。

首次发布成功后，用户在已有 Go 项目中执行：

```sh
go get github.com/Tencent/BqLog/go/v2@latest
```

固定版本则执行：

```sh
go get github.com/Tencent/BqLog/go/v2@v2.4.1
```

代码使用：

```go
import bq "github.com/Tencent/BqLog/go/v2"
```

新项目先运行一次 `go mod init example.com/myapp`。
Go 用完整模块路径定位源码，没有 `go get BqLog` 这种裸品牌名注册机制。
安装后，选定版本被记录在用户的 go.mod；编译不会自动跟随分支变化。

路径本身不表示 main 或 go_dist。Go 通过 `go/v2.4.1` 标签找到
go_dist 上的发布提交；`@latest` 选择适用的最高正式版本。
若从未创建该模块的版本标签，不能指望 `@latest` 自动选择 go_dist。
GitHub Release 的 ZIP 附件可供手动下载，但不是默认 go get 的安装来源。

## Release 流程

1. 现有 Build 任务给出 BqLog 版本；源码版本必须与这个值一致。
2. prepare_go_module 从触发 release 的确定提交生成源码模块。生成器
   输出的分类测试文件会与仓库 fixture 比较，防止模板与测试样本漂移。
3. validate_go_module 在 Linux x64/ARM64、macOS、Windows 构造本地
   Go 模块代理，以精确版本运行 go get、模块测试和独立消费者程序。
   验证不使用指向开发仓库的 replace，也不提供预编译 BqLog。
4. 普通发布在所有验证通过后创建正常 GitHub Release；`go_only` 跳过此步。
5. publish_go 等待 Go 验证成功（普通发布还需等待 release 成功），以私有 Git index 生成分支提交，
   原子推送 `go_dist` 和 `go/v<版本>`，不切换开发 checkout。
6. 重跑相同版本、相同源码不会再次修改；不同内容不能覆盖已有标签；
   不执行 force push，不能用旧版本回退发布分支。

Go 发布流程只有 release workflow 中的一处调用，脚本还检查
GITHUB_WORKFLOW / GITHUB_EVENT_NAME；仓库分支保护仍是最终权限边界。
现有 GITHUB_TOKEN 需要 contents:write。若仓库对 go_dist 或 go/* 标签
另有保护，需允许该发布身份写入。

本次代码配置不等于远程已经存在发布版本；需实际运行 Release workflow。

## 源码打包和构建

`build/wrapper/go/module` 是无第三方依赖的 Go 打包工具：

```sh
cd build/wrapper/go/module
go run . -repo ../../../.. -out /path/to/new/module -version 2.4.1
go run . -verify /path/to/new/module
```

工具拒绝覆盖已存在的输出目录。它复制现有 Go/native 源文件到发布
产物，重写包路径和 BqLog include 路径，记录 SOURCE.json 中的版本、
来源提交及源码哈希。系统头文件不重写，开发树不产生 .cc 转接层。
各 .cpp 保持独立编译单元；头文件与源码位于 cgo 包目录，避免外层文件
变化没有触发 Go 构建缓存更新。

用户需要 Go 1.21+、CGO_ENABLED=1 和 C/C++17 编译器。普通 go build
会编译模块内 native 源码；不要求先运行 CMake 或 go generate。
发布编译配置定义 BQ_DYNAMIC_LIB、BQ_GO，并保留原来的 API 宏策略；
这表示构建 native 实现，不代表生成独立 DLL/so。native 对象被链接进
Go 程序，Git 仓库不包含预编译二进制。

开发分支仍可通过现有 CMake 动态库和 wrapper 联调。
