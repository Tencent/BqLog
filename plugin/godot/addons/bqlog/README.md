# BqLog for Godot 4

[English](#english) | [中文](#中文)

---

## English

BqLog is a lightweight, high-performance, industrial-grade logging system developed by Tencent. This add-on is a GDExtension that brings BqLog to Godot 4 and works from GDScript, C# (.NET) and C++ (`godot-cpp`). All three languages share the same in-process logger instances.

### Requirements

- Godot 4.2 or later.
- Supported platforms: Windows (x86_64, arm64), macOS (universal), Linux (x86_64, arm64), Android (arm64-v8a, armeabi-v7a, x86_64, x86), HarmonyOS / OpenHarmony (arm64-v8a, armeabi-v7a, x86_64) and iOS.
- Web (HTML5) export is not supported.

### Installation

1. Copy the `addons/bqlog` folder into your project, so that it lives at `res://addons/bqlog`.
2. Reopen the project so that Godot registers `bqlog.gdextension`.

### Quick Start (GDScript)

```gdscript
var config := """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
    appenders_config.file.type=compressed_file
    appenders_config.file.file_name=game
    appenders_config.file.levels=[all]
"""
var logger := bq_log.create_log("game_log", config)
logger.info("Hello from BqLog! value: {}, {}", "text", 123)
logger.force_flush()
```

In the Godot editor, console output is redirected to the Output panel automatically. In exported builds, BqLog writes to the native platform channels (stdout, Logcat, NSLog).

### Documentation

- Project home and full documentation: https://github.com/Tencent/BqLog
- Godot integration guide: https://github.com/Tencent/BqLog/blob/develop/docs/ENGINE_INTEGRATION.md
- Issue tracker: https://github.com/Tencent/BqLog/issues

### License

Apache License 2.0. See `LICENSE.txt` in this folder.

---

## 中文

BqLog 是腾讯开发的轻量级、高性能、工业级日志系统。本插件是一个 GDExtension,把 BqLog 带到 Godot 4,支持 GDScript、C#(.NET)和 C++(`godot-cpp`),三种语言共享同一个进程内的日志实例。

### 环境要求

- Godot 4.2 及以上版本。
- 支持平台:Windows(x86_64、arm64)、macOS(universal)、Linux(x86_64、arm64)、Android(arm64-v8a、armeabi-v7a、x86_64、x86)、鸿蒙 / OpenHarmony(arm64-v8a、armeabi-v7a、x86_64)以及 iOS。
- 暂不支持 Web(HTML5)导出。

### 安装

1. 将 `addons/bqlog` 文件夹复制到你的项目中,路径为 `res://addons/bqlog`。
2. 重新打开项目,Godot 会自动注册 `bqlog.gdextension`。

### 快速开始(GDScript)

```gdscript
var config := """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
    appenders_config.file.type=compressed_file
    appenders_config.file.file_name=game
    appenders_config.file.levels=[all]
"""
var logger := bq_log.create_log("game_log", config)
logger.info("Hello from BqLog! value: {}, {}", "text", 123)
logger.force_flush()
```

在 Godot 编辑器中,控制台输出会自动重定向到 Output 面板;导出的运行时包则直接输出到平台原生通道(stdout、Logcat、NSLog)。

### 文档

- 项目主页与完整文档:https://github.com/Tencent/BqLog
- Godot 集成指南:https://github.com/Tencent/BqLog/blob/develop/docs/ENGINE_INTEGRATION_CHS.md
- 问题反馈:https://github.com/Tencent/BqLog/issues

### 许可证

Apache License 2.0,详见本目录下的 `LICENSE.txt`。
