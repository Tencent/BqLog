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
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_def.h"
namespace bq {
    struct fast_arg_size {
        uint32_t source_size_;
        uint32_t target_size_;
        uint32_t value_offset_;
        uint32_t value_size_;
    };

    // sizes of one fast layout argument and of its standard layout form
    bool get_fast_arg_size(log_arg_type_enum type, const uint8_t* source, uint32_t available, fast_arg_size& result);

    bool validate_fast_args(const fast_format_meta& format, const uint8_t* args, uint32_t args_size);

    // standard layout: a 4 byte type word per argument, 1 and 2 byte values in its upper half, wider values after it
    class standard_args_cursor {
        const uint8_t* cursor_;
        const uint8_t* end_;

    public:
        standard_args_cursor(const uint8_t* begin, uint32_t size)
            : cursor_(begin)
            , end_(begin + size)
        {
        }
        bq_forceinline bool has_next() const { return cursor_ < end_; }
        bq_forceinline bool in_bounds() const { return cursor_ <= end_; }
        bq_forceinline uint8_t type() const { return *cursor_; }
        bq_forceinline const uint8_t* small_value() const { return cursor_ + 2; }
        bq_forceinline const uint8_t* value() const { return cursor_ + 4; }
        bq_forceinline void skip_null() { cursor_ += 4; }
        bq_forceinline void skip_small() { cursor_ += 4; }
        bq_forceinline void skip_value(uint32_t value_size) { cursor_ += 4 + value_size; }
        bq_forceinline void skip_all() { cursor_ = end_; }
    };

    // fast layout: types come from the format metadata, values are packed in 4 byte slots without type words
    class fast_args_cursor {
        const uint8_t* cursor_;
        const uint8_t* type_;
        const uint8_t* type_end_;

    public:
        fast_args_cursor(const uint8_t* args, const uint8_t* types, uint32_t count)
            : cursor_(args)
            , type_(types)
            , type_end_(types + count)
        {
        }
        bq_forceinline bool has_next() const { return type_ < type_end_; }
        bq_forceinline bool in_bounds() const { return type_ <= type_end_; }
        bq_forceinline uint8_t type() const { return *type_; }
        bq_forceinline const uint8_t* small_value() const { return cursor_; }
        bq_forceinline const uint8_t* value() const { return cursor_; }
        bq_forceinline void skip_null() { ++type_; }
        bq_forceinline void skip_small()
        {
            cursor_ += 4;
            ++type_;
        }
        bq_forceinline void skip_value(uint32_t value_size)
        {
            cursor_ += value_size;
            ++type_;
        }
        bq_forceinline void skip_all() { type_ = type_end_; }
    };

    struct log_entry_handle {
    private:
        const uint8_t* data_ptr;
        uint32_t data_len;
        const char* format_ptr_;
        // for records without ext info (ext_info_offset == 0)
        const struct _log_entry_ext_head_def* external_ext_head_ = nullptr;
        // fast layout records: data_ptr holds only the standard head, format and arguments stay where the producer put them
        const fast_format_meta* fast_format_ = nullptr;
        const uint8_t* fast_args_ = nullptr;
        uint32_t fast_args_size_ = 0;

    public:
        log_entry_handle(const uint8_t* in_data_ptr, uint32_t in_data_len)
            : data_ptr(in_data_ptr)
            , data_len(in_data_len)
            , format_ptr_(reinterpret_cast<const char*>(in_data_ptr) + sizeof(_log_entry_head_def))
        {
        }

        bq_forceinline void set_fast_layout(const fast_format_meta* format, const uint8_t* args, uint32_t args_size)
        {
            format_ptr_ = reinterpret_cast<const char*>(format->format_data());
            fast_format_ = format;
            fast_args_ = args;
            fast_args_size_ = args_size;
        }

        bq_forceinline bool is_fast_layout() const
        {
            return fast_format_ != nullptr;
        }

        bq_forceinline const uint8_t* get_fast_args_data() const
        {
            return fast_args_;
        }

        bq_forceinline standard_args_cursor get_standard_args() const
        {
            return standard_args_cursor(get_log_args_data(), get_log_args_data_size());
        }

        bq_forceinline fast_args_cursor get_fast_args() const
        {
            return fast_args_cursor(fast_args_, fast_format_->arg_types(), fast_format_->arg_count);
        }

        // every fast argument gains exactly one 4 byte type word in the standard layout
        bq_forceinline uint32_t get_standard_args_size() const
        {
            return fast_format_ ? fast_args_size_ + static_cast<uint32_t>(fast_format_->arg_count) * 4 : get_log_args_data_size();
        }

        // writes the arguments of a fast layout record in the standard layout, get_standard_args_size() bytes
        void write_standard_args(uint8_t* target) const;

        bq_forceinline void set_external_ext_head(const struct _log_entry_ext_head_def* ext_head)
        {
            external_ext_head_ = ext_head;
        }

        bq_forceinline bool has_inline_ext_info() const
        {
            return get_log_head().ext_info_offset != 0;
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
            return format_ptr_;
        }

        bq_forceinline _log_entry_head_def& get_log_head()
        {
            return *const_cast<_log_entry_head_def*>((const _log_entry_head_def*)data_ptr);
        }

        bq_forceinline const _log_entry_head_def& get_log_head() const
        {
            return *(const _log_entry_head_def*)data_ptr;
        }

        bq_forceinline const struct _log_entry_ext_head_def& get_ext_head() const
        {
            const uint32_t ext_info_offset = get_log_head().ext_info_offset;
            return ext_info_offset ? *(const struct _log_entry_ext_head_def*)(data_ptr + ext_info_offset) : *external_ext_head_;
        }

        bq_forceinline size_t get_log_args_offset() const
        {
            return sizeof(_log_entry_head_def) + bq::align_4(static_cast<size_t>(get_log_head().log_format_data_len));
        }

        bq_forceinline const uint8_t* get_log_args_data() const
        {
            return data_ptr + get_log_args_offset();
        }

        bq_forceinline uint32_t get_log_args_data_size() const
        {
            const uint32_t ext_info_offset = get_log_head().ext_info_offset;
            return (ext_info_offset ? ext_info_offset : data_len) - static_cast<uint32_t>(get_log_args_offset());
        }

        bq_forceinline bq::log_level get_level() const
        {
            return static_cast<bq::log_level>(get_log_head().level);
        }

        bq_forceinline uint32_t get_category_idx() const
        {
            return get_log_head().category_idx;
        }

        bool validate() const;
    };

    BQ_PACK_BEGIN
    struct _log_entry_ext_head_def {
        uint8_t thread_name_len_;
    } BQ_PACK_END static_assert(sizeof(_log_entry_ext_head_def) == sizeof(decltype(_log_entry_ext_head_def::thread_name_len_)), "_log_entry_ext_head_def's memory layout must be packed!");

}
