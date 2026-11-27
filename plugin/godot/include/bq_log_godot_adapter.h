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

// Single-header BqLog C++ adapter for Godot 4 (godot-cpp).
// Include this header instead of "bq_log/bq_log.h" directly.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/vector4i.hpp>
#include "bq_log/bq_log.h"

// Functions below must live in namespace godot so C++ two-phase ADL resolves them
// from bq::tools::_custom_type_helper regardless of header include order.
namespace godot {

    namespace bq_detail {
        constexpr uint32_t format_cache_slots = 8u;
        constexpr size_t format_buf_capacity = 128u;

        struct format_cache_slot {
            const void* addr_ = nullptr;
            size_t len_ = 0u;
            char buf_[format_buf_capacity];
        };

        struct format_cache {
            format_cache_slot slots_[format_cache_slots];
            uint32_t next_idx_ = 0u;
        };

        inline format_cache& get_format_cache()
        {
            static thread_local format_cache cache;
            return cache;
        }

        inline size_t clamp_snprintf_len(int32_t n)
        {
            if (n <= 0) {
                return 0u;
            }
            const size_t len = static_cast<size_t>(n);
            return (len < format_buf_capacity) ? len : (format_buf_capacity - 1u);
        }

        inline int32_t format_value(const Vector2& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%g, %g)", static_cast<double>(v.x), static_cast<double>(v.y));
        }

        inline int32_t format_value(const Vector2i& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y));
        }

        inline int32_t format_value(const Vector3& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
        }

        inline int32_t format_value(const Vector3i& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z));
        }

        inline int32_t format_value(const Vector4& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%g, %g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z), static_cast<double>(v.w));
        }

        inline int32_t format_value(const Vector4i& v, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%d, %d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z), static_cast<int32_t>(v.w));
        }

        inline int32_t format_value(const Color& c, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%g, %g, %g, %g)", static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b), static_cast<double>(c.a));
        }

        inline int32_t format_value(const Quaternion& q, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "(%g, %g, %g, %g)", static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z), static_cast<double>(q.w));
        }

        inline int32_t format_value(const Rect2& r, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "[P: (%g, %g), S: (%g, %g)]",
                static_cast<double>(r.position.x), static_cast<double>(r.position.y),
                static_cast<double>(r.size.x), static_cast<double>(r.size.y));
        }

        inline int32_t format_value(const Rect2i& r, char* buf, size_t cap)
        {
            return snprintf(buf, cap, "[P: (%d, %d), S: (%d, %d)]",
                static_cast<int32_t>(r.position.x), static_cast<int32_t>(r.position.y),
                static_cast<int32_t>(r.size.x), static_cast<int32_t>(r.size.y));
        }

        // Format once in pass 1 (size) and reuse via thread_local ring cache in pass 2 (copy).
        template <typename T>
        inline size_t cached_format_size(const T& val)
        {
            format_cache& cache = get_format_cache();
            format_cache_slot& slot = cache.slots_[cache.next_idx_ & (format_cache_slots - 1u)];
            ++cache.next_idx_;
            const size_t len = clamp_snprintf_len(format_value(val, slot.buf_, format_buf_capacity));
            slot.addr_ = static_cast<const void*>(&val);
            slot.len_ = len;
            return len;
        }

        template <typename T>
        inline void cached_format_copy(const T& val, char* dest, size_t max_chars)
        {
            if (!dest || max_chars == 0u) {
                return;
            }
            const void* addr = static_cast<const void*>(&val);
            format_cache& cache = get_format_cache();
            for (uint32_t i = 1u; i <= format_cache_slots; ++i) {
                const format_cache_slot& slot = cache.slots_[(cache.next_idx_ - i) & (format_cache_slots - 1u)];
                if (slot.addr_ == addr) {
                    const size_t copy_len = (slot.len_ < max_chars) ? slot.len_ : max_chars;
                    if (copy_len > 0u) {
                        memcpy(dest, slot.buf_, copy_len);
                    }
                    return;
                }
            }
            char fallback_buf[format_buf_capacity];
            const size_t len = clamp_snprintf_len(format_value(val, fallback_buf, format_buf_capacity));
            const size_t copy_len = (len < max_chars) ? len : max_chars;
            if (copy_len > 0u) {
                memcpy(dest, fallback_buf, copy_len);
            }
        }
    }

    // String types: reuse BqLog's 4-wide unrolled UTF-32 -> UTF-16 fast path.
    inline size_t bq_log_format_str_size(const String& str)
    {
        const int64_t len = str.length();
        if (len <= 0 || !str.ptr()) {
            return 0u;
        }
        return bq::tools::_serialize_str_helper_by_encode<char32_t>::get_c_style_string_storage_size(
                   str.ptr(), static_cast<size_t>(len))
            >> 1u;
    }

    inline void bq_log_custom_format(const String& str, char16_t* dest, size_t max_chars)
    {
        const int64_t len = str.length();
        if (len <= 0 || !str.ptr() || !dest || max_chars == 0u) {
            return;
        }
        bq::tools::_serialize_str_helper_by_encode<char32_t>::c_style_string_copy(
            dest, str.ptr(), max_chars * sizeof(char16_t));
    }

    inline size_t bq_log_format_str_size(const StringName& str_name)
    {
        return bq_log_format_str_size(String(str_name));
    }

    inline void bq_log_custom_format(const StringName& str_name, char16_t* dest, size_t max_chars)
    {
        bq_log_custom_format(String(str_name), dest, max_chars);
    }

    inline size_t bq_log_format_str_size(const NodePath& path)
    {
        return bq_log_format_str_size(String(path));
    }

    inline void bq_log_custom_format(const NodePath& path, char16_t* dest, size_t max_chars)
    {
        bq_log_custom_format(String(path), dest, max_chars);
    }

    // Math and geometry types: single-pass snprintf cached across size + copy phases.
    inline size_t bq_log_format_str_size(const Vector2& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector2& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Vector2i& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector2i& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Vector3& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector3& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Vector3i& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector3i& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Vector4& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector4& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Vector4i& v) { return bq_detail::cached_format_size(v); }
    inline void bq_log_custom_format(const Vector4i& v, char* dest, size_t max_chars) { bq_detail::cached_format_copy(v, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Color& c) { return bq_detail::cached_format_size(c); }
    inline void bq_log_custom_format(const Color& c, char* dest, size_t max_chars) { bq_detail::cached_format_copy(c, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Quaternion& q) { return bq_detail::cached_format_size(q); }
    inline void bq_log_custom_format(const Quaternion& q, char* dest, size_t max_chars) { bq_detail::cached_format_copy(q, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Rect2& r) { return bq_detail::cached_format_size(r); }
    inline void bq_log_custom_format(const Rect2& r, char* dest, size_t max_chars) { bq_detail::cached_format_copy(r, dest, max_chars); }

    inline size_t bq_log_format_str_size(const Rect2i& r) { return bq_detail::cached_format_size(r); }
    inline void bq_log_custom_format(const Rect2i& r, char* dest, size_t max_chars) { bq_detail::cached_format_copy(r, dest, max_chars); }

}
