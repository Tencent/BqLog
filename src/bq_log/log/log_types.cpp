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

#include "bq_log/log/log_types.h"

namespace bq {
    bool get_fast_arg_size(log_arg_type_enum type, const uint8_t* source, uint32_t available, fast_arg_size& result)
    {
        result = {};
        switch (type) {
        case log_arg_type_enum::null_type:
            result.target_size_ = 4;
            return true;
        case log_arg_type_enum::bool_type:
        case log_arg_type_enum::char_type:
        case log_arg_type_enum::int8_type:
        case log_arg_type_enum::uint8_type:
            result.source_size_ = 4;
            result.target_size_ = 4;
            result.value_offset_ = 2;
            result.value_size_ = 1;
            break;
        case log_arg_type_enum::char16_type:
        case log_arg_type_enum::int16_type:
        case log_arg_type_enum::uint16_type:
            result.source_size_ = 4;
            result.target_size_ = 4;
            result.value_offset_ = 2;
            result.value_size_ = 2;
            break;
        case log_arg_type_enum::char32_type:
        case log_arg_type_enum::int32_type:
        case log_arg_type_enum::uint32_type:
        case log_arg_type_enum::float_type:
            result.source_size_ = 4;
            result.target_size_ = 8;
            result.value_offset_ = 4;
            result.value_size_ = 4;
            break;
        case log_arg_type_enum::int64_type:
        case log_arg_type_enum::uint64_type:
        case log_arg_type_enum::double_type:
        case log_arg_type_enum::pointer_type:
            result.source_size_ = 8;
            result.target_size_ = 12;
            result.value_offset_ = 4;
            result.value_size_ = 8;
            break;
        case log_arg_type_enum::string_utf8_type:
        case log_arg_type_enum::string_utf16_type: {
            if (available < 4) {
                return false;
            }
            uint32_t length = 0;
            memcpy(&length, source, sizeof(length));
            const uint64_t total = sizeof(uint32_t) + bq::align_4(static_cast<size_t>(length));
            if (total > available || total > UINT32_MAX - 4) {
                return false;
            }
            result.source_size_ = static_cast<uint32_t>(total);
            result.target_size_ = result.source_size_ + 4;
            result.value_offset_ = 4;
            result.value_size_ = result.source_size_;
            break;
        }
        default:
            return false;
        }
        return result.source_size_ <= available;
    }

    bool validate_fast_args(const fast_format_meta& format, const uint8_t* args, uint32_t args_size)
    {
        const uint8_t* arg_types = format.arg_types();
        uint32_t cursor = 0;
        for (uint16_t i = 0; i < format.arg_count; ++i) {
            fast_arg_size field;
            if (!get_fast_arg_size(static_cast<log_arg_type_enum>(arg_types[i]), args + cursor, args_size - cursor, field)) {
                return false;
            }
            cursor += field.source_size_;
        }
        return cursor == args_size;
    }

    void log_entry_handle::write_standard_args(uint8_t* target) const
    {
        const uint8_t* arg_types = fast_format_->arg_types();
        uint32_t cursor = 0;
        for (uint16_t i = 0; i < fast_format_->arg_count; ++i) {
            const uint8_t type = arg_types[i];
            fast_arg_size field;
            if (!get_fast_arg_size(static_cast<log_arg_type_enum>(type), fast_args_ + cursor, fast_args_size_ - cursor, field)) {
                return;
            }
            const uint32_t type_word = type;
            memcpy(target, &type_word, sizeof(type_word));
            if (field.value_size_) {
                memcpy(target + field.value_offset_, fast_args_ + cursor, field.value_size_);
            }
            cursor += field.source_size_;
            target += field.target_size_;
        }
    }

