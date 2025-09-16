# BqLog Node API 技术方案：鸿蒙和Node.js统一适配

## 问题解答

> **问题**: "我要做鸿蒙的适配，和nodejs的适配，在native层，是不是都可以选择Node API方案？就可以一套代码通用？"

**答案**: **是的，完全可以！** Node API (N-API) 是实现鸿蒙和Node.js统一适配的最佳方案。

## 技术方案概述

### Node API 的优势

1. **一套代码通用** ✅
   - 同一套C++代码同时支持Node.js和HarmonyOS
   - 避免重复开发JNI和其他绑定

2. **ABI稳定** ✅
   - 跨Node.js版本兼容
   - 二进制兼容性保证

3. **跨平台支持** ✅
   - Windows, macOS, Linux, HarmonyOS
   - 统一的API接口

## 架构对比

### 传统方案 (不推荐)
```
Node.js 环境     鸿蒙环境
    ↓              ↓
Node.js绑定    JNI/NAPI绑定
    ↓              ↓
  C++ Core ←--→ C++ Core
(需要维护两套绑定代码)
```

### Node API 方案 (推荐) ✅
```
Node.js 环境     鸿蒙环境
    ↓              ↓
    Node API 绑定 ←-→ (同一套代码)
         ↓
     C++ Core
(只需维护一套绑定代码)
```

## 实现细节

### 1. 核心绑定代码 (src/bqlog_node_api.cpp)

```cpp
#include <napi.h>
#include "bq_log/misc/bq_log_api.h"

// 统一的Node API绑定，同时支持Node.js和HarmonyOS
class BqLogWrapper : public Napi::ObjectWrap<BqLogWrapper> {
    // 统一的API实现
    Napi::Value CreateLog(const Napi::CallbackInfo& info);
    void Log(const Napi::CallbackInfo& info);
    // ... 其他方法
};
```

### 2. 构建配置 (binding.gyp)

```json
{
  "targets": [
    {
      "target_name": "bqlog",
      "sources": ["src/bqlog_node_api.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../../include"
      ],
      // 平台无关的配置
    }
  ]
}
```

### 3. JavaScript接口 (index.js)

```javascript
// 统一的JavaScript接口
const { BqLog } = require('./build/Release/bqlog');

// 在Node.js和HarmonyOS中都可以使用相同的API
const logger = BqLog.createLog('MyApp', config);
logger.info(0, 'Hello from both platforms!');
```

## 平台适配指南

### Node.js 环境使用

```javascript
// server.js - Node.js服务器
const { BqLog } = require('bqlog-nodejs');

const logger = BqLog.createLog('WebServer', {
    "appenders_config": [{
        "type": "file",
        "file_name": "./logs/server.log"
    }]
});

logger.info(0, 'Server started');
```

### HarmonyOS 环境使用

```javascript
// harmony_app.js - 鸿蒙应用
const { BqLog } = require('bqlog-nodejs'); // 同一个包！

const logger = BqLog.createLog('HarmonyApp', {
    "appenders_config": [{
        "type": "file",
        "file_name": "/data/storage/el2/base/files/logs/app.log" // 鸿蒙路径
    }]
});

logger.info(0, 'HarmonyOS app started');
```

## 编译部署

### Node.js 环境
```bash
npm install bqlog-nodejs
# 自动编译对应平台的native模块
```

### HarmonyOS 环境
```bash
# 在HarmonyOS项目中
npm install bqlog-nodejs
# 编译HarmonyOS平台的native模块
node-gyp rebuild --target_platform=harmonyos
```

## 性能对比

| 方案 | 开发成本 | 维护成本 | 性能 | 兼容性 |
|------|----------|----------|------|--------|
| JNI方案 | 高 | 高 | 高 | 限定平台 |
| Node API方案 | 低 | 低 | 高 | 跨平台 |

## 技术细节

### 1. 内存管理
```cpp
// Node API自动处理垃圾回收
Napi::String message = info[0].As<Napi::String>();
std::string msg = message.Utf8Value(); // 自动管理内存
```

### 2. 异步回调支持
```cpp
// 支持异步console回调
static Napi::ThreadSafeFunction console_callback_tsfn;

static void console_callback_handler(uint64_t log_id, int32_t level, const char* content) {
    // 线程安全的回调处理
    console_callback_tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
        jsCallback.Call({Napi::String::New(env, content)});
    });
}
```

### 3. 错误处理
```cpp
// 统一的错误处理机制
if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected argument").ThrowAsJavaScriptException();
    return env.Null();
}
```

## 部署策略

### 开发阶段
1. 开发统一的Node API绑定
2. 在Node.js环境中测试
3. 在HarmonyOS环境中验证

### 生产阶段
1. 发布npm包，包含预编译的二进制文件
2. 支持多平台自动选择对应的二进制
3. 提供源码编译选项

## 最佳实践

### 1. 配置管理
```javascript
// 根据平台调整配置
const config = {
    "appenders_config": [{
        "type": "file",
        "file_name": process.platform === 'harmonyos' 
            ? "/data/storage/el2/base/files/logs/app.log"  // 鸿蒙路径
            : "./logs/app.log"  // Node.js路径
    }]
};
```

### 2. 错误处理
```javascript
try {
    const logger = BqLog.createLog('MyApp', config);
    if (!logger || !logger.isValid()) {
        throw new Error('Failed to create logger');
    }
} catch (error) {
    console.error('Logger initialization failed:', error);
}
```

### 3. 资源清理
```javascript
// 应用退出时清理资源
process.on('exit', () => {
    if (logger) {
        logger.forceFlush();
    }
    BqLog.uninit();
});
```

## 总结

使用Node API方案可以：

✅ **一套代码通用**: 同时支持Node.js和HarmonyOS  
✅ **降低维护成本**: 无需维护多套绑定代码  
✅ **保证性能**: 直接调用C++核心，无额外开销  
✅ **提高兼容性**: ABI稳定，跨版本兼容  
✅ **简化部署**: 统一的npm包管理  

这是目前最优的技术方案，强烈推荐采用！

## 示例项目

完整的示例代码在 `/wrapper/nodejs/examples/` 目录中：

- `basic_usage.js` - 基本使用示例
- `harmonyos_example.js` - 鸿蒙专用示例
- `console_callback.js` - 回调功能示例

## 下一步

1. 按照本方案实现Node API绑定
2. 在Node.js环境中测试验证
3. 在HarmonyOS环境中部署测试
4. 根据需要调整配置和优化性能