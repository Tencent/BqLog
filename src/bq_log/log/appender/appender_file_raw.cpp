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
#include <assert.h>
#include "appender_file_raw.h"

namespace bq {
    void appender_file_raw::log_impl(const log_entry_handle& handle)
    {
        appender_file_base::log_impl(handle);
        size_t item_size = handle.data_size();
        if (item_size > 0xFFFFFFFF) {
            util::log_device_console(bq::log_level::warning, "log length exceed MAX_UINT32, skipped");
            return;
        }
        uint32_t item_size_uint32 = static_cast<uint32_t>(item_size);
        auto write_handle = write_with_cache_alloc(sizeof(item_size_uint32) + item_size_uint32);
        *(decltype(item_size_uint32)*)write_handle.data() = item_size_uint32;
        memcpy(write_handle.data() + sizeof(item_size_uint32), handle.data(), item_size);
        write_with_cache_commit(write_handle);
    }

    static const bq::string log_file_ext_name_raw = ".lograw";
    bq::string appender_file_raw::get_file_ext_name()
    {
        return log_file_ext_name_raw;
    }

    uint32_t appender_file_raw::get_binary_format_version() const
    {
        return format_version;
    }

}
