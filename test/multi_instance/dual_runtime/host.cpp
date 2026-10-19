/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
// Host for the dual-runtime test: loads two differently named copies of the
// same plugin, each statically linking its own BqLog runtime. The two runtimes
// must claim different instance indices.
#include <cstdio>
#include <cstring>
#include <string>
#include "bq_common/platform/macros.h"

#if defined(BQ_WIN)
#include <windows.h>
typedef HMODULE lib_handle;
#define BQ_HOST_LIB_EXT ".dll"
static lib_handle open_lib(const char* path)
{
    return LoadLibraryA(path);
}
static void* load_symbol(lib_handle lib, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(lib, name));
}
static const char* last_lib_error()
{
    return "LoadLibrary failed";
}
#else
#include <dlfcn.h>
typedef void* lib_handle;
#if defined(BQ_APPLE)
#define BQ_HOST_LIB_EXT ".dylib"
#else
#define BQ_HOST_LIB_EXT ".so"
#endif
static lib_handle open_lib(const char* path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}
static void* load_symbol(lib_handle lib, const char* name)
{
    return dlsym(lib, name);
}
static const char* last_lib_error()
{
    return dlerror();
}
#endif

typedef int (*plugin_run_fn)(const char* log_name);

static int run_plugin(const char* tag, const std::string& path, const char* log_name)
{
    lib_handle lib = open_lib(path.c_str());
    if (!lib) {
        std::fprintf(stderr, "failed to load %s: %s\n", path.c_str(), last_lib_error());
        return -100;
    }
    auto plugin_run = reinterpret_cast<plugin_run_fn>(load_symbol(lib, "plugin_run"));
    if (!plugin_run) {
        std::fprintf(stderr, "plugin_run not found in %s\n", path.c_str());
        return -101;
    }
    int instance = plugin_run(log_name);
    std::printf("plugin %s instance %d\n", tag, instance);
    return instance;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: bq_dual_runtime_host <log_name>\n");
        return 2;
    }
    std::string lib_dir;
    {
        const char* self = argv[0] != nullptr ? argv[0] : "";
        const char* slash = std::strrchr(self, '/');
        const char* backslash = std::strrchr(self, '\\');
        if (backslash != nullptr && (slash == nullptr || backslash > slash)) {
            slash = backslash;
        }
        if (slash != nullptr) {
            lib_dir.assign(self, static_cast<size_t>(slash - self) + 1);
        } else {
            lib_dir = "./";
        }
    }
    // two file copies of the same plugin so the OS loader treats them as
    // distinct modules even on systems that cache by content
    int a = run_plugin("a", lib_dir + "plugin_a" BQ_HOST_LIB_EXT, argv[1]);
    int b = run_plugin("b", lib_dir + "plugin_b_copy" BQ_HOST_LIB_EXT, argv[1]);
    return (a == 0 && b == 1) ? 0 : 1;
}
