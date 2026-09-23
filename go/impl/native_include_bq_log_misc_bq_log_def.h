/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
/*!
 * basic typedef for library header
 *
 * \brief
 *
 * \author pippocao
 * \date 2022.08.03
 */
#include <stdint.h>
#include <stddef.h>
#include "native_include_bq_common_bq_common_public_include.h"
#include "native_include_bq_log_misc_bq_log_c_types.h"

namespace bq {
    class log;
    namespace test {
        class test_log;
    }
}

namespace bq {
    enum class log_memory_policy {
        discard_when_full, // If the log_buffer is full, incoming logs will be discarded.
        block_when_full, // If the log_buffer is full, the logging thread will be blocked until space becomes available.
        auto_expand_when_full, // If the log_buffer is full, a new space will be allocated and the log will be written.
    };

    enum class log_thread_mode {
        sync, // synchronous mode, very slow.
        async, // asynchronous mode, use public worker thread.
        independent // asynchronous mode, use independent worker thread.
    };

    BQ_PACK_BEGIN
    struct alignas(8) _log_entry_head_def {
        uint64_t timestamp_epoch;
        uint32_t ext_info_offset;
        uint32_t category_idx;
        uint64_t log_thread_id;
        uint64_t format_hash;
        uint8_t log_format_str_type; // log_arg_type_enum::string_utf8_type or log_arg_type_enum::string_utf16_type
        uint8_t level;
        uint16_t padding;
        uint32_t log_format_data_len;

        static constexpr uint32_t get_head_size_without_format_str();
    } BQ_PACK_END constexpr uint32_t _log_entry_head_def::get_head_size_without_format_str()
    {
        return offsetof(_log_entry_head_def, log_format_str_type);
    }

    static_assert(_log_entry_head_def::get_head_size_without_format_str() == 32,
        "_log_entry_head_def::get_head_size_without_format_str() must equal 32");
    static_assert(sizeof(_log_entry_head_def) == 40,
        "_log_entry_head_def's memory layout must be packed!");

    // this is C-linkage version of bq::log_buffer_read_handle
    BQ_PACK_BEGIN
    struct _api_log_buffer_chunk_read_handle {
        uint8_t* format_data_addr;
        enum_buffer_result_code result;
    } BQ_PACK_END static_assert(sizeof(_api_log_buffer_chunk_read_handle) == sizeof(decltype(_api_log_buffer_chunk_read_handle::format_data_addr)) + sizeof(decltype(_api_log_buffer_chunk_read_handle::result)), "_api_log_buffer_chunk_read_handle's memory layout must be packed!");

    struct _log_level_bitmap_def {
        uint32_t bitmap = 0;
        bool have_level(bq::log_level level)
        {
            return (bitmap & static_cast<uint32_t>(1 << (int32_t)level)) != 0;
        }
    };

    template <uint32_t CAT_INDEX>
    struct log_category_base {
    };

    enum class log_arg_type_enum : uint8_t {
        unsupported_type,
        null_type,
        pointer_type,
        bool_type,
        char_type,
        char16_type,
        char32_type,
        int8_type,
        uint8_type,
        int16_type,
        uint16_type,
        int32_type,
        uint32_type,
        int64_type,
        uint64_type,
        float_type,
        double_type,
        string_utf8_type,
        string_utf16_type,
        string_utf32_type,
        string_utf_mixed_type
    };


}
