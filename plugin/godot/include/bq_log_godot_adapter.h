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
        inline size_t write_formatted(char* dest, size_t max_chars, const char* src, int32_t len)
        {
            if (len <= 0) {
                return 0u;
            }
            const size_t n = static_cast<size_t>(len);
            if (dest && max_chars > 0u) {
                const size_t copy_len = (n < max_chars) ? n : max_chars;
                memcpy(dest, src, copy_len);
            }
            return n;
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

    // Math and geometry types: stack-only UTF-8 formatting with zero heap allocation.
    inline size_t bq_log_format_str_size(const Vector2& v)
    {
        char buf[64];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g)", static_cast<double>(v.x), static_cast<double>(v.y));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector2& v, char* dest, size_t max_chars)
    {
        char buf[64];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g)", static_cast<double>(v.x), static_cast<double>(v.y));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Vector2i& v)
    {
        char buf[64];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector2i& v, char* dest, size_t max_chars)
    {
        char buf[64];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Vector3& v)
    {
        char buf[96];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector3& v, char* dest, size_t max_chars)
    {
        char buf[96];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Vector3i& v)
    {
        char buf[96];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector3i& v, char* dest, size_t max_chars)
    {
        char buf[96];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Vector4& v)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z), static_cast<double>(v.w));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector4& v, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z), static_cast<double>(v.w));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Vector4i& v)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z), static_cast<int32_t>(v.w));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Vector4i& v, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%d, %d, %d, %d)", static_cast<int32_t>(v.x), static_cast<int32_t>(v.y), static_cast<int32_t>(v.z), static_cast<int32_t>(v.w));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Color& c)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b), static_cast<double>(c.a));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Color& c, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b), static_cast<double>(c.a));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Quaternion& q)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z), static_cast<double>(q.w));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Quaternion& q, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "(%g, %g, %g, %g)", static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z), static_cast<double>(q.w));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Rect2& r)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "[P: (%g, %g), S: (%g, %g)]",
            static_cast<double>(r.position.x), static_cast<double>(r.position.y),
            static_cast<double>(r.size.x), static_cast<double>(r.size.y));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Rect2& r, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "[P: (%g, %g), S: (%g, %g)]",
            static_cast<double>(r.position.x), static_cast<double>(r.position.y),
            static_cast<double>(r.size.x), static_cast<double>(r.size.y));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

    inline size_t bq_log_format_str_size(const Rect2i& r)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "[P: (%d, %d), S: (%d, %d)]",
            static_cast<int32_t>(r.position.x), static_cast<int32_t>(r.position.y),
            static_cast<int32_t>(r.size.x), static_cast<int32_t>(r.size.y));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

    inline void bq_log_custom_format(const Rect2i& r, char* dest, size_t max_chars)
    {
        char buf[128];
        const int32_t n = snprintf(buf, sizeof(buf), "[P: (%d, %d), S: (%d, %d)]",
            static_cast<int32_t>(r.position.x), static_cast<int32_t>(r.position.y),
            static_cast<int32_t>(r.size.x), static_cast<int32_t>(r.size.y));
        bq_detail::write_formatted(dest, max_chars, buf, n);
    }

}
