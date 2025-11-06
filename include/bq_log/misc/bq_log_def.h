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
#include "bq_common/bq_common_public_include.h"

namespace bq {
    class log;
    namespace test {
        class test_log;
    }
}

namespace bq {
    enum class enum_buffer_result_code {
        success = 0,
        err_empty_log_buffer, // no valid data to read in log buffer;
        err_not_enough_space, // not enough space in log buffer to alloc
        err_wait_and_retry, // need wait and try again
        err_data_not_contiguous, // data is not contiguous, this error code is only used for internal statistics within the log_buffer and will not be exposed externally.
        err_alloc_size_invalid, // invalid alloc size, too big or 0.
        err_buffer_not_inited, // buffer not initialized
        result_code_count
    };

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
        uint32_t log_args_offset;
        uint32_t ext_info_offset;
        uint16_t padding;
        uint8_t log_format_str_type; // log_arg_type_enum::string_utf8_type or log_arg_type_enum::string_utf16_type
        uint8_t level;
        uint32_t category_idx;
    } BQ_PACK_END static_assert(sizeof(_log_entry_head_def) % 8 == 0
            && (sizeof(_log_entry_head_def) == sizeof(decltype(_log_entry_head_def::timestamp_epoch)) + sizeof(decltype(_log_entry_head_def::category_idx)) + sizeof(decltype(_log_entry_head_def::level)) + sizeof(decltype(_log_entry_head_def::log_format_str_type)) + sizeof(decltype(_log_entry_head_def::log_args_offset)) + sizeof(decltype(_log_entry_head_def::ext_info_offset)) + sizeof(decltype(_log_entry_head_def::padding))),
        "_log_entry_head_def's memory layout must be packed!");
    static_assert(sizeof(_log_entry_head_def) == 24,
        "_log_entry_head_def's memory layout must be packed!");

    BQ_PACK_BEGIN
    struct alignas(4) _api_string_def {
        const char* str;
        uint32_t len;
    } BQ_PACK_END static_assert(sizeof(_api_string_def) == sizeof(decltype(_api_string_def::str)) + sizeof(decltype(_api_string_def::len)), "_api_string_def's memory layout must be packed!");

    BQ_PACK_BEGIN
    struct alignas(4) _api_u16string_def {
        const char16_t* str;
        uint32_t len;
    } BQ_PACK_END static_assert(sizeof(_api_u16string_def) == sizeof(decltype(_api_u16string_def::str)) + sizeof(decltype(_api_u16string_def::len)), "_api_u16string_def's memory layout must be packed!");

    // this is C-linkage version of bq::log_buffer_read_handle
    BQ_PACK_BEGIN
    struct alignas(4) _api_log_buffer_chunk_read_handle {
        uint8_t* data_addr;
        enum_buffer_result_code result;
    } BQ_PACK_END static_assert(sizeof(_api_log_buffer_chunk_read_handle) == sizeof(decltype(_api_log_buffer_chunk_read_handle::data_addr)) + sizeof(decltype(_api_log_buffer_chunk_read_handle::result)), "_api_log_buffer_chunk_read_handle's memory layout must be packed!");

    // this is C-linkage version of bq::log_buffer_write_handle
    BQ_PACK_BEGIN
    struct alignas(4) _api_log_buffer_chunk_write_handle {
        uint8_t* data_addr;
        enum_buffer_result_code result;
    } BQ_PACK_END static_assert(sizeof(_api_log_buffer_chunk_write_handle) == sizeof(decltype(_api_log_buffer_chunk_write_handle::data_addr)) + sizeof(decltype(_api_log_buffer_chunk_write_handle::result)), "_api_log_buffer_chunk_write_handle's memory layout must be packed!");

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
        string_utf16_type
    };

    enum appender_decode_result {
        success, // decoding successful, you can call the corresponding function to obtain the decoded log text.
        eof, // all the content is decoded
        failed_invalid_handle,
        failed_decode_error,
        failed_io_error
    };

    /// <summary>
    /// `content` is a C-style string and end with '\0';
    /// </summary>
    typedef void(BQ_STDCALL* type_func_ptr_console_callback)(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length);

    /// <summary>
    /// `content` is a C-style string and end with '\0';
    /// </summary>
    typedef void(BQ_STDCALL* type_func_ptr_console_buffer_fetch_callback)(void* pass_through_param, uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length);

}
