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
#if !defined(BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED)
#include <stdint.h>
#include <stddef.h>
#endif
#include "bq_common/bq_common_public_include.h"
#include "bq_log/misc/bq_log_c_types.h"

namespace bq {
    // The first word is epoch milliseconds in both heads; its high bit marks fast records.
    static constexpr uint64_t log_head_fast_flag = UINT64_C(1) << 63;

    BQ_MODULE_EXPORT_BEGIN
    struct log_head_base_def {
        static bq_forceinline uint64_t get_epoch(uint64_t value)
        {
            return value & ~log_head_fast_flag;
        }

        static bq_forceinline bool is_fast(uint64_t value)
        {
            return (value & log_head_fast_flag) != 0;
        }

        static bq_forceinline uint64_t set_fast(uint64_t epoch)
        {
            return epoch | log_head_fast_flag;
        }
    };
    class log;
    namespace test {
        class test_log;
        class test_fast_log;
    }
    BQ_MODULE_EXPORT_END
}

namespace bq {
    BQ_MODULE_EXPORT_BEGIN
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
    struct alignas(8) _log_entry_head_def : log_head_base_def {
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
    } BQ_PACK_END
        BQ_MODULE_EXPORT_END

        // A member definition may not appear in an export block, so it sits between the markers.
        constexpr uint32_t _log_entry_head_def::get_head_size_without_format_str()
    {
        return offsetof(_log_entry_head_def, log_format_str_type);
    }
    BQ_MODULE_EXPORT_BEGIN

    static_assert(_log_entry_head_def::get_head_size_without_format_str() == 32,
        "_log_entry_head_def::get_head_size_without_format_str() must equal 32");
    static_assert(sizeof(_log_entry_head_def) == 40,
        "_log_entry_head_def's memory layout must be packed!");

    BQ_PACK_BEGIN
    struct alignas(8) log_head_fast_def : log_head_base_def {
        uint64_t timestamp_epoch;
        uint64_t format_meta_addr;
    } BQ_PACK_END

        static_assert(sizeof(log_head_fast_def) == 16, "log_head_fast_def size");
    static_assert(offsetof(log_head_fast_def, format_meta_addr) == 8, "log_head_fast_def alignment");

    enum class fast_meta_kind : uint8_t {
        format = 1,
        oversize = 2 // unused since 2.7.0
    };

    BQ_PACK_BEGIN
    struct alignas(8) fast_meta_head {
        uint64_t checksum;
        uint32_t record_size;
        fast_meta_kind kind;
        uint8_t ready;
        uint16_t reserved;
    } BQ_PACK_END static_assert(sizeof(fast_meta_head) == 16, "fast meta head size");

    BQ_PACK_BEGIN
    struct alignas(8) fast_format_meta {
        fast_meta_head head;
        uint64_t format_hash;
        uint32_t category_idx;
        uint32_t format_size;
        uint16_t arg_count;
        uint8_t level;
        uint8_t format_type;
        uint32_t reserved;

        const uint8_t* format_data() const { return reinterpret_cast<const uint8_t*>(this) + sizeof(*this); }
        const uint8_t* arg_types() const { return format_data() + format_size; }
    } BQ_PACK_END static_assert(sizeof(fast_format_meta) == 40, "fast format meta size");
    static_assert(offsetof(fast_format_meta, format_hash) == 16, "fast format hash alignment");
    static_assert(offsetof(fast_format_meta, category_idx) == 24, "fast format category alignment");
    static_assert(offsetof(fast_format_meta, format_size) == 28, "fast format size alignment");

    // Meta arena entries are aligned to this boundary. The fast_info site packs the enabled level
    // mask into the low bits of the meta address it publishes (see fast_info_site), so every
    // consumer must mask the low bits off before using the address stored in a record head as a
    // pointer. The macro path stores a clean address, for which the mask is a no-op.
    BQ_MODULE_EXPORT_END
    constexpr uint64_t fast_meta_addr_align() { return 64; }
    constexpr uint64_t fast_meta_addr_mask() { return fast_meta_addr_align() - 1; }
    BQ_MODULE_EXPORT_BEGIN

    // zero level words for an invalid log (all levels disabled); a template static member
    // so the single array definition in a public header never multiplies across TUs
    template <typename T = void>
    struct log_zero_level_words {
        static const uint32_t value[1];
    };
    BQ_MODULE_EXPORT_END

    // A static data member definition may not appear in an export block.
    template <typename T>
    const uint32_t log_zero_level_words<T>::value[1] = { 0 };
    BQ_MODULE_EXPORT_BEGIN

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

    // A generated category type (log.cat.xxx) derives from log_category_base<CAT_INDEX>.
    // 0: not a category, otherwise category index + 1.
    template <typename T>
    struct log_category_index {
        template <uint32_t CAT_INDEX>
        static char (&pick(const bq::log_category_base<CAT_INDEX>*))[CAT_INDEX + 2];
        static char (&pick(...))[1];
        static constexpr uint32_t value = static_cast<uint32_t>(sizeof(pick(static_cast<const bq::decay_t<T>*>(nullptr)))) - 1;
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
    BQ_MODULE_EXPORT_END
}