    bool log_entry_handle::validate() const
    {
        if (data_len < sizeof(_log_entry_head_def)) {
            return false;
        }
        const auto& h = get_log_head();

        const auto fmt_type = static_cast<log_arg_type_enum>(h.log_format_str_type);
        if (fmt_type != log_arg_type_enum::string_utf8_type
            && fmt_type != log_arg_type_enum::string_utf16_type
            && fmt_type != log_arg_type_enum::string_utf32_type) {
            return false;
        }

        const uint64_t fmt_len = h.log_format_data_len;
        if (fast_format_) {
            return external_ext_head_ && fmt_len == fast_format_->format_size
                && validate_fast_args(*fast_format_, fast_args_, fast_args_size_);
        }
        const uint64_t args_offset_u64 = sizeof(_log_entry_head_def) + ((fmt_len + static_cast<uint64_t>(3)) & ~static_cast<uint64_t>(3));
        if (args_offset_u64 > data_len) {
            return false;
        }
        const uint32_t args_offset = static_cast<uint32_t>(args_offset_u64);

        uint32_t args_end = data_len;
        const uint32_t ext_off = h.ext_info_offset;
        if (ext_off == 0) {
            if (!external_ext_head_) {
                return false;
            }
        } else {
            if (ext_off < args_offset
                || static_cast<uint64_t>(ext_off) + sizeof(_log_entry_ext_head_def) > data_len) {
                return false;
            }
            const auto& ext = *reinterpret_cast<const _log_entry_ext_head_def*>(data_ptr + ext_off);
            if (static_cast<uint64_t>(ext_off) + sizeof(_log_entry_ext_head_def) + ext.thread_name_len_ > data_len) {
                return false;
            }
            args_end = ext_off;
        }

        const uint8_t* args = data_ptr + args_offset;
        const uint32_t args_len = args_end - args_offset;
        uint32_t cursor = 0;
        while (cursor < args_len) {
            if (cursor + static_cast<uint32_t>(4) > args_len) {
                return false;
            }
            const auto type = static_cast<log_arg_type_enum>(args[cursor]);
            uint32_t step = 0;
            switch (type) {
            case log_arg_type_enum::null_type:
            case log_arg_type_enum::bool_type:
            case log_arg_type_enum::char_type:
            case log_arg_type_enum::char16_type:
            case log_arg_type_enum::int8_type:
            case log_arg_type_enum::uint8_type:
            case log_arg_type_enum::int16_type:
            case log_arg_type_enum::uint16_type:
                step = static_cast<uint32_t>(4);
                break;
            case log_arg_type_enum::char32_type:
            case log_arg_type_enum::int32_type:
            case log_arg_type_enum::uint32_type:
            case log_arg_type_enum::float_type:
                step = static_cast<uint32_t>(4) + static_cast<uint32_t>(sizeof(int32_t));
                break;
            case log_arg_type_enum::int64_type:
            case log_arg_type_enum::uint64_type:
            case log_arg_type_enum::double_type:
            case log_arg_type_enum::pointer_type:
                step = static_cast<uint32_t>(4) + static_cast<uint32_t>(sizeof(int64_t));
                break;
            case log_arg_type_enum::string_utf8_type:
            case log_arg_type_enum::string_utf16_type: {
                if (cursor + static_cast<uint32_t>(4) + sizeof(uint32_t) > args_len) {
                    return false;
                }
                uint32_t str_len = *(const uint32_t*)(args + cursor + static_cast<uint32_t>(4));
                const uint64_t step_u64 = static_cast<uint64_t>(4) + sizeof(uint32_t) + ((static_cast<uint64_t>(str_len) + static_cast<uint64_t>(3)) & ~static_cast<uint64_t>(3));
                if (step_u64 > args_len - cursor) {
                    return false;
                }
                step = static_cast<uint32_t>(step_u64);
            } break;
            case log_arg_type_enum::string_utf_mixed_type:
            case log_arg_type_enum::string_utf32_type:
            case log_arg_type_enum::unsupported_type:
            default:
                return false;
            }
            if (cursor + step > args_len) {
                return false;
            }
            cursor += step;
        }
        return cursor == args_len;
    }
}
