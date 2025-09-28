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
#include <assert.h>
#include "appender_file_raw.h"

namespace bq {
    void appender_file_raw::log_impl(const log_entry_handle& handle)
    {
        appender_file_base::log_impl(handle);
        uint32_t item_size = handle.data_size();
        auto write_handle = write_with_cache_alloc(sizeof(item_size) + item_size);
        *(decltype(item_size)*)write_handle.data() = item_size;
        memcpy(write_handle.data() + sizeof(item_size), handle.data(), item_size);
        write_with_cache_commit(write_handle);
    }

    bq::string appender_file_raw::get_file_ext_name()
    {
        return ".lograw";
    }

    uint32_t appender_file_raw::get_binary_format_version() const
    {
        return format_version;
    }

}
