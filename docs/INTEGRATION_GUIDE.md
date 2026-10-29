# Integration Guide

[← Back to Home](../README.md) | [简体中文](./INTEGRATION_GUIDE_CHS.md)

> The examples below assume you have already downloaded the corresponding binary package or source code from the [Releases page](https://github.com/Tencent/BqLog/releases).

---

## Distribution channels

The C/C++ packages published by vcpkg and Homebrew contain the C and C++ APIs and the command line tools only. The Java, C#, Node.js and Python wrappers are not part of them: a wrapper is built together with a core library compiled for that language, so each wrapper ships with its own package (see the per-language sections below). Use vcpkg / Homebrew for C/C++ projects, or to build a wrapper from the source tree.

---

## C++ (dynamic / static / source)

- **Homebrew (macOS / Linux)**

  ```bash
  brew install bqlog
  ```

  The formula includes the `BqLog_CategoryLogGenerator` and `BqLog_LogDecoder` command line tools by default. The package is then found through CMake or pkg-config:

  ```cmake
  find_package(BqLog REQUIRED)
  target_link_libraries(your_target PRIVATE bqlog::BqLogShared)   # or bqlog::BqLogStatic
  ```

  ```bash
  pkg-config --cflags --libs bqlog
  ```

- **vcpkg (Windows / Linux / macOS)**

  ```bash
  vcpkg install bqlog
  vcpkg install "bqlog[tools]"   # also the command line tools (off by default), into installed/<triplet>/tools/bqlog/
  ```

  In manifest mode, list `{ "name": "bqlog", "features": ["tools"] }` in `vcpkg.json`. The triplet decides static or shared; the target is `bqlog::BqLog`.

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

### C++20 modules (`import`)

```cpp
import bq.log;
```

The module interface unit `bq_log/bq.log.cppm` ships with the headers and is compiled by your project. With CMake 3.28+ (Ninja or Visual Studio generator):

```cmake
find_package(BqLog REQUIRED)

target_sources(your_target PRIVATE FILE_SET CXX_MODULES
    BASE_DIRS "${BQLOG_INCLUDE_DIR}"
    FILES "${BQLOG_MODULE_FILE}")
target_link_libraries(your_target PRIVATE bqlog::BqLogStatic)
```

Supported compilers: Clang 16+, MSVC 19.34+, GCC 15+ (GCC 14 crashes with an internal compiler error on the module). GCC also needs `-fmodules-ts`.

**Known GCC 16 issue:** a C header included after `import bq.log;` — `<string.h>` among them — may be reported as `redefinition of 'strstr'` (or `memcpy`). It is a GCC 16 regression: GCC 15, Clang and MSVC accept the same code. Workaround: include your system headers before the import.

```cpp
#include <string.h> // system headers first
import bq.log;
```

Without CMake, with clang:

```bash
clang++ -std=c++20 -x c++-module --precompile -I$PREFIX/include $PREFIX/include/bq_log/bq.log.cppm -o bq.log.pcm
clang++ -std=c++20 -c bq.log.pcm -o bq.log.o
clang++ -std=c++20 -fmodule-file=bq.log=bq.log.pcm -c app.cpp
clang++ bq.log.o app.o -lBqLog -o app
```

Macros are not exported, so with `import` the fast log is written as `log.fast_info<"...">(args...)`.

---

## C# (Unity / Tuanjie Engine / .NET)

- **Unity / Tuanjie Engine**
  For full engine integration steps, see [Game Engine Integration Guide](./ENGINE_INTEGRATION.md).

- **.NET — NuGet**

  ```bash
  dotnet add package BqLog
  ```

  Includes the C# bindings and the native libraries for `win-x64`, `win-arm64`, `linux-x64`,
  `linux-arm64`, `osx-x64` and `osx-arm64`.

- **.NET — manual**
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

<!-- go-module:start -->
## Go

Install through Go Modules:

```sh
# Skip initialization if the project already has go.mod
go mod init example.com/myapp
go get github.com/Tencent/BqLog/go/v2@latest
```

Save the following as `main.go`:

```go
package main

import bq "github.com/Tencent/BqLog/go/v2"

func main() {
    config := `
appenders_config.console.type=console
appenders_config.console.levels=[all]
`
    log := bq.Create_log("go_log", config, nil)
    if !log.Is_valid() {
        panic("invalid log config")
    }
    defer log.Force_flush()

    log.Info("Hello from Go! params: {}, {}", "text", 123)
}
```

Run the example:

```sh
go run .
```

Logging methods accept zero or any number of arguments. Objects support Go's
String/Error methods, and nil is logged as null.

### Packages and versions

The module follows the BqLog version. Go Modules downloads the source, and
`go build` compiles the native sources automatically.

[pkg.go.dev](https://pkg.go.dev/github.com/Tencent/BqLog/go/v2) provides package
search, README and API documentation.
<!-- go-module:end -->

- **Prebuilt library**: when linking a prebuilt BqLog library instead of letting cgo build the
  sources, use a desktop (Windows / Linux / macOS) package from the release page; it includes the
  Go bridge.

---

## Next Steps

- [API Reference](./API_REFERENCE.md) — Core APIs for creating logs, writing logs, and more
- [Configuration](./CONFIGURATION.md) — Full configuration reference
- [Advanced Usage](./ADVANCED_USAGE.md) — Category, encryption, custom types, and more
- [Game Engine Integration](./ENGINE_INTEGRATION.md) — Unity, Tuanjie Engine, Unreal Engine

## Console platforms

Because of licensing, PS4, PS5 and Switch are source integrations only: add `src/` and `include/`
to the platform toolchain's project and build them there.

The platform is recognized from macros the toolchain passes: `__ORBIS__` (PS4), `__PROSPERO__`
(PS5), `__SWITCH__` or `NN_NINTENDO_SDK` (Switch). A normal console build environment already
defines them, so they usually need no manual work.

Both base dirs (type 0 and type 1) must be set before the first log is created, once the storage is
mounted; a console has no writable working directory, so otherwise the process stops with a fatal
message.

```cpp
// Mount the console's storage first and take mount_path from it
bq::log::reset_base_dir(0, mount_path);   // after the game mounts its storage
bq::log::reset_base_dir(1, mount_path);
auto log = bq::log::create_log("game", R"({"log_config": {"recovery": true}})");
```

Note: integrating through Unreal Engine lets you skip this step, because the plugin already points
both base dirs at the project's saved directory; see
[Game Engine Integration — Unreal Engine](./ENGINE_INTEGRATION.md#unreal-engine).

| Capability | PS4 / PS5 | Switch | Effect when absent |
|---|---|---|---|
| `memory_map` | yes | no | no crash recovery, buffers stay on the heap |
| `multi_process` | no | no | one instance per title; no multi-process isolation |
| `cross_process_file_lock` | no | no | implied by the line above |
| `crash_handler` | no | no | `enable_auto_crash_handle()` installs nothing |
| `backtrace` | no | no | the stack field says it is unsupported |
| `dirent` | yes | yes | `expire_time_*` and `capacity_limit` need it |
| `thread_name` | no | no | threads report `thread_<id>` |
| `default_base_dir` | no | no | `reset_base_dir` is mandatory |
| `user_counter` | no | no | timestamps come from the system clock |

`bq::log::get_platform_capabilities()` returns these as bits, and the same values are printed once
at startup, so a bug report can carry them. PlayStation and Switch are experimental for now: if you
hit a problem, please file an issue with the `get_platform_capabilities()` output mentioned above.
