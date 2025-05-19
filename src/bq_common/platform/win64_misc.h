#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#ifdef BQ_WIN
#include <WinSock2.h>
#include <Windows.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        using platform_file_handle = HANDLE;
        const platform_file_handle invalid_platform_file_handle = INVALID_HANDLE_VALUE;
        bq_forceinline bool is_platform_handle_valid(const platform_file_handle& file_handle)
        {
            return file_handle != invalid_platform_file_handle;
        }

        // File exclusive works well across different processes,
        // but mutual exclusion within the same process is not explicitly documented to function reliably across different system platforms.
        // To eliminate platform compatibility risks, we decided to implement it ourselves.
        BQ_PACK_BEGIN
        struct alignas(4) file_node_info {
            DWORD volumn;
            DWORD idx_high;
            DWORD idx_low;
            uint64_t hash_code() const;
            bool operator==(const file_node_info& rhs) const
            {
                return volumn == rhs.volumn && idx_high == rhs.idx_high && idx_low == rhs.idx_low;
            }
        }
        BQ_PACK_END

        const RTL_OSVERSIONINFOW& get_windows_version_info();

        template<typename API_DEC_TYPE>
        API_DEC_TYPE get_sys_api(const char* module_name, const char* api_name)
        {
            HMODULE module = GetModuleHandle(module_name);
            if (module) {
                return reinterpret_cast<API_DEC_TYPE>((void*)GetProcAddress(module, api_name));
            }
            return nullptr;
        }
    }
}
#endif
