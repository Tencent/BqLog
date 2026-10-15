# NuGet package for BqLog

[简体中文](./README_CHS.md)

The `BqLog` package: the C# bindings from `wrapper/csharp/` plus the native library for `win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-x64` and `osx-arm64`.

```bash
dotnet add package BqLog
```

## Building locally

```bash
bash pack/nuget/collect_runtimes.sh <artifacts_dir> pack/nuget
dotnet pack pack/nuget/BqLog.csproj -c Release -p:Version=<version> -o out
```

`<artifacts_dir>` holds the downloaded platform artifacts (`windows_x86_64_libs_*`, `linux_x86_64_libs_*`, ...). `collect_runtimes.sh` places each native library under `runtimes/<rid>/native/` with the name the bindings load (`BqLog`).

The version is never written into `BqLog.csproj`: it holds a `0.0.0` placeholder and every build passes `-p:Version=<version>` taken from `src/bq_log/global/version.cpp`.

The managed assembly is `BqLog.Managed.dll`, so that it never collides with the native `BqLog.dll` in a `dotnet publish -r <rid>` output.

## .NET Framework

.NET Core and .NET 5+ resolve `runtimes/<rid>/native/` themselves. .NET Framework does not, so the package carries `buildTransitive/BqLog.targets`, which copies the native library next to the assembly for a `.NETFramework` target (`win-x64` for `x64`/`AnyCPU`, `win-arm64` for `arm64`; there is no 32-bit native, so an `x86` build fails with `BQLOG0001`).

A legacy `packages.config` project imports neither `runtimes/` nor `buildTransitive/`, so it has to copy `runtimes/win-x64/native/BqLog.dll` next to its assembly itself.

## Publishing

The `publish_nuget` job in `.github/workflows/release.yml` packs and pushes through nuget.org Trusted Publishing. It needs a trusted publishing policy for `Tencent/BqLog` / `release.yml` on nuget.org and a `NUGET_USER` repository secret with the account name.
