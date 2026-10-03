/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
#pragma once
#include <stdint.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/basic_types.h"

#if defined(__cplusplus)
namespace bq {
    enum class enum_buffer_result_code : int32_t {
        success = 0,
        err_empty_log_buffer, // no valid data to read in log buffer;
        err_not_enough_space, // not enough space in log buffer to alloc
        err_wait_and_retry, // need wait and try again
        err_data_not_contiguous, // data is not contiguous, this error code is only used for internal statistics within the log_buffer and will not be exposed externally.
        err_alloc_size_invalid, // invalid alloc size, too big or 0.
        err_buffer_not_inited, // buffer not initialized
        err_io_failure_drop, // unrecoverable IO failure (e.g. disk-full when creating a recovery-mode mmap-backed oversize buffer); the producer must drop this entry immediately. Waiting is futile - no consumer can ever free disk blocks. This error code MUST NEVER be translated to err_wait_and_retry by any layer.
        err_fast_path_failed, // fallback to normal path
        result_code_count
    };

    enum appender_decode_result : int32_t {
        success, // decoding successful, you can call the corresponding function to obtain the decoded log text.
        eof, // all the content is decoded
        failed_invalid_handle,
        failed_decode_error,
        failed_io_error
    };
#define BQ_LOG_ABI_TYPE(cpp_type, c_type) cpp_type
#define BQ_LOG_ABI_CHAR16 char16_t
#else
#include <stdbool.h>
typedef int32_t bq_buffer_result_code;
enum bq_buffer_result_code_value {
    bq_buffer_result_code_success = 0,
    bq_buffer_result_code_err_empty_log_buffer,
    bq_buffer_result_code_err_not_enough_space,
    bq_buffer_result_code_err_wait_and_retry,
    bq_buffer_result_code_err_data_not_contiguous,
    bq_buffer_result_code_err_alloc_size_invalid,
    bq_buffer_result_code_err_buffer_not_inited,
    bq_buffer_result_code_err_io_failure_drop,
    bq_buffer_result_code_err_fast_path_failed,
    bq_buffer_result_code_count
};
typedef int32_t bq_appender_decode_result;
enum bq_appender_decode_result_value {
    bq_appender_decode_result_success = 0,
    bq_appender_decode_result_eof,
    bq_appender_decode_result_failed_invalid_handle,
    bq_appender_decode_result_failed_decode_error,
    bq_appender_decode_result_failed_io_error
};
#define BQ_LOG_ABI_TYPE(cpp_type, c_type) c_type
#define BQ_LOG_ABI_CHAR16 uint16_t
#endif

BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_string_def, bq_api_string_def) {
    const char* str;
    uint32_t len;
} BQ_PACK_END

BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_u16string_def, bq_api_u16string_def) {
    const BQ_LOG_ABI_CHAR16* str;
    uint32_t len;
} BQ_PACK_END

BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_log_write_handle, bq_api_log_write_handle) {
    uint8_t* format_data_addr;
    BQ_LOG_ABI_TYPE(enum_buffer_result_code, bq_buffer_result_code) result;
} BQ_PACK_END

BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_fast_log_site_handle, bq_api_fast_log_site_handle) {
    uint64_t format_meta_addr;
    uint64_t buffer_id; // compared with the thread's current buffer id on the inline fast path
    uint64_t log_id;
    void* buffer_ptr;
    // bit (1 << level) set when the bound log has that level enabled without stack trace for the site's category.
    // Kept up to date by the library; an unregistered site points at a constant 0.
    const uint32_t* level_word;
} BQ_PACK_END

// Byte offsets inside the SISO head and chunk format, for the header-inlined fast path.
// Plain uint32_t fields only; new fields may only be appended. All offsets are computed by the library.
// Bump the version whenever the meaning of any field, the thread state below, or the chunk/record format changes.
#define BQ_FAST_LOG_LAYOUT_VERSION 3
BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_fast_log_layout, bq_api_fast_log_layout) {
    uint32_t struct_size;
    uint32_t layout_version;
    uint32_t head_wt_reading_cursor_cache; // head: uint32_t
    uint32_t head_wt_writing_cursor_cache; // head: uint32_t
    uint32_t head_writing_cursor; // head: uint32_t, published with release
    uint32_t head_reading_cursor; // head: uint32_t, consumer progress, read with acquire
    uint32_t chunk_data_offset; // bytes from a chunk start to its payload
    uint32_t block_size_log2; // log2 of the SISO allocation unit in bytes
} BQ_PACK_END

