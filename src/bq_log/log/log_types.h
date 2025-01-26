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
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_def.h"
namespace bq {
    struct log_entry_handle {
    private:
        const uint8_t* data_ptr;
        uint32_t data_len;

    public:
        log_entry_handle(const uint8_t* in_data_ptr, uint32_t in_data_len)
            : data_ptr(in_data_ptr)
            , data_len(in_data_len)
        {
        }

        bq_forceinline const uint8_t* data() const
        {
            return data_ptr;
        }

        bq_forceinline uint32_t data_size() const
        {
            return data_len;
        }

        bq_forceinline const char* get_format_string_data() const
        {
            return reinterpret_cast<const char*>(data_ptr) + sizeof(_log_entry_head_def);
        }

        bq_forceinline _log_entry_head_def& get_log_head()
        {
            return *(_log_entry_head_def*)data_ptr;
        }

        bq_forceinline const _log_entry_head_def& get_log_head() const
        {
            return *(const _log_entry_head_def*)data_ptr;
        }

        bq_forceinline const struct ext_log_entry_info_head& get_ext_head() const
        {
            return *(struct ext_log_entry_info_head*)(data_ptr + get_log_head().ext_info_offset);
        }

        bq_forceinline uint32_t get_log_args_data_size() const
        {
            return (int32_t)(get_log_head().ext_info_offset - get_log_head().log_args_offset);
        }

        bq_forceinline const uint8_t* get_log_args_data() const
        {
            return data_ptr + (size_t)get_log_head().log_args_offset;
        }

        bq_forceinline bq::log_level get_level() const
        {
            return (bq::log_level)(get_log_head().level);
        }

        bq_forceinline uint32_t get_category_idx() const
        {
            return (get_log_head().category_idx);
        }
    };

    BQ_STRUCT_PACK(struct ext_log_entry_info_head {
        uint64_t thread_id_;
        uint8_t thread_name_len_;
    });
    static_assert(sizeof(ext_log_entry_info_head::thread_id_) == sizeof(bq::platform::thread::thread_id), "thread_id size error!");
}
