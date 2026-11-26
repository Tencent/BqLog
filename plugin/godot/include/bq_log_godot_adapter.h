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
#include <stddef.h>
#include <stdint.h>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include "bq_log/bq_log.h"

namespace godot {
    namespace bq_adapter_detail {
        inline size_t calc_utf32_to_utf8_bytes(const char32_t* src, int64_t len)
        {
            if (!src || len <= 0) {
                return 0;
            }
            size_t total = 0;
            for (int64_t i = 0; i < len; ++i) {
                const uint32_t cp = static_cast<uint32_t>(src[i]);
                if (cp <= 0x7Fu) {
                    total += 1;
                } else if (cp <= 0x7FFu) {
                    total += 2;
                } else if (cp <= 0xFFFFu) {
                    total += 3;
                } else if (cp <= 0x10FFFFu) {
                    total += 4;
                } else {
                    total += 3;
                }
            }
            return total;
        }

        inline void encode_utf32_to_utf8(const char32_t* src, int64_t len, char* dst, size_t max_bytes)
        {
            if (!src || len <= 0 || !dst || max_bytes == 0) {
                return;
            }
            size_t pos = 0;
            for (int64_t i = 0; i < len; ++i) {
                uint32_t cp = static_cast<uint32_t>(src[i]);
                if (cp <= 0x7Fu) {
                    if (pos + 1 > max_bytes) {
                        break;
                    }
                    dst[pos++] = static_cast<char>(cp);
                } else if (cp <= 0x7FFu) {
                    if (pos + 2 > max_bytes) {
                        break;
                    }
                    dst[pos++] = static_cast<char>(0xC0u | (cp >> 6u));
                    dst[pos++] = static_cast<char>(0x80u | (cp & 0x3Fu));
                } else if (cp <= 0xFFFFu) {
                    if (pos + 3 > max_bytes) {
                        break;
                    }
                    if (cp >= 0xD800u && cp <= 0xDFFFu) {
                        cp = 0xFFFDu;
                    }
                    dst[pos++] = static_cast<char>(0xE0u | (cp >> 12u));
                    dst[pos++] = static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu));
                    dst[pos++] = static_cast<char>(0x80u | (cp & 0x3Fu));
                } else if (cp <= 0x10FFFFu) {
                    if (pos + 4 > max_bytes) {
                        break;
                    }
                    dst[pos++] = static_cast<char>(0xF0u | (cp >> 18u));
                    dst[pos++] = static_cast<char>(0x80u | ((cp >> 12u) & 0x3Fu));
                    dst[pos++] = static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu));
                    dst[pos++] = static_cast<char>(0x80u | (cp & 0x3Fu));
                } else {
                    if (pos + 3 > max_bytes) {
                        break;
                    }
                    dst[pos++] = static_cast<char>(0xEFu);
                    dst[pos++] = static_cast<char>(0xBFu);
                    dst[pos++] = static_cast<char>(0xBDu);
                }
            }
        }
    }

    inline size_t bq_log_format_str_size(const String& str)
    {
        return bq_adapter_detail::calc_utf32_to_utf8_bytes(str.ptr(), str.length());
    }

    inline void bq_log_custom_format(const String& str, char* dest, size_t max_bytes)
    {
        bq_adapter_detail::encode_utf32_to_utf8(str.ptr(), str.length(), dest, max_bytes);
    }

    inline size_t bq_log_format_str_size(const StringName& str_name)
    {
        const String s(str_name);
        return bq_adapter_detail::calc_utf32_to_utf8_bytes(s.ptr(), s.length());
    }

    inline void bq_log_custom_format(const StringName& str_name, char* dest, size_t max_bytes)
    {
        const String s(str_name);
        bq_adapter_detail::encode_utf32_to_utf8(s.ptr(), s.length(), dest, max_bytes);
    }
}
