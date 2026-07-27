# Integration Guide

[← Back to Home](../README.md) | [简体中文](./INTEGRATION_GUIDE_CHS.md)

> The examples below assume you have already downloaded the corresponding binary package or source code from the [Releases page](https://github.com/Tencent/BqLog/releases).

---

## C++ (dynamic / static / source)

- **Dynamic library**
  Download `dynamic_lib_{version}` archive:
  - Add `dynamic_lib/include` directory to your header search path;
  - Link against the dynamic library file in `dynamic_lib/lib` for your platform.

- **Static library**
  Download `static_lib_{version}` archive:
  - Add `static_lib/include` directory to your header search path;
  - Link against the static library file in `static_lib/lib` for your platform.

- **Source integration**
  - Add the `/src` directory under the repo into your project sources compilation;
  - Add `/include` directory to your header search path.
  - Windows + Visual Studio: please add compile option `/Zc:__cplusplus`.
  - Android supports `ANDROID_STL = none`.
  - If you need to enable Java / NAPI (Node.js / HarmonyOS ArkTS) support, as well as system link libraries and some macro definitions, please refer to `/src/CMakeLists.txt`(If the file implies complexity, consider using AI to interpret it.).
  - When NAPI environment (Node.js or HarmonyOS ArkTS) is present, or when Java / C# needs to call, it is not recommended to integrate directly as "pure C++ source", because initialization and library loading processes need to be handled manually; using prebuilt packages and corresponding wrappers is more recommended.

---

## C# (Unity / Tuanjie Engine / .NET)

- **Unity / Tuanjie Engine**
  For full engine integration steps, see [Game Engine Integration Guide](./ENGINE_INTEGRATION.md).

- **.NET**
  - Download the dynamic library package `{os}_{arch}_libs_{version}` for the corresponding platform, and import the dynamic libraries within;
  - Add the C# wrapper source to your project — either clone the repository and include `/wrapper/csharp/src`, or download `c#_wrapper_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases) which contains the same source files pre-packaged.

---

## iOS / Apple Platforms (C++ / Objective-C)

Download `ios_libs_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases). The package contains `BqLog.xcframework` with support for multiple Apple platforms (iOS device, iOS Simulator, tvOS, watchOS, visionOS) and multiple build configurations (Debug, Release, RelWithDebInfo, MinSizeRel).

### Integration via Xcode

1. Drag `BqLog.xcframework` into your Xcode project;
2. In your target's **General → Frameworks, Libraries, and Embedded Content**, ensure `BqLog.xcframework` is set to **Embed & Sign**;
3. Include in your source files:

```cpp
#include "bq_log/bq_log.h"
```

### Integration via CMake (for CMake-based iOS projects)

```cmake
# Adjust the path to where you placed the xcframework
set(BQLOG_XCFRAMEWORK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/path/to/BqLog.xcframework")
find_library(BQLOG_LIB BqLog PATHS ${BQLOG_XCFRAMEWORK_PATH} REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} ${BQLOG_LIB})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${BQLOG_XCFRAMEWORK_PATH}/Headers")
```

> **Note:** BqLog does not currently publish to CocoaPods or Swift Package Manager. Use the xcframework from the Releases page for integration.

---

## PS5 (C++)

C++ only. Both the Sony official SDK (prospero-clang, detected via the `__PROSPERO__` predefine) and the open ps5-payload-sdk are supported. The platform is auto-detected in `include/bq_common/platform/macros.h`, so no macro definitions are needed.

- **Source integration**: same as the C++ source integration above — add `/src` to your project sources and `/include` to the header search path; the platform is detected automatically from the toolchain.
- **Standalone library** (static library only):

```bash
cmake /path/to/BqLog/src -DTARGET_PLATFORM:STRING=ps5 -DCMAKE_TOOLCHAIN_FILE=$PS5_PAYLOAD_SDK/toolchain/prospero.cmake
```

Or run the script `build/lib/ps5/build_all_and_pack.sh` (requires the `PS5_PAYLOAD_SDK` environment variable).

---

## Nintendo Switch (C++)

C++ only. Both development environments are supported, and the platform is auto-detected in `include/bq_common/platform/macros.h` — no macro definitions are needed:

- **devkitPro devkitA64 + libnx** (homebrew): detected via `__SWITCH__`, which devkitPro's own build rules define. Source integration works, or build the standalone library (static library only):

```bash
cmake /path/to/BqLog/src -DTARGET_PLATFORM:STRING=switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
```

Or run the script `build/lib/switch/build_all_and_pack.sh` (requires the `DEVKITPRO` environment variable).

- **Official Nintendo SDK** (`nn::` APIs): detected via `__NX__` / `NN_NINTENDO_SDK` / `__has_include(<nn/os.h>)`. Source integration is the way to go — drop `/src` and `/include` into your game project and detection is automatic.

---

## Java / Kotlin (Android / Server)

### Android

- **Maven Central (recommended)**

  Add to your `app/build.gradle.kts`:

  ```kotlin
  android {
      buildFeatures {
          prefab = true   // required for C++ native header access via Prefab
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

  **C++ (NDK) usage via Prefab** — add to your `CMakeLists.txt`:

  ```cmake
  find_package(BqLog REQUIRED CONFIG)

  target_link_libraries(${CMAKE_PROJECT_NAME}
      bqlog::BqLog
      android
      log)
  ```

  Then include in C++:

  ```cpp
  #include "bq_log/bq_log.h"
  ```

- **Manual AAR**

  Download `android_libs_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases), copy `bqlog-release.aar` into your project's `libs/` directory, then add to `app/build.gradle.kts`:

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

  Java/Kotlin and C++ (NDK) usage are identical to the Maven approach above.

### Java (Server / Desktop)

- **Maven Central (recommended)**

  The fat JAR bundles native libraries for all platforms (Windows x86_64/arm64, Linux x86_64/arm64/x86, macOS universal, FreeBSD, OpenBSD, NetBSD, DragonFlyBSD, SunOS). No extra configuration needed — the correct `.so`/`.dll`/`.dylib` is extracted and loaded automatically at runtime.

  **Maven (`pom.xml`)**:

  ```xml
  <dependency>
      <groupId>com.tencent.bqlog</groupId>
      <artifactId>java</artifactId>
      <version>[2.0,3.0)</version>
  </dependency>
  ```

  **Gradle (`build.gradle.kts`)**:

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

- **Manual JAR**

  Download the dynamic library `{os}_{arch}_libs_{version}` for your platform from the [Releases page](https://github.com/Tencent/BqLog/releases), then download `java_wrapper_{version}` and add the JAR to your classpath. When running, pass the native library directory via `-Djava.library.path=/path/to/lib`.

---

## HarmonyOS / OpenHarmony (ArkTS / C++)

> **HarmonyOS NEXT** and **OpenHarmony** (4.1+ / API 11+) share the **same** `bqlog` package on ohpm. The native `.so` is binary-compatible across both runtimes, and the ArkTS wrapper uses only common APIs. No separate package or build is needed.

### Integration

- **ohpm (recommended)**

  ```bash
  ohpm install bqlog
  ```

  Or add to your module's `oh-package.json5`:

  ```json5
  "dependencies": {
      "bqlog": "latest"
  }
  ```

- **Manual har**

  Download `harmony_os_libs_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases), copy the `.har` file into your project, then add to your module's `oh-package.json5`:

  ```json5
  "dependencies": {
      "bqlog": "file:./path/to/bqlog.har"
  }
  ```

Supports direct calling from ArkTS side, also supports calling from Native C++ side. Works identically on HarmonyOS NEXT and OpenHarmony.

### C++ (Native) usage

After importing the `har` package (via ohpm or manual), add the following to your native module's `CMakeLists.txt`:

```cmake
find_package(BqLog)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC bqlog::BqLog)
```

Then include in C++:

```cpp
#include "bq_log/bq_log.h"
```

---

## Node.js

- Supports CommonJS and ES Modules.
- **npm (recommended)**:

```bash
npm install @pippocao/bqlog
```

- **Manual install** — From Releases download `nodejs_npm_{version}` package, unzip to find `pippocao-bqlog-{version}.tgz` inside, install via npm:

```bash
npm install ./pippocao-bqlog-{version}.tgz
```

Refer to `/demo/nodejs` directory under the repository.

---

## Python

- Supports Python 3.7+ (CPython C Extension, Stable ABI).
- **pip (recommended)**:

```bash
pip install bqlog
```

- **Manual install** — From Releases download `python_wrapper_{version}` package, unzip to find `.whl` file inside, install via pip:

```bash
pip install ./bqlog-{version}-cp37-abi3-{platform}.whl
```

Refer to `/demo/python` directory under the repository.

---

## Unity / Tuanjie Engine / Unreal Engine

See the dedicated **[Game Engine Integration Guide](./ENGINE_INTEGRATION.md)** for:
- Unity and Tuanjie Engine package import
- Unreal Engine plugin via [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf), prebuilt, or source (UE4, UE5, and current UE6 development builds; UE6 is GitHub Release only)
- `FString` / `FName` / `FText` support
- Redirect BqLog output to Unreal's Output Log window
- Blueprint usage

---

## Quick Start Demos

### C++

```cpp
#include <string>
#include <bq_log/bq_log.h>

int main() {
    // Config: output to console
    std::string config = R"(
        appenders_config.appender_console.type=console
        appenders_config.appender_console.levels=[all]
    )";
    auto log = bq::log::create_log("main_log", config);

    log.info("Hello BqLog 2.0! int:{}, float:{}", 123, 3.14f);
    log.force_flush(); // Force flush (usually used before program exit)

    return 0;
}
```

For more examples, refer to `/demo/cpp` directory.

### TypeScript — Node.js

```typescript
import { bq } from "@pippocao/bqlog"; // ESM style
// const { bq } = require("@pippocao/bqlog"); // CommonJS style

const config = `
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
`;
const log = bq.log.create_log("node_log", config);

log.info("Hello from Node.js! params: {}, {}", "text", 123);
bq.log.force_flush_all_logs();
```

For more examples, refer to `/demo/nodejs` directory.

### TypeScript — ArkTS on HarmonyOS

```typescript
import { bq } from "bqlog"; // ohpm package name

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

For more examples, refer to `/demo/python` directory.

### C#

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello C#! value:{}", 42);
```

For more examples, refer to `/demo/csharp` directory.

### Java (Android / Server)

```java
String config = """
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
""";
bq.log.Log log = bq.log.Log.createLog("java_log", config);
log.info("Hello Java! value: {}", 3.14);
```

For more examples, refer to `/demo/java` directory.

---

## Next Steps

- [API Reference](./API_REFERENCE.md) — Core APIs for creating logs, writing logs, and more
- [Configuration](./CONFIGURATION.md) — Full configuration reference
- [Advanced Usage](./ADVANCED_USAGE.md) — Category, encryption, custom types, and more
- [Game Engine Integration](./ENGINE_INTEGRATION.md) — Unity, Tuanjie Engine, Unreal Engine
