# BqLog Node.js Binding

Node.js binding for BqLog - High-performance logging library using Node API (N-API).

## 概述 (Overview)

这个Node.js绑定使用Node API (N-API)技术，可以同时支持Node.js和鸿蒙(HarmonyOS)平台，实现一套代码通用的目标。

This Node.js binding uses Node API (N-API) technology, which supports both Node.js and HarmonyOS platforms, achieving the goal of universal code.

## 主要特性 (Key Features)

- **Node API (N-API) 支持**: ABI稳定，跨Node.js版本兼容
- **鸿蒙系统支持**: 同样的代码可以在HarmonyOS中运行
- **高性能**: 直接调用BqLog C++核心，无额外开销
- **TypeScript支持**: 完整的类型定义
- **异步回调**: 支持console输出回调处理
- **内存管理**: 自动垃圾回收集成

## 架构优势 (Architecture Benefits)

### 为什么选择Node API？(Why Choose Node API?)

1. **一套代码通用 (Universal Code)**:
   - Node.js环境：服务器端、工具脚本
   - HarmonyOS环境：鸿蒙应用的Node.js运行时

2. **ABI稳定性 (ABI Stability)**:
   - 无需为不同Node.js版本重新编译
   - 减少维护成本

3. **跨平台兼容 (Cross-platform)**:
   - Windows, macOS, Linux
   - HarmonyOS (通过Node.js运行时)

## 安装 (Installation)

### Node.js环境

```bash
npm install bqlog-nodejs
```

### HarmonyOS环境

```bash
# 在HarmonyOS项目中安装
npm install bqlog-nodejs
```

## 快速开始 (Quick Start)

### 基本使用 (Basic Usage)

```javascript
const { BqLog, LogLevel } = require('bqlog-nodejs');

// 配置日志系统
const config = JSON.stringify({
    "appenders_config": [
        {
            "type": "console",
            "name": "console_appender",
            "levels": ["all"],
            "enable": true
        },
        {
            "type": "file",
            "name": "file_appender", 
            "file_name": "./logs/app.log",
            "levels": ["all"],
            "enable": true
        }
    ]
});

// 创建日志对象
const logger = BqLog.createLog('MyApp', config, ['General', 'Network']);

// 记录日志
logger.info(0, 'Application started');
logger.error(1, 'Network error: {0}', error.message);
```

### 鸿蒙系统使用 (HarmonyOS Usage)

```javascript
const { BqLog } = require('bqlog-nodejs');

// HarmonyOS优化配置
const harmonyConfig = JSON.stringify({
    "appenders_config": [
        {
            "type": "file",
            "name": "harmony_file",
            "file_name": "/data/storage/el2/base/files/logs/app.log",
            "levels": ["all"],
            "enable": true
        }
    ]
});

const logger = BqLog.createLog('HarmonyApp', harmonyConfig, ['UI', 'Business']);
logger.info(0, 'HarmonyOS app started');
```

## API文档 (API Documentation)

### 静态方法 (Static Methods)

#### `BqLog.getVersion(): string`
获取BqLog版本信息

#### `BqLog.createLog(name, config, categories?): BqLog | null`
创建日志对象
- `name`: 日志名称
- `config`: JSON配置字符串
- `categories`: 分类数组(可选)

#### `BqLog.getLogByName(name): BqLog | null`
根据名称获取已存在的日志对象

#### `BqLog.enableAutoCrashHandler(): void`
启用自动崩溃处理

### 实例方法 (Instance Methods)

#### 日志记录方法 (Logging Methods)
- `verbose(categoryIdx, message, ...args)`: 详细日志
- `debug(categoryIdx, message, ...args)`: 调试日志
- `info(categoryIdx, message, ...args)`: 信息日志
- `warning(categoryIdx, message, ...args)`: 警告日志
- `error(categoryIdx, message, ...args)`: 错误日志
- `fatal(categoryIdx, message, ...args)`: 致命错误日志

