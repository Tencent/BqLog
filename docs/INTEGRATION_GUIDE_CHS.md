# 集成指南

[← 返回首页](../README_CHS.md) | [English](./INTEGRATION_GUIDE.md)

> 以下示例假定您已在 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载对应版本的二进制包或源码。

---

## C++（动态库 / 静态库 / 源码）

- **动态库**
  下载 `dynamic_lib_{version}` 压缩包：
  - 将 `dynamic_lib/include` 目录添加到头文件搜索路径；
  - 链接 `dynamic_lib/lib` 中对应平台的动态库文件。

- **静态库**
  下载 `static_lib_{version}` 压缩包：
  - 将 `static_lib/include` 目录添加到头文件搜索路径；
  - 链接 `static_lib/lib` 中对应平台的静态库文件。

- **源码集成**
  - 将仓库下的 `/src` 目录加入工程源码编译；
  - 将 `/include` 目录添加到头文件搜索路径。
  - Windows + Visual Studio：请添加编译选项 `/Zc:__cplusplus`。
  - Android 模式下支持 `ANDROID_STL = none`。
  - 如需启用 Java / NAPI（Node.js / HarmonyOS ArkTS）支持，以及系统链接库与部分宏定义，请参考 `/src/CMakeLists.txt`（如果觉得难以理解，建议求助AI提炼）。
  - 当存在 NAPI 环境（Node.js 或 HarmonyOS ArkTS）或需要 Java / C# 调用时，不推荐以「纯 C++ 源码」直接集成，因为需要手动处理初始化和库加载流程；更推荐使用预编译包及对应 wrapper。

---

## C#（Unity / 团结引擎 / .NET）

- **Unity / 团结引擎**
  完整引擎集成步骤请见 [游戏引擎集成指南](./ENGINE_INTEGRATION_CHS.md)。

- **.NET**
  - 下载对应平台的动态库包 `{os}_{arch}_libs_{version}`，引入其中动态库；
  - 将 C# wrapper 源码加入工程 —— 可以直接使用仓库下 `/wrapper/csharp/src`，也可以从 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载 `c#_wrapper_{version}` 预打包的源码包。

---

## iOS / Apple 平台（C++ / Objective-C）

从 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载 `ios_libs_{version}`。包内包含 `BqLog.xcframework`，支持多个 Apple 平台（iOS 真机、iOS 模拟器、tvOS、watchOS、visionOS）及多种构建配置（Debug、Release、RelWithDebInfo、MinSizeRel）。

### 通过 Xcode 集成

1. 将 `BqLog.xcframework` 拖入 Xcode 工程；
2. 在 target 的 **General → Frameworks, Libraries, and Embedded Content** 中，确保 `BqLog.xcframework` 设置为 **Embed & Sign**；
3. 在源码中包含头文件：

```cpp
#include "bq_log/bq_log.h"
```

### 通过 CMake 集成（适用于基于 CMake 的 iOS 工程）

```cmake
# 将路径替换为你放置 xcframework 的实际位置
set(BQLOG_XCFRAMEWORK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/path/to/BqLog.xcframework")
find_library(BQLOG_LIB BqLog PATHS ${BQLOG_XCFRAMEWORK_PATH} REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} ${BQLOG_LIB})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${BQLOG_XCFRAMEWORK_PATH}/Headers")
```

> **注意：** BqLog 目前未发布到 CocoaPods 或 Swift Package Manager，请从 Releases 页面下载 xcframework 进行集成。

---

## PS5（C++）

仅支持 C++。Sony 官方 SDK（prospero-clang，通过预定义宏 `__PROSPERO__` 检测）与开源 ps5-payload-sdk 均已支持。平台在 `include/bq_common/platform/macros.h` 中自动识别，无需定义任何宏。

- **源码集成**：与上文 C++ 源码集成方式相同 —— 将 `/src` 加入工程源码编译、`/include` 加入头文件搜索路径即可，平台会根据工具链自动识别。
- **独立库编译**（仅支持静态库）：

```bash
cmake /path/to/BqLog/src -DTARGET_PLATFORM:STRING=ps5 -DCMAKE_TOOLCHAIN_FILE=$PS5_PAYLOAD_SDK/toolchain/prospero.cmake
```

或运行脚本 `build/lib/ps5/build_all_and_pack.sh`（需要 `PS5_PAYLOAD_SDK` 环境变量）。

