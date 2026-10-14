# BqLog

.NET bindings for the [BqLog](https://github.com/Tencent/BqLog) logging library. The native
library for each supported platform ships inside this package, so nothing has to be copied or
installed by hand.

```csharp
using bq;

bq.log log = bq.log.create_log("my_log", "appenders_config.console.type=console");
log.info("Hello from BqLog");
```

## Supported platforms

`win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-x64`, `osx-arm64`.

The macOS library is universal, so it runs on both Apple silicon and Intel.

The managed assembly is `BqLog.Managed.dll`, while the native library keeps its platform name
(`BqLog.dll`, `libBqLog.so`, `libBqLog.dylib`). The two names have to differ: a publish with a
runtime identifier puts both in one directory, and two files with the same path are an error.

## Target framework

`netstandard2.0`: .NET Framework 4.6.1 and newer, .NET Core 2.0 and newer, and .NET 5 and
newer.

## Documentation

- [Integration guide](https://github.com/Tencent/BqLog/blob/develop/docs/INTEGRATION_GUIDE.md)
- [API reference](https://github.com/Tencent/BqLog/blob/develop/docs/API_REFERENCE.md)
