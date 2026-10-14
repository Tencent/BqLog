# BqLog 的 NuGet 包

[English](./README.md)

构建 `BqLog` 包：`wrapper/csharp/` 里的 C# 绑定，加上各支持平台的预编译原生库。

```
pack/nuget/
├── BqLog.csproj          # 包工程
├── PACKAGE_README.md     # 会成为包内的 README.md
├── collect_runtimes.sh   # 从各平台 artifact 组装 runtimes/<rid>/native/
├── README.md / README_CHS.md
└── runtimes/             # 生成物，不入库
```

## 原生库

`collect_runtimes.sh` 从桌面产物里取出六个库：

| RID | 包内文件名 | 来源 artifact |
| --- | --- | --- |
| `win-x64` | `BqLog.dll` | `windows_x86_64_libs_*` |
| `win-arm64` | `BqLog.dll` | `windows_arm64_libs_*` |
| `linux-x64` | `libBqLog.so` | `linux_x86_64_libs_*` |
| `linux-arm64` | `libBqLog.so` | `linux_arm64_libs_*` |
| `osx-x64` | `libBqLog.dylib` | `macos_universal_binary_libs_*` |
| `osx-arm64` | `libBqLog.dylib` | `macos_universal_binary_libs_*` |

两个细节很关键：

- 文件名不能随意取：绑定层向运行时请求的是 `"BqLog"`
  （`wrapper/csharp/src/bq/lib_def.cs`），所以每个平台必须恰好是这个名字，放在
  `runtimes/<rid>/native/` 下。
- Linux 与 macOS 的产物是「带版本号的真实文件 + 符号链接」。脚本复制真实文件并重命名，
  因为 NuGet 会把符号链接展平成空文件。
- 托管程序集叫 `BqLog.Managed.dll`，不叫 `BqLog.dll`。`dotnet publish -r win-x64` 这类带 RID 的
  发布会把托管程序集和 `runtimes/win-*/native/BqLog.dll` 拷到同一个输出目录，SDK 会因为两个
  文件相对路径相同而直接报 `NETSDK1152`。原生库的名字让不了——`DllImport("BqLog")` 和各引擎
  插件都按这个名字找——所以由托管侧加后缀。

原生库最终落在哪儿取决于构建方式：不带 RID 时 SDK 保留 `runtimes/<rid>/native/` 的嵌套结构，
由 host 按 `deps.json` 选择；`dotnet publish -r <rid>` 则把该 RID 的库拍平到托管程序集旁边。

Windows 用的是 MSVC 产物而非 MinGW 产物：.NET 进程本身已经带有 MSVC 运行时。

## 本地打包

```bash
bash pack/nuget/collect_runtimes.sh <artifacts_dir> pack/nuget
dotnet pack pack/nuget/BqLog.csproj -c Release -p:Version=2.7.0 -o out
```

`<artifacts_dir>` 是存放已下载各平台 artifact 的目录；脚本会在其下查找名为
`windows_x86_64_libs_*`、`linux_x86_64_libs_*` 之类的目录。

## 发布

`.github/workflows/release.yml` 里的 `publish_nuget` job 会收集产物、执行脚本、打包并推送。
它走 nuget.org 的
[Trusted Publishing](https://learn.microsoft.com/nuget/nuget-org/trusted-publishing)：job 先向
GitHub 索取 OIDC token，由 `NuGet/login@v1` 换成一把只活 1 小时的 API key，再用它推送。仓库里
不存任何长期凭据，而且这把 key 只能触达策略允许的范围。

首次发布之前需要存在两样东西：

- nuget.org 上的一条 trusted publishing 策略，其 Package Owner 即包的归属账号，填
  Repository Owner `Tencent`、Repository `BqLog`、Workflow File `release.yml`，并在
  `Push new packages and package versions` 这个 scope 下填 glob `BqLog`。必须选这个 scope 而不是
  `Push only new package versions`，因为首次推送要创建这个包；
- 一个 `NUGET_USER` 仓库 secret，值为该账号名。它是用户名而不是凭据——`NuGet/login@v1` 只是拿它
  当输入。

login 步骤紧贴 push 是有意为之：key 一小时过期，且一个 OIDC token 只能换一把 key。

## 首次推送前：这个 ID 空着吗？

nuget.org 上的包 ID 一旦被使用就永久占用，而且没有预留机制，所以要尽早检查并占位。ID 不区分
大小写，查询时一律用小写形式：

```bash
curl -sI https://api.nuget.org/v3-flatcontainer/bqlog/index.json | head -1
# 404 -> 未占用，200 -> 已被占用

# 或者用 .NET SDK
dotnet package search BqLog --exact-match
```

## 说明

- `netstandard2.0` 是有意选择的：它覆盖 .NET Framework 4.6.1+、.NET Core 2.0+ 与 .NET 5+，
  而绑定层不需要更新的特性。受众是 .NET 应用；各引擎有自己的分发渠道（Unity 走 UPM 或插件
  压缩包，Unreal 走插件包），本包既不面向它们，也不替代它们。
- macOS 的 universal 库原样放进两个 `osx-*` RID —— 它在两种架构上都能加载。
