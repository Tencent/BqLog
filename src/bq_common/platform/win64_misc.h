#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "bq_common/bq_common_public_include.h"
#ifdef BQ_WIN

namespace bq {
    namespace platform {
        using platform_file_handle = void*;
        const platform_file_handle invalid_platform_file_handle = reinterpret_cast<void*>(-1);
        bq_forceinline bool is_platform_handle_valid(const platform_file_handle& file_handle)
        {
            return file_handle != invalid_platform_file_handle;
        }

        // File exclusive works well across different processes,
        // but mutual exclusion within the same process is not explicitly documented to function reliably across different system platforms.
        // To eliminate platform compatibility risks, we decided to implement it ourselves.
        BQ_PACK_BEGIN
        struct alignas(4) file_node_info {
            uint32_t volumn;
            uint32_t idx_high;
            uint32_t idx_low;
            uint64_t hash_code() const;
            bool operator==(const file_node_info& rhs) const
            {
                return volumn == rhs.volumn && idx_high == rhs.idx_high && idx_low == rhs.idx_low;
            }
        } BQ_PACK_END

            struct windows_version_info {
            uint32_t major_version = 0;
            uint32_t minor_version = 0;
            uint32_t build_number = 0;
        };
        const windows_version_info& get_windows_version_info();

        void* get_process_adress(const char* module_name, const char* api_name);

        template <typename API_DEC_TYPE>
        API_DEC_TYPE get_sys_api(const char* module_name, const char* api_name)
        {
#if defined(BQ_MSVC)
#pragma warning(push)
#pragma warning(disable : 4191)
#endif
            return reinterpret_cast<API_DEC_TYPE>((void*)get_process_adress(module_name, api_name));
#if defined(BQ_MSVC)
#pragma warning(pop)
#endif
        }
    }
}
#endif
