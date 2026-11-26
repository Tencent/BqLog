# Godot 4 Plugin Build Scripts

These scripts build the `BqLogGodot` GDExtension library against a prebuilt `BqLog` dynamic library (shared singleton across GDScript, C#, and C++).

## Prerequisites

- CMake 3.22+
- Python 3.7+ (used by `godot-cpp` binding generator)
- Platform C++17 toolchain (MSVC on Windows, Xcode on macOS/iOS, GCC/Clang on Linux, Android NDK on Android)
- Optional: set `GODOT_CPP_DIR` to an existing `godot-cpp` (`godot-4.2-stable`) checkout to avoid network fetch.

## Usage

- **Windows**: `build_win64.bat <path_to_BqLog.lib> [x64|arm64]`
- **Linux**: `./build_linux.sh <path_to_libBqLog.so> [x86_64|arm64]`
- **macOS**: `./build_macos.sh <path_to_libBqLog.dylib>`
- **Android**: `./build_android.sh <android_ndk_root> <bqlog_android_jni_dir>`
- **OpenHarmony / HarmonyOS**: `./build_ohos.sh <ohos_sdk_dir> <bqlog_ohos_lib_dir>`
- **iOS**: `./build_ios.sh <path_to_BqLog.xcframework>`
