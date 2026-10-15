# BqLog

.NET bindings for the [BqLog](https://github.com/Tencent/BqLog) logging library, with the native library for every supported platform included.

```csharp
bq.log log = bq.log.create_log("my_log", "appenders_config.console.type=console");
log.info("Hello from BqLog");
```

Platforms: `win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-x64`, `osx-arm64`. Target framework: `netstandard2.0`, which also covers .NET Framework 4.6.1+ on Windows (x64 or AnyCPU; a 32-bit Windows process is not supported, since no 32-bit native library ships).

- [Integration guide](https://github.com/Tencent/BqLog/blob/main/docs/INTEGRATION_GUIDE.md)
- [API reference](https://github.com/Tencent/BqLog/blob/main/docs/API_REFERENCE.md)
