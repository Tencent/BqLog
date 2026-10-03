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
#include "bq_log/log/log_record_reader.h"

namespace bq {
    namespace {
        struct fast_arg_size {
            uint32_t source_size;
            uint32_t target_size;
            uint32_t value_offset;
            uint32_t value_size;
        };

        bool get_fast_arg_size(log_arg_type_enum type, const uint8_t* source,
            uint32_t available, fast_arg_size& result)
        {
            result = {};
            switch (type) {
            case log_arg_type_enum::null_type:
                result.target_size = 4;
                return true;
            case log_arg_type_enum::bool_type:
            case log_arg_type_enum::char_type:
            case log_arg_type_enum::int8_type:
            case log_arg_type_enum::uint8_type:
                result.source_size = 4;
                result.target_size = 4;
                result.value_offset = 2;
                result.value_size = 1;
                break;
            case log_arg_type_enum::char16_type:
            case log_arg_type_enum::int16_type:
            case log_arg_type_enum::uint16_type:
                result.source_size = 4;
                result.target_size = 4;
                result.value_offset = 2;
                result.value_size = 2;
                break;
            case log_arg_type_enum::char32_type:
            case log_arg_type_enum::int32_type:
            case log_arg_type_enum::uint32_type:
            case log_arg_type_enum::float_type:
                result.source_size = 4;
                result.target_size = 8;
                result.value_offset = 4;
                result.value_size = 4;
                break;
            case log_arg_type_enum::int64_type:
            case log_arg_type_enum::uint64_type:
            case log_arg_type_enum::double_type:
            case log_arg_type_enum::pointer_type:
                result.source_size = 8;
                result.target_size = 12;
                result.value_offset = 4;
                result.value_size = 8;
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
                result.source_size = static_cast<uint32_t>(total);
                result.target_size = result.source_size + 4;
                result.value_offset = 4;
                result.value_size = result.source_size;
                break;
            }
            default:
                return false;
            }
            return result.source_size <= available;
        }
    }

    const log_thread_info* log_record_reader::get_block_thread_info(const log_buffer& buffer)
    {
        const log_thread_info* info = buffer.get_current_reading_thread_info();
        return (info && info->thread_name_len_ <= log_thread_info::MAX_THREAD_NAME_LEN) ? info : nullptr;
    }

    bool log_record_reader::is_fast_record(const uint8_t* data, uint32_t size)
    {
        return size >= sizeof(uint64_t)
            && log_head_base_def::is_fast(*reinterpret_cast<const uint64_t*>(data));
    }

    uint64_t log_record_reader::get_timestamp(const uint8_t* data, uint32_t size)
    {
        return size >= sizeof(uint64_t)
            ? log_head_base_def::get_epoch(*reinterpret_cast<const uint64_t*>(data))
            : 0;
    }

    bool log_record_reader::read(log_buffer& buffer, const uint8_t* data, uint32_t size,
        bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>>& converted_data,
        log_entry_handle& output)
    {
        if (!is_fast_record(data, size)) {
            output = log_entry_handle(data, size);
            if (size < sizeof(_log_entry_head_def) || output.has_inline_ext_info()) {
                return true;
            }
            const log_thread_info* owner = get_block_thread_info(buffer);
            if (!owner) {
                return false;
            }
            output.get_log_head().log_thread_id = owner->thread_id_;
            output.set_external_ext_head(reinterpret_cast<const _log_entry_ext_head_def*>(&owner->thread_name_len_));
            return true;
        }
        if (size < sizeof(log_head_fast_def)) {
            return false;
        }
        const auto& head = *reinterpret_cast<const log_head_fast_def*>(data);
        const log_thread_info* thread = get_block_thread_info(buffer);
        if (!thread) {
            return false;
        }
        const uint16_t version = buffer.get_current_reading_version();
        const fast_format_meta* format = nullptr;
        if (version == buffer.get_version()) {
            format = reinterpret_cast<const fast_format_meta*>(
                static_cast<uintptr_t>(head.format_meta_addr));
        } else {
            format = reinterpret_cast<const fast_format_meta*>(
                buffer.resolve_fast_log_meta(version, head.format_meta_addr));
        }
        if (!format || format->head.kind != fast_meta_kind::format) {
            return false;
        }
        const uint8_t* args = data + sizeof(head);
        const uint32_t args_size = size - static_cast<uint32_t>(sizeof(head));
        uint64_t legacy_args_size = 0;
        uint32_t cursor = 0;
        for (uint16_t i = 0; i < format->arg_count; ++i) {
            fast_arg_size field;
            if (!get_fast_arg_size(static_cast<log_arg_type_enum>(format->arg_types()[i]),
                    args + cursor, args_size - cursor, field)) {
                return false;
            }
            cursor += field.source_size;
            legacy_args_size += field.target_size;
        }
        if (cursor != args_size) {
            return false;
        }
        const uint64_t args_offset = sizeof(_log_entry_head_def)
            + bq::align_4(static_cast<size_t>(format->format_size));
        const uint64_t total_size = args_offset + legacy_args_size;
        if (total_size > UINT32_MAX) {
            return false;
        }
        converted_data.clear();
        converted_data.fill_uninitialized(static_cast<uint32_t>(total_size));
        uint8_t* target = &converted_data[0];
        memset(target, 0, static_cast<size_t>(total_size));
        auto& standard = *reinterpret_cast<_log_entry_head_def*>(target);
        standard.timestamp_epoch = log_head_base_def::get_epoch(head.timestamp_epoch);
        standard.ext_info_offset = 0;
        standard.category_idx = format->category_idx;
        standard.log_thread_id = thread->thread_id_;
        standard.format_hash = format->format_hash;
        standard.log_format_str_type = format->format_type;
        standard.level = format->level;
        standard.log_format_data_len = format->format_size;
        memcpy(target + sizeof(standard), format->format_data(), format->format_size);
        cursor = 0;
        uint32_t target_cursor = static_cast<uint32_t>(args_offset);
        for (uint16_t i = 0; i < format->arg_count; ++i) {
            fast_arg_size field;
            const uint8_t type = format->arg_types()[i];
            get_fast_arg_size(static_cast<log_arg_type_enum>(type),
                args + cursor, args_size - cursor, field);
            target[target_cursor] = type;
            if (field.value_size) {
                memcpy(target + target_cursor + field.value_offset,
                    args + cursor, field.value_size);
            }
            cursor += field.source_size;
            target_cursor += field.target_size;
        }
        output = log_entry_handle(target, static_cast<uint32_t>(total_size));
        output.set_external_ext_head(reinterpret_cast<const _log_entry_ext_head_def*>(&thread->thread_name_len_));
        return true;
    }

    bool log_record_reader::make_recovery_error(log_buffer& buffer, const uint8_t* data,
        uint32_t size, bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>>& converted_data,
        log_entry_handle& output)
    {
        if (!buffer.is_current_reading_recovered()) {
            return false;
        }
        char message[192];
        uint64_t format_addr = 0;
        if (size >= sizeof(log_head_fast_def)) {
            format_addr = reinterpret_cast<const log_head_fast_def*>(data)->format_meta_addr;
        }
        const int32_t written = snprintf(message, sizeof(message),
            "[bqlog] fast log recovery failed: version=%" PRIu16 " format_addr=0x%" PRIx64,
            buffer.get_current_reading_version(), format_addr);
        if (written <= 0 || written >= static_cast<int32_t>(sizeof(message))) {
            return false;
        }
        static const char thread_name[] = "bqlog";
        const uint32_t format_size = static_cast<uint32_t>(written);
        const uint32_t ext_offset = static_cast<uint32_t>(sizeof(_log_entry_head_def)
            + bq::align_4(format_size));
        const uint32_t total_size = ext_offset + static_cast<uint32_t>(sizeof(_log_entry_ext_head_def) + sizeof(thread_name) - 1);
        converted_data.clear();
        converted_data.fill_uninitialized(total_size);
        uint8_t* target = &converted_data[0];
        memset(target, 0, total_size);
        auto& head = *reinterpret_cast<_log_entry_head_def*>(target);
        head.timestamp_epoch = get_timestamp(data, size);
        head.ext_info_offset = ext_offset;
        head.log_format_str_type = static_cast<uint8_t>(log_arg_type_enum::string_utf8_type);
        head.level = static_cast<uint8_t>(log_level::warning);
        head.log_format_data_len = format_size;
        head.format_hash = bq::util::get_hash_64(message, format_size);
        memcpy(target + sizeof(head), message, format_size);
        target[ext_offset] = static_cast<uint8_t>(sizeof(thread_name) - 1);
        memcpy(target + ext_offset + sizeof(_log_entry_ext_head_def),
            thread_name, sizeof(thread_name) - 1);
        output = log_entry_handle(target, total_size);
        return true;
    }
}