#### 控制方法 (Control Methods)
- `forceFlush()`: 强制刷新日志缓冲区
- `setAppendersEnable(name, enable)`: 启用/禁用输出器
- `takeSnapshotString(useGMT?)`: 获取日志快照

## 配置说明 (Configuration)

### 基本配置结构

```json
{
  "appenders_config": [
    {
      "type": "console",
      "name": "console_appender",
      "levels": ["all"],
      "enable": true
    },
    {
      "type": "file",
      "name": "file_appender",
      "file_name": "./logs/app.log",
      "levels": ["info", "warning", "error", "fatal"],
      "enable": true,
      "max_file_size": "10MB",
      "max_file_count": 5
    }
  ],
  "global_config": {
    "buffer_size": "2MB",
    "thread_mode": "async"
  }
}
```

## 示例项目 (Examples)

### Node.js服务器日志

```javascript
// server.js
const express = require('express');
const { BqLog } = require('bqlog-nodejs');

const app = express();
const logger = BqLog.createLog('WebServer', config, ['HTTP', 'DB', 'Auth']);

app.use((req, res, next) => {
    logger.info(0, 'HTTP {0} {1}', req.method, req.url);
    next();
});

app.listen(3000, () => {
    logger.info(0, 'Server started on port 3000');
});
```

### HarmonyOS应用日志

```javascript
// harmony_app.js
const { BqLog } = require('bqlog-nodejs');

class HarmonyLogger {
    constructor() {
        this.logger = BqLog.createLog('HarmonyApp', harmonyConfig, 
            ['UI', 'Business', 'Network']);
    }
    
    logUIEvent(message) {
        this.logger.info(0, `[UI] ${message}`);
    }
    
    logBusinessLogic(message) {
        this.logger.info(1, `[Business] ${message}`);
    }
}

const harmonyLogger = new HarmonyLogger();
harmonyLogger.logUIEvent('Page loaded');
```

## 性能优化 (Performance Optimization)

### 异步日志模式
BqLog默认使用异步模式，大幅提升性能：

```javascript
const config = JSON.stringify({
    "global_config": {
        "thread_mode": "async",
        "buffer_size": "4MB"
    }
});
```

### 内存管理
使用完毕后及时清理：

```javascript
// 应用退出前
logger.forceFlush();
BqLog.uninit();
```

## 平台特定注意事项 (Platform-specific Notes)

### Node.js环境
- 支持所有Node.js v10+版本
- 自动处理跨平台编译

### HarmonyOS环境
- 需要HarmonyOS SDK 8+
- 文件路径使用HarmonyOS沙盒路径
- 注意应用生命周期管理

## 编译说明 (Build Instructions)

### 开发环境要求
- Node.js 10+
- Python 3.6+
- C++17编译器
- cmake 3.15+

### 编译步骤

```bash
# 安装依赖
npm install

# 编译native模块
npm run build

# 运行测试
npm test
```

### HarmonyOS编译

```bash
# 设置HarmonyOS NDK环境
export HARMONY_NDK_ROOT=/path/to/harmony/ndk

# 编译
npm run build:harmony
```

## 常见问题 (FAQ)

### Q: Node API相比JNI有什么优势？
A: 
- **ABI稳定**: 无需为不同版本重新编译
- **内存安全**: 自动垃圾回收集成
- **跨平台**: 同一代码支持多个平台
- **维护简单**: 减少平台特定代码

### Q: 在HarmonyOS中如何使用？
A: HarmonyOS支持Node.js运行时，可以直接使用相同的Node API绑定。

### Q: 性能如何？
A: 直接调用C++核心，性能与原生C++接口基本一致。

## 贡献指南 (Contributing)

欢迎贡献代码和建议！请参考[贡献指南](../../CONTRIBUTING.md)。

## 许可证 (License)

Apache License 2.0 - 详见[LICENSE](../../LICENSE.txt)文件。

## 支持 (Support)

- GitHub Issues: [报告问题](https://github.com/Tencent/BqLog/issues)
- 文档: [完整文档](../../README_CHS.md)
- 示例: [更多示例](./examples/)