// Per-thread state of the header-inlined fast path. The header owns it in thread local storage; the library writes it,
// on the owning thread only, whenever that thread's current HP block of the bound log buffer changes.
// Natural layout (not packed): read on the hot path.
struct BQ_LOG_ABI_TYPE(_api_fast_log_thread_state, bq_api_fast_log_thread_state) {
    uint64_t buffer_key; // ~id of the log buffer described below; 0 when there is none (never matches a site)
    uint8_t* siso_head; // head of the current block's siso_ring_buffer
    uint8_t* siso_units; // first allocation unit of that ring
    const bool* need_reallocate; // the current block's flag, set by the consumer; read on every write. Never null:
                                 // with buffer_key 0 it points at a constant, so a write may read it unconditionally
    uint32_t unit_count; // power of two
    uint32_t half_unit_count;
};

BQ_PACK_BEGIN
struct BQ_LOG_ABI_TYPE(_api_fast_log_write_handle, bq_api_fast_log_write_handle) {
    uint8_t* args_addr;
    BQ_LOG_ABI_TYPE(enum_buffer_result_code, bq_buffer_result_code) result;
} BQ_PACK_END

typedef void(BQ_STDCALL* BQ_LOG_ABI_TYPE(type_func_ptr_console_callback, bq_console_callback))(
    uint64_t log_id, int32_t category_idx, BQ_LOG_ABI_TYPE(log_level, bq_log_level) level, const char* content, int32_t length);
typedef void(BQ_STDCALL* BQ_LOG_ABI_TYPE(type_func_ptr_console_buffer_fetch_callback, bq_console_buffer_fetch_callback))(
    void* pass_through_param, uint64_t log_id, int32_t category_idx, BQ_LOG_ABI_TYPE(log_level, bq_log_level) level, const char* content, int32_t length);

#undef BQ_LOG_ABI_TYPE
#undef BQ_LOG_ABI_CHAR16
#if defined(__cplusplus)
    static_assert(sizeof(_api_string_def) == sizeof(void*) + sizeof(uint32_t), "packed string ABI");
    static_assert(sizeof(_api_u16string_def) == sizeof(void*) + sizeof(uint32_t), "packed UTF-16 string ABI");
    static_assert(sizeof(_api_log_write_handle) == sizeof(void*) + sizeof(int32_t), "packed write handle ABI");
    static_assert(sizeof(_api_fast_log_site_handle) == 3 * sizeof(uint64_t) + 2 * sizeof(void*), "fast log site handle ABI");
    static_assert(sizeof(_api_fast_log_layout) == 8 * sizeof(uint32_t), "fast log layout ABI");
    static_assert(sizeof(void*) != 8 || sizeof(_api_fast_log_thread_state) == 40, "fast log thread state ABI");
    static_assert(sizeof(_api_fast_log_write_handle) == sizeof(void*) + sizeof(int32_t), "fast log write handle ABI");
    static_assert(sizeof(log_level) == sizeof(int32_t), "32-bit log level ABI");
    static_assert(sizeof(enum_buffer_result_code) == sizeof(int32_t), "32-bit buffer result ABI");
    static_assert(sizeof(appender_decode_result) == sizeof(int32_t), "32-bit decoder result ABI");
}
#else
typedef struct bq_api_string_def bq_api_string_def;
typedef struct bq_api_u16string_def bq_api_u16string_def;
typedef struct bq_api_log_write_handle bq_api_log_write_handle;
typedef struct bq_api_fast_log_site_handle bq_api_fast_log_site_handle;
typedef struct bq_api_fast_log_layout bq_api_fast_log_layout;
typedef struct bq_api_fast_log_thread_state bq_api_fast_log_thread_state;
typedef struct bq_api_fast_log_write_handle bq_api_fast_log_write_handle;
#endif
