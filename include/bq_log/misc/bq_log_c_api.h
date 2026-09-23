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
#pragma once
/*!
 * C-linkage API of the BqLog dynamic library, dual mode:
 *  - C++: redirects to bq_log/misc/bq_log_api.h (namespace bq::api).
 *  - C: mirrors the ABI types below and declares the shared entry list
 *    bq_log_c_api.inc as plain extern prototypes at global scope.
 * The enum class/enum types of the C++ header are mirrored as int32_t-based
 * typedefs and prefixed constants with identical numeric values, so no C++
 * call site changes are needed and values pass across the ABI directly.
 *
 * \author pippocao
 * \date 2026.09.23
 */
#include <stdint.h>

#if defined(__cplusplus)

#include "bq_log/misc/bq_log_api.h"

#else

#include <stdbool.h>
#include "bq_common/platform/macros.h"

typedef int32_t log_level;
enum {
    log_level_verbose = 0,
    log_level_debug,
    log_level_info,
    log_level_warning,
    log_level_error,
    log_level_fatal,
    log_level_max = 32
};

typedef int32_t enum_buffer_result_code;
enum {
    enum_buffer_result_code_success = 0,
    enum_buffer_result_code_err_empty_log_buffer,
    enum_buffer_result_code_err_not_enough_space,
    enum_buffer_result_code_err_wait_and_retry,
    enum_buffer_result_code_err_data_not_contiguous,
    enum_buffer_result_code_err_alloc_size_invalid,
    enum_buffer_result_code_err_buffer_not_inited,
    enum_buffer_result_code_err_io_failure_drop,
    enum_buffer_result_code_count
};

typedef enum appender_decode_result {
    appender_decode_result_success = 0,
    appender_decode_result_eof,
    appender_decode_result_failed_invalid_handle,
    appender_decode_result_failed_decode_error,
    appender_decode_result_failed_io_error
} appender_decode_result;

BQ_PACK_BEGIN
struct _api_string_def {
    const char* str;
    uint32_t len;
} BQ_PACK_END
typedef struct _api_string_def _api_string_def;

BQ_PACK_BEGIN
struct _api_u16string_def {
    const uint16_t* str;
    uint32_t len;
} BQ_PACK_END
typedef struct _api_u16string_def _api_u16string_def;

BQ_PACK_BEGIN
struct _api_log_write_handle {
    uint8_t* format_data_addr;
    enum_buffer_result_code result;
} BQ_PACK_END
typedef struct _api_log_write_handle _api_log_write_handle;

typedef void(BQ_STDCALL* type_func_ptr_console_callback)(uint64_t log_id, int32_t category_idx, log_level log_level_value, const char* content, int32_t length);
typedef void(BQ_STDCALL* type_func_ptr_console_buffer_fetch_callback)(void* pass_through_param, uint64_t log_id, int32_t category_idx, log_level log_level_value, const char* content, int32_t length);

#undef BQ_API_DEF
#define BQ_API_DEF(return_type, name, parameters, arguments) extern return_type name parameters
#include "bq_log/misc/bq_log_c_api.inc"
#undef BQ_API_DEF

#endif
