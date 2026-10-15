# NuGet package for BqLog

[简体中文](./README_CHS.md)

The `BqLog` package: the C# bindings from `wrapper/csharp/` plus the native library for `win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-x64` and `osx-arm64`.

```bash
dotnet add package BqLog
```

## Building locally

```bash
bash pack/nuget/collect_runtimes.sh <artifacts_dir> pack/nuget
dotnet pack pack/nuget/BqLog.csproj -c Release -o out
```

`<artifacts_dir>` holds the downloaded platform artifacts (`windows_x86_64_libs_*`, `linux_x86_64_libs_*`, ...). `collect_runtimes.sh` places each native library under `runtimes/<rid>/native/` with the name the bindings load (`BqLog`).

The managed assembly is `BqLog.Managed.dll`, so that it never collides with the native `BqLog.dll` in a `dotnet publish -r <rid>` output.

## Publishing

The `publish_nuget` job in `.github/workflows/release.yml` packs and pushes through nuget.org Trusted Publishing. It needs a trusted publishing policy for `Tencent/BqLog` / `release.yml` on nuget.org and a `NUGET_USER` repository secret with the account name.
