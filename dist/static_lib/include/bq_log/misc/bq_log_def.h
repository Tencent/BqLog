#pragma once
/*
 * Copyright (C) 2024 Tencent.
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
        err_empty_ring_buffer, // no valid data to read in ring buffer;
        err_not_enough_space, // not enough space in ring buffer to alloc
        err_data_not_contiguous, // data is not contiguous, this error code is only used for internal statistics within the ring_buffer and will not be exposed externally.
        err_alloc_failed_by_race_condition, // alloc failed caused by multi-thread race condition, you can try again later.
        err_alloc_size_invalid, // invalid alloc size, too big or 0.
        err_buffer_not_inited, // buffer not initialized
        err_mmap_sync, //memory map sync error
        result_code_count
    };

    enum class log_reliable_level {
        /*low: when ring_buffer is full, new log will be discarded to make sure the invoke is not blocked.
         */
        low = 1,
        /*normal : When the remaining space in the ring_buffer is not sufficient to write new logs,
                        the thread calling the logging function will be blocked until the worker thread
                        processes a portion of the data from the ring_buffer and frees up space.
        */
        normal = 2,
        /*high: In addition to the behavior of 2 (normal), the logging system will try its best
                        to ensure that logs are not lost due to events such as crashes or power outages.
                        As long as the log object's configuration remains unchanged, upon restarting the program,
                        any logs that were not written to the disk previously due to the asynchronous write functionality will be recovered and written.
                        However, this feature comes at a cost, potentially causing a certain performance degradation.
                        But there's no need to worry; unless you're writing logs with a very high frequency at full I/O load,
                        this performance impact should be imperceptible.
                        (To ensure this feature works correctly, the running system must support memory mapping, such as Windows, Linux, Android, macOS, or iOS.)
        */
        high = 3
    };

    enum class log_thread_mode {
        sync, // synchronous mode
        async, // asynchronous mode, use public worker thread.
        independent // asynchronous mode, use independent worker thread.
    };

    extern "C" {
    BQ_STRUCT_PACK(struct _log_entry_head_def {
        uint64_t timestamp_epoch;
        uint32_t category_idx;
        uint8_t level;
        uint8_t log_format_str_type; // log_arg_type_enum::string_utf8_type or log_arg_type_enum::string_utf16_type
        uint32_t log_args_offset;
        uint32_t ext_info_offset;
        uint16_t dummy;
    });
    static_assert(sizeof(_log_entry_head_def) % 4 == 0
            && (sizeof(_log_entry_head_def) == sizeof(decltype(_log_entry_head_def::timestamp_epoch)) + sizeof(decltype(_log_entry_head_def::category_idx)) + sizeof(decltype(_log_entry_head_def::level)) + sizeof(decltype(_log_entry_head_def::log_format_str_type)) + sizeof(decltype(_log_entry_head_def::log_args_offset)) + sizeof(decltype(_log_entry_head_def::ext_info_offset)) + sizeof(decltype(_log_entry_head_def::dummy))),
        "_log_entry_head_def's memory layout must be packed!");

    BQ_STRUCT_PACK(struct _api_string_def {
        const char* str;
        uint32_t len;
    });
    static_assert(sizeof(_api_string_def) == sizeof(decltype(_api_string_def::str)) + sizeof(decltype(_api_string_def::len)), "_api_string_def's memory layout must be packed!");

    BQ_STRUCT_PACK(struct _api_u16string_def {
        const char16_t* str;
        uint32_t len;
    });
    static_assert(sizeof(_api_u16string_def) == sizeof(decltype(_api_u16string_def::str)) + sizeof(decltype(_api_u16string_def::len)), "_api_u16string_def's memory layout must be packed!");

    // this is C-linkage version of bq::ring_buffer_read_handle
    BQ_STRUCT_PACK(struct _api_ring_buffer_chunk_read_handle {
        uint8_t* data_addr;
        enum_buffer_result_code result;
    });

    // this is C-linkage version of bq::ring_buffer_write_handle
    BQ_STRUCT_PACK(struct _api_ring_buffer_chunk_write_handle {
        uint8_t* data_addr;
        enum_buffer_result_code result;
    });

    BQ_STRUCT_PACK(struct _api_console_log_data_head {
        uint64_t log_id;
        uint32_t category_idx;
        int32_t level;
        int32_t log_len;
        char log_str[1];
    });
    static_assert(sizeof(_api_console_log_data_head) == sizeof(decltype(_api_console_log_data_head::log_id)) + sizeof(decltype(_api_console_log_data_head::category_idx)) + sizeof(decltype(_api_console_log_data_head::level)) + sizeof(decltype(_api_console_log_data_head::log_len)) + sizeof(decltype(_api_console_log_data_head::log_str)), "_api_console_log_data_head's memory layout must be packed!");
    }

    struct console_log_item {
    public:
        uint64_t log_id;
        uint32_t category_idx;
        bq::log_level level;
        bq::string log_str;
    };

    struct _log_level_bitmap_def {
        uint32_t bitmap = 0;
        bool have_level(bq::log_level level)
        {
            return (bitmap & (1 << (int32_t)level)) != 0;
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
