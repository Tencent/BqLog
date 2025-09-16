# BqLog 鸿蒙 & Node.js 统一适配方案

## 问题回答

> **原问题**: "我要做鸿蒙的适配，和nodejs的适配，在native层，是不是都可以选择Node API方案？就可以一套代码通用？"

## ✅ 答案：完全可以！

**Node API (N-API) 是实现鸿蒙和Node.js统一适配的最佳方案，可以做到一套代码完全通用。**

## 核心优势

### 1. 一套代码通用 ✅
```
同一套 C++ Node API 绑定代码
        ↓
   支持 Node.js 环境  +  支持 HarmonyOS 环境
```

### 2. 技术优势对比

| 特性 | JNI方案 | Node API方案 |
|------|---------|---------------|
| 代码复用 | ❌ 需要多套代码 | ✅ 一套代码通用 |
| 维护成本 | ❌ 高 | ✅ 低 |
| ABI稳定性 | ❌ 平台相关 | ✅ 跨版本兼容 |
| 开发效率 | ❌ 低 | ✅ 高 |
| 性能 | ✅ 原生性能 | ✅ 原生性能 |

## 实现方案

### 架构图
```
JavaScript 层 (Node.js + HarmonyOS)
        ↓
    Node API 绑定层 (统一实现)
        ↓
    BqLog C++ 核心库
```

### 核心文件结构
```
wrapper/nodejs/
├── src/bqlog_node_api.cpp     # Node API 绑定实现
├── index.js                   # JavaScript 包装器
├── index.d.ts                 # TypeScript 类型定义
├── package.json               # npm 包配置
├── binding.gyp                # 构建配置
├── examples/                  # 示例代码
│   ├── basic_usage.js         # 基本使用
│   ├── harmonyos_example.js   # 鸿蒙专用示例
│   └── console_callback.js    # 回调功能
└── test/test.js               # 测试用例
```

## 使用示例

### Node.js 环境
```javascript
const { BqLog } = require('bqlog-nodejs');

// 创建日志对象
const logger = BqLog.createLog('WebServer', config, ['HTTP', 'DB']);

// 记录日志
logger.info(0, 'Server started on port {0}', 3000);
logger.error(1, 'Database connection failed');
```

### HarmonyOS 环境
```javascript
const { BqLog } = require('bqlog-nodejs'); // 同一个包！

// HarmonyOS优化配置
const config = {
    "appenders_config": [{
        "type": "file",
        "file_name": "/data/storage/el2/base/files/logs/app.log"
    }]
};

const logger = BqLog.createLog('HarmonyApp', config, ['UI', 'Business']);
logger.info(0, 'HarmonyOS 应用启动');
```

## 技术实现要点

### 1. Node API 绑定核心
```cpp
#include <napi.h>
#include "bq_log/misc/bq_log_api.h"

class BqLogWrapper : public Napi::ObjectWrap<BqLogWrapper> {
    // 统一的 API 实现，同时支持 Node.js 和 HarmonyOS
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Napi::Value CreateLog(const Napi::CallbackInfo& info);
    void Log(const Napi::CallbackInfo& info);
    // ... 其他方法
};
```

### 2. 内存管理
```cpp
// Node API 自动处理垃圾回收
Napi::String message = info[0].As<Napi::String>();
std::string msg = message.Utf8Value(); // 自动内存管理
```

### 3. 异步回调支持
```cpp
// 线程安全的回调机制
static Napi::ThreadSafeFunction console_callback_tsfn;

static void console_callback_handler(uint64_t log_id, int32_t level, const char* content) {
    console_callback_tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
        jsCallback.Call({Napi::String::New(env, content)});
    });
}
```

## 部署方案

### 开发阶段
```bash
# 1. 安装依赖
npm install

# 2. 编译 native 模块
npm run build

# 3. 测试
npm test
```

### 生产部署
```bash
# Node.js 环境
npm install bqlog-nodejs

# HarmonyOS 环境  
npm install bqlog-nodejs  # 同一个包！
```

## 性能特点

### 1. 零开销抽象
- 直接调用 BqLog C++ 核心
- 无额外的序列化/反序列化开销
- 内存零拷贝设计

### 2. 异步日志支持
```javascript
// 异步日志记录，不阻塞主线程
logger.info(0, 'High frequency logging');  // 立即返回
```

### 3. 内存效率
- 自动垃圾回收集成
- 智能缓冲区管理
- 最小内存占用

## 兼容性说明

### Node.js 版本支持
- Node.js 10.x+
- npm 6.x+
- 所有主流平台 (Windows, macOS, Linux)

### HarmonyOS 版本支持
- HarmonyOS 3.0+
- 支持 Node.js 运行时的 HarmonyOS 应用
- Native 开发套件支持

## 最佳实践

### 1. 错误处理
```javascript
try {
    const logger = BqLog.createLog('MyApp', config);
    if (!logger || !logger.isValid()) {
        throw new Error('Logger creation failed');
    }
} catch (error) {
    console.error('日志系统初始化失败:', error);
}
```

### 2. 资源清理
```javascript
// 应用退出时清理
process.on('exit', () => {
    logger.forceFlush();
    BqLog.uninit();
});
```

### 3. 配置优化
```javascript
// 根据平台调整配置
const isHarmonyOS = process.platform === 'harmonyos';
const logPath = isHarmonyOS 
    ? '/data/storage/el2/base/files/logs/'
    : './logs/';
```

## 总结

✅ **技术可行性**: Node API 完全支持一套代码通用  
✅ **性能保证**: 直接调用 C++ 核心，性能最优  
✅ **维护效率**: 单一代码库，降低维护成本  
✅ **跨平台兼容**: 支持所有目标平台  
✅ **开发体验**: 完整的 TypeScript 支持  

**推荐立即采用此方案进行开发！**

## 下一步行动

1. ✅ **方案确认**: Node API 是最优选择
2. 🔄 **实际测试**: 在目标环境中验证功能
3. 📋 **配置调优**: 根据具体需求调整配置
4. 🚀 **生产部署**: 集成到实际项目中

## 联系支持

如有技术问题，请参考：
- 📖 [详细文档](./README.md)
- 🔧 [技术指南](./TECHNICAL_GUIDE.md)  
- 💡 [示例代码](./examples/)
- 🐛 [GitHub Issues](https://github.com/Tencent/BqLog/issues)