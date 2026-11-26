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

// Zero-heap-allocation C++ adapter allowing godot::String and godot::StringName to be passed
// directly as BqLog format strings and arguments (including BQ_LOG_FAST_*).
// Reuses BqLog's 4-wide unrolled UTF-32 -> UTF-16 fast path in bq_log_wrapper_tools.h.
#include <stddef.h>
#include <stdint.h>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include "bq_log/bq_log.h"

namespace godot {
    inline size_t bq_log_format_str_size(const String& str)
    {
        const int64_t len = str.length();
        if (len <= 0 || !str.ptr()) {
            return 0;
        }
        return bq::tools::_serialize_str_helper_by_encode<char32_t>::get_c_style_string_storage_size(
                   str.ptr(), static_cast<size_t>(len))
            >> 1u;
    }

    inline void bq_log_custom_format(const String& str, char16_t* dest, size_t max_chars)
    {
        const int64_t len = str.length();
        if (len <= 0 || !str.ptr() || !dest || max_chars == 0) {
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
}
