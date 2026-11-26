/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
// Defined before any header so glibc/Bionic/musl expose Dl_info and dladdr under -std=c++17.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "bq_log_godot_runtime.h"

#include "bq_log/bq_log.h"
#if defined(BQ_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(BQ_POSIX)
#include <dlfcn.h>
#endif

namespace bq_godot {

    // Special workaround (hack) specific to Godot 4 GDExtension teardown:
    // Unlike Unity/Unreal/JVM/CLR/CPython, Godot calls FreeLibrary/dlclose on GDExtensions inside
    // Main::cleanup() before main() returns. BqLog core is designed as an immortal process-wide
    // singleton with persistent background log_worker threads (which cannot be joined inside DllMain
    // due to the Windows Loader Lock, and must remain usable by C#/C++ destructors during shutdown).
    // Rather than invading BqLog core src/ (which would affect direct C++ source integrators), we
    // locate the loaded BqLog image via the .rodata/.rdata address returned by __api_get_log_version()
    // and retain +1 OS module reference for the process lifetime.
    void retain_bqlog_runtime()
    {
        static bool retained = false;
        if (retained) {
            return;
        }
        retained = true;

        const char* version_rodata_addr = bq::api::__api_get_log_version();
        if (!version_rodata_addr) {
            return;
        }

#if defined(BQ_WIN)
        HMODULE bqlog_module = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(version_rodata_addr),
            &bqlog_module);
#elif defined(BQ_POSIX)
        Dl_info info {};
        if (dladdr(static_cast<const void*>(version_rodata_addr), &info) != 0 && info.dli_fname) {
            int flags = RTLD_LAZY | RTLD_LOCAL;
#if defined(RTLD_NOLOAD)
            flags |= RTLD_NOLOAD;
#endif
#if defined(RTLD_NODELETE)
            flags |= RTLD_NODELETE;
#endif
            (void)dlopen(info.dli_fname, flags);
        }
#endif
    }

}
