# BqLog 的 NuGet 包

[English](./README.md)

`BqLog` 包：`wrapper/csharp/` 中的 C# 绑定，以及 `win-x64`、`win-arm64`、`linux-x64`、`linux-arm64`、`osx-x64`、`osx-arm64` 的原生库。

```bash
dotnet add package BqLog
```

## 本地打包

```bash
bash pack/nuget/collect_runtimes.sh <artifacts_dir> pack/nuget
dotnet pack pack/nuget/BqLog.csproj -c Release -o out
```

`<artifacts_dir>` 放下载好的各平台产物（`windows_x86_64_libs_*`、`linux_x86_64_libs_*` 等）。`collect_runtimes.sh` 会把原生库放到 `runtimes/<rid>/native/`，文件名就是绑定加载的 `BqLog`。

托管程序集叫 `BqLog.Managed.dll`，这样 `dotnet publish -r <rid>` 时不会和原生的 `BqLog.dll` 冲突。

## 发布

`.github/workflows/release.yml` 里的 `publish_nuget` 任务通过 nuget.org 的 Trusted Publishing 打包并推送。需要在 nuget.org 上为 `Tencent/BqLog` / `release.yml` 配置 trusted publishing policy，并在仓库里设置 `NUGET_USER` secret（账号名）。
