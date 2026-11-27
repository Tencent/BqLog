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
        constexpr size_t format_raw_capacity = 32u; // Covers both float and double (REAL_T_IS_DOUBLE) builds

        enum math_type_tag : uint8_t {
            tag_none = 0u,
            tag_vec2 = 1u,
            tag_vec2i = 2u,
            tag_vec3 = 3u,
            tag_vec3i = 4u,
            tag_vec4 = 5u,
            tag_vec4i = 6u,
            tag_color = 7u,
            tag_quat = 8u,
            tag_rect2 = 9u,
            tag_rect2i = 10u
        };

        template <typename T>
        struct math_type_traits {
            static constexpr uint8_t tag = tag_none;
        };
        template <>
        struct math_type_traits<Vector2> {
            static constexpr uint8_t tag = tag_vec2;
        };
        template <>
        struct math_type_traits<Vector2i> {
            static constexpr uint8_t tag = tag_vec2i;
        };
        template <>
        struct math_type_traits<Vector3> {
            static constexpr uint8_t tag = tag_vec3;
        };
        template <>
        struct math_type_traits<Vector3i> {
            static constexpr uint8_t tag = tag_vec3i;
        };
        template <>
        struct math_type_traits<Vector4> {
            static constexpr uint8_t tag = tag_vec4;
        };
        template <>
        struct math_type_traits<Vector4i> {
            static constexpr uint8_t tag = tag_vec4i;
        };
        template <>
        struct math_type_traits<Color> {
            static constexpr uint8_t tag = tag_color;
        };
        template <>
        struct math_type_traits<Quaternion> {
            static constexpr uint8_t tag = tag_quat;
        };
        template <>
        struct math_type_traits<Rect2> {
            static constexpr uint8_t tag = tag_rect2;
        };
        template <>
        struct math_type_traits<Rect2i> {
            static constexpr uint8_t tag = tag_rect2i;
        };

        // Pure trivial POD so BQ_TLS is zero-initialized in .tbss with zero runtime guard overhead.
        struct format_cache_slot {
            const void* addr_;
            size_t len_;
            uint8_t type_tag_;
            uint8_t raw_[format_raw_capacity];
            char buf_[format_buf_capacity];
        };

        struct format_cache {
            format_cache_slot slots_[format_cache_slots];
            uint32_t next_idx_;
        };

        template <typename DUMMY = void>
        struct format_cache_holder {
            static BQ_TLS format_cache cache_;
        };

        template <typename DUMMY>
        BQ_TLS format_cache format_cache_holder<DUMMY>::cache_ = {};

        inline size_t clamp_snprintf_len(int32_t n)
        {
            if (n <= 0) {
                return 0u;
            }
            const size_t len = static_cast<size_t>(n);
            return (len < format_buf_capacity) ? len : (format_buf_capacity - 1u);
        }

        inline size_t string_utf16_chars(const String& str)
        {
            const int64_t len = str.length();
            if (len <= 0 || !str.ptr()) {
                return 0u;
            }
            return bq::tools::_serialize_str_helper_by_encode<char32_t>::get_c_style_string_storage_size(
                       str.ptr(), static_cast<size_t>(len))
                >> 1u;
        }

        inline void string_utf16_copy(const String& str, char16_t* dest, size_t max_chars)
        {
            const int64_t len = str.length();
            if (len <= 0 || !str.ptr() || !dest || max_chars == 0u) {
                return;
            }
            bq::tools::_serialize_str_helper_by_encode<char32_t>::c_style_string_copy(
                dest, str.ptr(), max_chars * sizeof(char16_t));
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

        // Format once in pass 1 (size), recording address + type tag + raw POD bytes,
        // and verify + consume in pass 2 (copy) via zero-init BQ_TLS ring cache.
        template <uint8_t TAG, typename T>
        inline size_t cached_format_size(const T& val)
        {
            static_assert(sizeof(T) <= format_raw_capacity, "Godot math type exceeds format_raw_capacity");
            format_cache& cache = format_cache_holder<>::cache_;
            format_cache_slot& slot = cache.slots_[cache.next_idx_ & (format_cache_slots - 1u)];
            ++cache.next_idx_;
            const size_t len = clamp_snprintf_len(format_value(val, slot.buf_, format_buf_capacity));
            slot.addr_ = static_cast<const void*>(&val);
            slot.len_ = len;
            slot.type_tag_ = TAG;
            memcpy(slot.raw_, &val, sizeof(T));
            return len;
        }

        template <uint8_t TAG, typename T>
        inline void cached_format_copy(const T& val, char* dest, size_t max_chars)
        {
            static_assert(sizeof(T) <= format_raw_capacity, "Godot math type exceeds format_raw_capacity");
            if (!dest || max_chars == 0u) {
                return;
            }
            const void* addr = static_cast<const void*>(&val);
            format_cache& cache = format_cache_holder<>::cache_;
            for (uint32_t i = 1u; i <= format_cache_slots; ++i) {
                format_cache_slot& slot = cache.slots_[(cache.next_idx_ - i) & (format_cache_slots - 1u)];
                if (slot.type_tag_ == TAG
                    && slot.addr_ == addr
                    && slot.len_ == max_chars
                    && memcmp(slot.raw_, &val, sizeof(T)) == 0) {
                    slot.type_tag_ = tag_none;
                    memcpy(dest, slot.buf_, max_chars);
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

    // Exact-type SFINAE prevents implicit conversions (e.g. godot::Color(const String&))
    // from matching both char* and char16_t* partial specializations in _custom_type_helper.
    template <typename T>
    inline bq::enable_if_t<bq::is_same<bq::decay_t<T>, String>::value, size_t>
    bq_log_format_str_size(const T& str)
    {
        return bq_detail::string_utf16_chars(str);
    }

    template <typename T>
    inline bq::enable_if_t<bq::is_same<bq::decay_t<T>, String>::value, void>
    bq_log_custom_format(const T& str, char16_t* dest, size_t max_chars)
    {
        bq_detail::string_utf16_copy(str, dest, max_chars);
    }

    template <typename T>
    inline bq::enable_if_t<bq::is_same<bq::decay_t<T>, StringName>::value || bq::is_same<bq::decay_t<T>, NodePath>::value, size_t>
    bq_log_format_str_size(const T& val)
    {
        return bq_detail::string_utf16_chars(String(val));
    }

    template <typename T>
    inline bq::enable_if_t<bq::is_same<bq::decay_t<T>, StringName>::value || bq::is_same<bq::decay_t<T>, NodePath>::value, void>
    bq_log_custom_format(const T& val, char16_t* dest, size_t max_chars)
    {
        bq_detail::string_utf16_copy(String(val), dest, max_chars);
    }

    template <typename T>
    inline bq::enable_if_t<bq_detail::math_type_traits<bq::decay_t<T>>::tag != bq_detail::tag_none, size_t>
    bq_log_format_str_size(const T& val)
    {
        return bq_detail::cached_format_size<bq_detail::math_type_traits<bq::decay_t<T>>::tag>(val);
    }

    template <typename T>
    inline bq::enable_if_t<bq_detail::math_type_traits<bq::decay_t<T>>::tag != bq_detail::tag_none, void>
    bq_log_custom_format(const T& val, char* dest, size_t max_chars)
    {
        bq_detail::cached_format_copy<bq_detail::math_type_traits<bq::decay_t<T>>::tag>(val, dest, max_chars);
    }

}