---

## Nintendo Switch（C++）

仅支持 C++。两种开发环境均已支持，平台在 `include/bq_common/platform/macros.h` 中自动识别，无需定义任何宏：

- **devkitPro devkitA64 + libnx**（Homebrew）：通过 devkitPro 构建规则自带的 `__SWITCH__` 宏检测。可源码集成，也可编译独立库（仅支持静态库）：

```bash
cmake /path/to/BqLog/src -DTARGET_PLATFORM:STRING=switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
```

或运行脚本 `build/lib/switch/build_all_and_pack.sh`（需要 `DEVKITPRO` 环境变量）。

- **Nintendo 官方 SDK**（`nn::` API）：通过 `__NX__` / `NN_NINTENDO_SDK` / `__has_include(<nn/os.h>)` 检测。推荐源码集成 —— 将 `/src` 与 `/include` 放入游戏工程即可自动完成平台检测。

---

## Java / Kotlin（Android / Server）

### Android

- **Maven Central（推荐）**

  在 `app/build.gradle.kts` 中添加：

  ```kotlin
  android {
      buildFeatures {
          prefab = true   // 启用 Prefab，C++ 侧才能通过 find_package 找到头文件和 .so
      }
  }

  dependencies {
      implementation("com.tencent.bqlog:android:2.+")
  }
  ```

  ```java
  import bq.log;

  bq.log myLog = bq.log.create_log("myApp", """
      appenders_config.console.type=console
      appenders_config.console.levels=[all]
  """);
  myLog.info("Hello from Android Java!");
  ```

  **C++（NDK）通过 Prefab 使用** — 在 `CMakeLists.txt` 中添加：

  ```cmake
  find_package(BqLog REQUIRED CONFIG)

  target_link_libraries(${CMAKE_PROJECT_NAME}
      bqlog::BqLog
      android
      log)
  ```

  C++ 代码中直接包含：

  ```cpp
  #include "bq_log/bq_log.h"
  ```

- **手动引入 AAR**

  从 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载 `android_libs_{version}`，将其中的 `bqlog-release.aar` 复制到项目的 `libs/` 目录，然后在 `app/build.gradle.kts` 中添加：

  ```kotlin
  android {
      buildFeatures {
          prefab = true
      }
  }

  dependencies {
      implementation(files("libs/bqlog-release.aar"))
  }
  ```

  Java / Kotlin 与 C++（NDK）的使用方式与 Maven 方式完全相同。

### Java（Server / Desktop）

- **Maven Central（推荐）**

  fat JAR 内已打包全平台 native 库（Windows x86_64/arm64、Linux x86_64/arm64/x86、macOS universal、FreeBSD、OpenBSD、NetBSD、DragonFlyBSD、SunOS），运行时自动解压并加载对应平台的库文件，无需额外配置。

  **Maven (`pom.xml`)**：

  ```xml
  <dependency>
      <groupId>com.tencent.bqlog</groupId>
      <artifactId>java</artifactId>
      <version>[2.0,3.0)</version>
  </dependency>
  ```

  **Gradle (`build.gradle.kts`)**：

  ```kotlin
  dependencies {
      implementation("com.tencent.bqlog:java:2.+")
  }
  ```

  ```java
  import bq.log;

  bq.log myLog = bq.log.create_log("myApp", """
      appenders_config.console.type=console
      appenders_config.console.levels=[all]
  """);
  myLog.info("Hello from Java! value: {}", 3.14);
  ```

- **手动引入 JAR**

  从 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载对应平台的动态库 `{os}_{arch}_libs_{version}`，再下载 `java_wrapper_{version}`，将 JAR 加入 classpath，运行时通过 `-Djava.library.path=/path/to/lib` 指定 native 库目录。

---

## HarmonyOS / OpenHarmony（ArkTS / C++）

> **HarmonyOS NEXT** 和 **OpenHarmony**（4.1+ / API 11+）共用 ohpm 上**同一个** `bqlog` 包：底层 `.so` 在两边二进制兼容，ArkTS Wrapper 只用公共 API，**不需要单独发包或单独配置**。

### 集成方式

- **ohpm（推荐）**

  ```bash
  ohpm install bqlog
  ```

  或在模块的 `oh-package.json5` 中添加：

  ```json5
  "dependencies": {
      "bqlog": "latest"
  }
  ```

