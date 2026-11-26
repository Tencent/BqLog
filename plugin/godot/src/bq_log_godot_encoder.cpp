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
#include "bq_log_godot_encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/vector4i.hpp>

namespace bq_godot {

    static constexpr uint32_t stack_arg_capacity = 32u;
    static constexpr uint32_t inline_utf8_capacity = 128u;

    static constexpr uint8_t type_null = static_cast<uint8_t>(bq::log_arg_type_enum::null_type);
    static constexpr uint8_t type_bool = static_cast<uint8_t>(bq::log_arg_type_enum::bool_type);
    static constexpr uint8_t type_int64 = static_cast<uint8_t>(bq::log_arg_type_enum::int64_type);
    static constexpr uint8_t type_double = static_cast<uint8_t>(bq::log_arg_type_enum::double_type);
    static constexpr uint8_t type_str8 = static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type);
    static constexpr uint8_t type_str16 = static_cast<uint8_t>(bq::log_arg_type_enum::string_utf16_type);

    static inline uint32_t align4(uint32_t v)
    {
        return (v + 3u) & ~3u;
    }

    static inline uint32_t calc_utf32_to_utf16_bytes(const char32_t* src, int64_t len)
    {
        if (!src || len <= 0) {
            return 0u;
        }
        return static_cast<uint32_t>(
            bq::tools::_serialize_str_helper_by_encode<char32_t>::get_c_style_string_storage_size(
                src, static_cast<size_t>(len)));
    }

    static inline void copy_utf32_to_utf16(uint8_t* dst, const char32_t* src, uint32_t utf16_bytes)
    {
        if (utf16_bytes > 0u && src) {
            bq::tools::_serialize_str_helper_by_encode<char32_t>::c_style_string_copy(
                dst, src, static_cast<size_t>(utf16_bytes));
        }
    }

    struct godot_arg_slot {
        enum class arg_kind : uint8_t {
            null_val,
            bool_val,
            int64_val,
            double_val,
            str16_from_godot_val,
            inline_utf8_val
        };

        arg_kind kind_ = arg_kind::null_val;
        union {
            bool bool_val_;
            int64_t int64_val_;
            double double_val_;
        } pod_ = { false };
        godot::String str_holder_; // Holds CoW reference for STRING / STRING_NAME / fallback stringify
        uint32_t byte_len_ = 0u;
        char inline_utf8_[inline_utf8_capacity];
    };

    static void prepare_arg_slot(const godot::Variant& v, godot_arg_slot& slot)
    {
        switch (v.get_type()) {
        case godot::Variant::NIL:
            slot.kind_ = godot_arg_slot::arg_kind::null_val;
            break;
        case godot::Variant::BOOL:
            slot.kind_ = godot_arg_slot::arg_kind::bool_val;
            slot.pod_.bool_val_ = static_cast<bool>(v);
            break;
        case godot::Variant::INT:
            slot.kind_ = godot_arg_slot::arg_kind::int64_val;
            slot.pod_.int64_val_ = static_cast<int64_t>(v);
            break;
        case godot::Variant::FLOAT:
            slot.kind_ = godot_arg_slot::arg_kind::double_val;
            slot.pod_.double_val_ = static_cast<double>(v);
            break;
        case godot::Variant::STRING: {
            slot.kind_ = godot_arg_slot::arg_kind::str16_from_godot_val;
            slot.str_holder_ = v;
            slot.byte_len_ = calc_utf32_to_utf16_bytes(slot.str_holder_.ptr(), slot.str_holder_.length());
            break;
        }
        case godot::Variant::STRING_NAME: {
            slot.kind_ = godot_arg_slot::arg_kind::str16_from_godot_val;
            godot::StringName sn = v;
            slot.str_holder_ = godot::String(sn);
            slot.byte_len_ = calc_utf32_to_utf16_bytes(slot.str_holder_.ptr(), slot.str_holder_.length());
            break;
        }
        case godot::Variant::VECTOR2: {
            const godot::Vector2 vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%g, %g)", static_cast<double>(vec.x), static_cast<double>(vec.y));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::VECTOR2I: {
            const godot::Vector2i vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%d, %d)", static_cast<int32_t>(vec.x), static_cast<int32_t>(vec.y));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::VECTOR3: {
            const godot::Vector3 vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%g, %g, %g)", static_cast<double>(vec.x), static_cast<double>(vec.y), static_cast<double>(vec.z));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::VECTOR3I: {
            const godot::Vector3i vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%d, %d, %d)", static_cast<int32_t>(vec.x), static_cast<int32_t>(vec.y), static_cast<int32_t>(vec.z));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::VECTOR4: {
            const godot::Vector4 vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%g, %g, %g, %g)", static_cast<double>(vec.x), static_cast<double>(vec.y), static_cast<double>(vec.z), static_cast<double>(vec.w));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::VECTOR4I: {
            const godot::Vector4i vec = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%d, %d, %d, %d)", static_cast<int32_t>(vec.x), static_cast<int32_t>(vec.y), static_cast<int32_t>(vec.z), static_cast<int32_t>(vec.w));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::COLOR: {
            const godot::Color c = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%g, %g, %g, %g)", static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b), static_cast<double>(c.a));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        case godot::Variant::QUATERNION: {
            const godot::Quaternion q = v;
            const int32_t n = snprintf(slot.inline_utf8_, inline_utf8_capacity, "(%g, %g, %g, %g)", static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z), static_cast<double>(q.w));
            slot.kind_ = godot_arg_slot::arg_kind::inline_utf8_val;
            slot.byte_len_ = (n > 0) ? (static_cast<uint32_t>(n) < inline_utf8_capacity ? static_cast<uint32_t>(n) : inline_utf8_capacity - 1u) : 0u;
            break;
        }
        default: {
            slot.kind_ = godot_arg_slot::arg_kind::str16_from_godot_val;
            slot.str_holder_ = v.stringify();
            slot.byte_len_ = calc_utf32_to_utf16_bytes(slot.str_holder_.ptr(), slot.str_holder_.length());
            break;
        }
        }
    }

    static uint32_t calc_slot_storage_size(const godot_arg_slot& slot)
    {
        switch (slot.kind_) {
        case godot_arg_slot::arg_kind::null_val:
        case godot_arg_slot::arg_kind::bool_val:
            return 4u;
        case godot_arg_slot::arg_kind::int64_val:
        case godot_arg_slot::arg_kind::double_val:
            return 12u;
        case godot_arg_slot::arg_kind::str16_from_godot_val:
        case godot_arg_slot::arg_kind::inline_utf8_val:
            return align4(8u + slot.byte_len_);
        }
        return 0u;
    }

    static void write_slot_to_buffer(const godot_arg_slot& slot, uint8_t* dst)
    {
        switch (slot.kind_) {
        case godot_arg_slot::arg_kind::null_val:
            dst[0] = type_null;
            dst[1] = 0u;
            dst[2] = 0u;
            dst[3] = 0u;
            break;
        case godot_arg_slot::arg_kind::bool_val:
            dst[0] = type_bool;
            dst[1] = 0u;
            dst[2] = slot.pod_.bool_val_ ? 1u : 0u;
            dst[3] = 0u;
            break;
        case godot_arg_slot::arg_kind::int64_val:
            dst[0] = type_int64;
            dst[1] = 0u;
            dst[2] = 0u;
            dst[3] = 0u;
            memcpy(dst + 4, &slot.pod_.int64_val_, sizeof(int64_t));
            break;
        case godot_arg_slot::arg_kind::double_val:
            dst[0] = type_double;
            dst[1] = 0u;
            dst[2] = 0u;
            dst[3] = 0u;
            memcpy(dst + 4, &slot.pod_.double_val_, sizeof(double));
            break;
        case godot_arg_slot::arg_kind::str16_from_godot_val:
            dst[0] = type_str16;
            dst[1] = 0u;
            dst[2] = 0u;
            dst[3] = 0u;
            memcpy(dst + 4, &slot.byte_len_, sizeof(uint32_t));
            copy_utf32_to_utf16(dst + 8, slot.str_holder_.ptr(), slot.byte_len_);
            break;
        case godot_arg_slot::arg_kind::inline_utf8_val:
            dst[0] = type_str8;
            dst[1] = 0u;
            dst[2] = 0u;
            dst[3] = 0u;
            memcpy(dst + 4, &slot.byte_len_, sizeof(uint32_t));
            if (slot.byte_len_ > 0u) {
                memcpy(dst + 8, slot.inline_utf8_, slot.byte_len_);
            }
            break;
        }
    }

    bool write_godot_log_entry(
        uint64_t log_id,
        bq::log_level level,
        uint32_t category_index,
        const godot::String& format_str,
        const godot::Variant* const* args,
        int64_t arg_count)
    {
        const char32_t* fmt_ptr = format_str.ptr();
        const uint32_t fmt_utf16_bytes = calc_utf32_to_utf16_bytes(fmt_ptr, format_str.length());

        godot_arg_slot stack_slots[stack_arg_capacity];
        godot_arg_slot* slots = stack_slots;
        bool heap_slots = false;
        if (arg_count > static_cast<int64_t>(stack_arg_capacity)) {
            slots = new godot_arg_slot[static_cast<size_t>(arg_count)];
            heap_slots = true;
        }

        uint32_t total_args_bytes = 0u;
        for (int64_t i = 0; i < arg_count; ++i) {
            prepare_arg_slot(*args[i], slots[i]);
            total_args_bytes += calc_slot_storage_size(slots[i]);
        }

        // Pass nullptr for format_str_data so BqLog reserves the slot without extra copy,
        // then convert UTF-32 -> UTF-16 directly into handle.format_data_addr.
        bq::_api_log_write_handle handle = bq::api::__api_log_write_begin(
            log_id,
            static_cast<uint8_t>(level),
            category_index,
            type_str16,
            fmt_utf16_bytes,
            nullptr,
            total_args_bytes);

        if (handle.result != bq::enum_buffer_result_code::success) {
            if (heap_slots) {
                delete[] slots;
            }
            return false;
        }

        copy_utf32_to_utf16(handle.format_data_addr, fmt_ptr, fmt_utf16_bytes);

        uint8_t* arg_cursor = handle.format_data_addr + align4(fmt_utf16_bytes);
        for (int64_t i = 0; i < arg_count; ++i) {
            write_slot_to_buffer(slots[i], arg_cursor);
            arg_cursor += calc_slot_storage_size(slots[i]);
        }

        bq::api::__api_log_write_finish(log_id, handle);

        if (heap_slots) {
            delete[] slots;
        }
        return true;
    }

}
