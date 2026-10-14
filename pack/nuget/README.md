# NuGet package for BqLog

[简体中文](./README_CHS.md)

Builds the `BqLog` package: the C# bindings from `wrapper/csharp/` plus the prebuilt native
library for each supported platform.

```
pack/nuget/
├── BqLog.csproj          # the package project
├── PACKAGE_README.md     # becomes README.md inside the package
├── collect_runtimes.sh   # assembles runtimes/<rid>/native/ from the platform artifacts
├── README.md / README_CHS.md
└── runtimes/             # generated, not committed
```

## Native libraries

`collect_runtimes.sh` copies six libraries out of the desktop artifacts:

| RID | File in the package | Source artifact |
| --- | --- | --- |
| `win-x64` | `BqLog.dll` | `windows_x86_64_libs_*` |
| `win-arm64` | `BqLog.dll` | `windows_arm64_libs_*` |
| `linux-x64` | `libBqLog.so` | `linux_x86_64_libs_*` |
| `linux-arm64` | `libBqLog.so` | `linux_arm64_libs_*` |
| `osx-x64` | `libBqLog.dylib` | `macos_universal_binary_libs_*` |
| `osx-arm64` | `libBqLog.dylib` | `macos_universal_binary_libs_*` |

Two details matter here:

- The names are not free: the bindings ask the runtime for `"BqLog"`
  (`wrapper/csharp/src/bq/lib_def.cs`), so each platform needs exactly that file name in
  `runtimes/<rid>/native/`.
- Linux and macOS ship the library as a versioned real file plus a symbolic link. The script
  copies the real file and renames it, because NuGet flattens symlinks into empty files.
- The managed assembly is `BqLog.Managed.dll`, not `BqLog.dll`. `dotnet publish -r win-x64` and
  friends copy the managed assembly and `runtimes/win-*/native/BqLog.dll` into the same output
  directory, and the SDK refuses two files with one relative path (`NETSDK1152`). The native name
  cannot give way — `DllImport("BqLog")` and the engine plugins both expect it — so the managed
  one carries the suffix.

Where the native library ends up depends on the build: without a runtime identifier the SDK keeps
`runtimes/<rid>/native/` nested and the host picks the right one through `deps.json`, while
`dotnet publish -r <rid>` flattens that RID's library next to the managed assembly.

The Windows file comes from the MSVC build, not the MinGW one: a .NET process already has the
MSVC runtime available.

## Building the package locally

```bash
bash pack/nuget/collect_runtimes.sh <artifacts_dir> pack/nuget
dotnet pack pack/nuget/BqLog.csproj -c Release -p:Version=2.7.0 -o out
```

`<artifacts_dir>` is a directory holding the downloaded platform artifacts; the script looks
for directories named `windows_x86_64_libs_*`, `linux_x86_64_libs_*` and so on anywhere
beneath it.

## Publishing

The `publish_nuget` job in `.github/workflows/release.yml` collects the artifacts, runs the
script, packs and pushes. It publishes through nuget.org
[Trusted Publishing](https://learn.microsoft.com/nuget/nuget-org/trusted-publishing): the job
asks GitHub for an OIDC token, `NuGet/login@v1` exchanges it for an API key that lives one hour,
and that key does the push. Nothing long-lived is stored, and the key reaches only what the
policy allows.

Two things have to exist before the first release:

- a trusted publishing policy on nuget.org, owned by the account that owns the package, with
  Repository Owner `Tencent`, Repository `BqLog`, Workflow File `release.yml`, and the glob
  pattern `BqLog` under the `Push new packages and package versions` scope. That scope, not
  `Push only new package versions`, is what lets the very first push create the package;
- a `NUGET_USER` repository secret holding that account name. It is a username, not a
  credential — `NuGet/login@v1` simply takes it as input.

The login step sits immediately before the push on purpose: the key expires after an hour, and
one OIDC token yields exactly one key.

## Before the first push: is the id free?

A package id on nuget.org is permanent once used, and there is no reservation mechanism, so
check and then claim it early. Ids are case-insensitive, so always query the lowercase form:

```bash
curl -sI https://api.nuget.org/v3-flatcontainer/bqlog/index.json | head -1
# 404 -> free, 200 -> taken

# or with the .NET SDK
dotnet package search BqLog --exact-match
```

## Notes

- `netstandard2.0` is deliberate: it covers .NET Framework 4.6.1+, .NET Core 2.0+ and .NET 5+,
  and the bindings need nothing newer. The audience is .NET applications; engines keep their own
  package channels (Unity via UPM or a plugin tarball, Unreal via its plugin), so this package
  neither targets nor replaces them.
- The macOS universal library is copied into both `osx-*` RIDs unchanged — it loads on either
  architecture.