- **手动引入 har**

  从 [Releases 页面](https://github.com/Tencent/BqLog/releases) 下载 `harmony_os_libs_{version}`，将其中的 `.har` 文件复制到项目中，然后在模块的 `oh-package.json5` 中添加：

  ```json5
  "dependencies": {
      "bqlog": "file:./path/to/bqlog.har"
  }
  ```

支持在 ArkTS 侧直接调用，也支持在 Native C++ 侧调用。**HarmonyOS NEXT 和 OpenHarmony 上行为一致，使用方式完全相同。**

### C++（Native）使用方式

引入 `har` 包后（无论 ohpm 还是手动方式），在你的 Native 模块的 `CMakeLists.txt` 中添加：

```cmake
find_package(BqLog)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC bqlog::BqLog)
```

然后在 C++ 代码中直接包含：

```cpp
#include "bq_log/bq_log.h"
```

---

## Node.js

- 支持 CommonJS 与 ES Module。
- **npm（推荐）**：

```bash
npm install @pippocao/bqlog
```

- **手动安装** — 从 Releases 下载 `nodejs_npm_{version}` 包，解压后找到其中的 `pippocao-bqlog-{version}.tgz`，通过 npm 安装：

```bash
npm install ./pippocao-bqlog-{version}.tgz
```

可参考仓库下 `/demo/nodejs` 目录。

---

## Python

- 支持 Python 3.7+（CPython C Extension，Stable ABI）。
- **pip（推荐）**：

```bash
pip install bqlog
```

- **手动安装** — 从 Releases 下载 `python_wrapper_{version}` 包，解压后找到 `.whl` 文件，通过 pip 安装：

```bash
pip install ./bqlog-{version}-cp37-abi3-{platform}.whl
```

可参考仓库下 `/demo/python` 目录。

---

## Unity / 团结引擎 / Unreal Engine

完整引擎集成步骤请见 **[游戏引擎集成指南](./ENGINE_INTEGRATION_CHS.md)**，包括：
- Unity 和团结引擎 Package 导入
- Unreal Engine 插件（通过 [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf)、预编译版或源码版，支持 UE4、UE5 与当前 UE6 开发版；UE6 仅通过 GitHub Release 提供）
- `FString` / `FName` / `FText` 支持
- BqLog 输出重定向到 Unreal Output Log 窗口
- 蓝图使用方式

---

## 快速上手 Demo

### C++

```cpp
#include <string>
#include <bq_log/bq_log.h>

int main() {
    // 配置：输出到控制台
    std::string config = R"(
        appenders_config.appender_console.type=console
        appenders_config.appender_console.levels=[all]
    )";
    auto log = bq::log::create_log("main_log", config);

    log.info("Hello BqLog 2.0! int:{}, float:{}", 123, 3.14f);
    log.force_flush(); // 强制刷新（通常用于程序退出前）

    return 0;
}
```

更多示例可参考仓库下的 `/demo/cpp` 目录。

### TypeScript — Node.js

```typescript
import { bq } from "@pippocao/bqlog"; // ESM 写法
// const { bq } = require("@pippocao/bqlog"); // CommonJS 写法

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);

log.info("Hello from Node.js! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

更多示例可参考仓库下的 `/demo/nodejs` 目录。

### TypeScript — 鸿蒙 ArkTS

```typescript
import { bq } from "bqlog"; // ohpm 包名

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("ohos_log", config);

log.info("Hello from HarmonyOS! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

### Python

```python
from bq.log import log

config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
"""
my_log = log.create_log("python_log", config)
my_log.info("Hello from Python! params: {}, {}", "text", 123)
log.force_flush_all_logs()
```

更多示例可参考仓库下的 `/demo/python` 目录。

### C#

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```

更多示例可参考仓库下的 `/demo/csharp` 目录。

### Java（Android / Server）

```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```

更多示例可参考仓库下的 `/demo/java` 目录。

---

## 接下来

- [API 参考](./API_REFERENCE_CHS.md) — 创建日志、写日志等核心 API
- [配置说明](./CONFIGURATION_CHS.md) — 完整配置参考
- [高级用法](./ADVANCED_USAGE_CHS.md) — Category、加密、自定义类型等
- [游戏引擎集成](./ENGINE_INTEGRATION_CHS.md) — Unity、团结引擎、Unreal Engine
