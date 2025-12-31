#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
/*!
 * \file string_tools.h
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */

#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include "bq_common/types/type_tools.h"
#include "bq_common/types/type_traits.h"

namespace bq {
    namespace string_tools {

        /**
         * @brief Fallback loop for compile-time constant evaluation.
         * Compilers like GCC and Clang can unroll and constant-fold this
         * when the input is a string literal.
         */
        template <typename T>
        static BQ_FUNC_RETURN_CONSTEXPR size_t constexpr_len(const T* str) {
            size_t i = 0;
            while (str[i] != 0) {
                i++;
            }
            return i;
        }

        /**
         * @brief SWAR (SIMD Within A Register) implementation for UTF-16.
         * Processes 4 characters (8 bytes) per iteration using bit manipulation
         * to detect the null terminator.
         */
        static bq_forceinline size_t u16_len_rt(const char16_t* s) {
            const char16_t* p = s;
            // Align to 8-byte boundary
            while ((reinterpret_cast<uintptr_t>(p) & 7) != 0) {
                if (*p == 0) return static_cast<size_t>(p - s);
                p++;
            }

            const uint64_t* chunk = reinterpret_cast<const uint64_t*>(p);
            const uint64_t mask = 0x8000800080008000ULL;
            const uint64_t low_bit = 0x0001000100010001ULL;

            while (true) {
                uint64_t v = *chunk;
                // Zero detection logic: (v - 0x0001) & ~v & 0x8000
                if ((v - low_bit) & ~v & mask) {
                    p = reinterpret_cast<const char16_t*>(chunk);
                    if (p[0] == 0) return static_cast<size_t>(p - s);
                    if (p[1] == 0) return static_cast<size_t>(p + 1 - s);
                    if (p[2] == 0) return static_cast<size_t>(p + 2 - s);
                    if (p[3] == 0) return static_cast<size_t>(p + 3 - s);
                }
                chunk++;
            }
        }

        /**
         * @brief SWAR implementation for UTF-32.
         * Processes 2 characters (8 bytes) per iteration.
         */
        static bq_forceinline size_t u32_len_rt(const char32_t* s) {
            const char32_t* p = s;
            while ((reinterpret_cast<uintptr_t>(p) & 7) != 0) {
                if (*p == 0) return static_cast<size_t>(p - s);
                p++;
            }

            const uint64_t* chunk = reinterpret_cast<const uint64_t*>(p);
            const uint64_t mask = 0x8000000080000000ULL;
            const uint64_t low_bit = 0x0000000100000001ULL;

            while (true) {
                uint64_t v = *chunk;
                if ((v - low_bit) & ~v & mask) {
                    p = reinterpret_cast<const char32_t*>(chunk);
                    if (p[0] == 0) return static_cast<size_t>(p - s);
                    if (p[1] == 0) return static_cast<size_t>(p + 1 - s);
                }
                chunk++;
            }
        }

        /* --- Dispatcher Templates --- */

        template <typename T, bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch {
            static BQ_FUNC_RETURN_CONSTEXPR size_t exec(const T* s) {
                return constexpr_len(s);
            }
        };

        template <bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch<char, WCHAR_SIZE_IS_16> {
            static bq_forceinline size_t exec(const char* s) {
#if BQ_GCC_CLANG_BUILTIN(__builtin_wcslen)
                return __builtin_strlen(s);
#else
                return strlen(s);
#endif
            }
        };

        template <bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch<wchar_t, WCHAR_SIZE_IS_16> {
            static bq_forceinline size_t exec(const wchar_t* s) {
#if BQ_GCC_CLANG_BUILTIN(__builtin_wcslen)
                return __builtin_wcslen(s);
#else
                return wcslen(s);
#endif
            }
        };

        template <>
        struct string_len_dispatch<char16_t, true> {
            static bq_forceinline size_t exec(const char16_t* s) {
                return string_len_dispatch<wchar_t, true>::exec(reinterpret_cast<const wchar_t*>(s));
            }
        };
        template <>
        struct string_len_dispatch<char16_t, false> {
            static bq_forceinline size_t exec(const char16_t* s) {
                return u16_len_rt(s);
            }
        };

        template <>
        struct string_len_dispatch<char32_t, true> {
            static bq_forceinline size_t exec(const char32_t* s) {
                return u32_len_rt(s);
            }
        };
        template <>
        struct string_len_dispatch<char32_t, false> {
            static bq_forceinline size_t exec(const char32_t* s) {
                return string_len_dispatch<wchar_t, false>::exec(reinterpret_cast<const wchar_t*>(s));
            }
        };

        /**
         * @brief Unified entry point for string length calculation.
         * * Performance characteristics:
         * 1. Compile-time: Literal strings result in a zero-cost constant.
         * 2. Runtime (char/wchar_t): Dispatches to compiler built-ins (SIMD optimized).
         * 3. Runtime (u16/u32): Uses 64-bit SWAR to process multiple units per cycle.
         */
        template <typename CHAR_TYPE>
        BQ_FUNC_RETURN_CONSTEXPR size_t string_len_ptr(const CHAR_TYPE* str) {
            if (!str) {
                return 0;
            }
#if BQ_GCC_CLANG_BUILTIN(__builtin_constant_p)
            BQ_CONSTEXPR_IF(__builtin_constant_p(*str))
            {
                return constexpr_len(str);
            }
            else {
#endif
                return string_len_dispatch<CHAR_TYPE, sizeof(char16_t) == sizeof(wchar_t)>::exec(str);
#if BQ_GCC_CLANG_BUILTIN(__builtin_constant_p)
            }
#endif
        }
        template <typename CHAR_TYPE, size_t N>
        BQ_FUNC_RETURN_CONSTEXPR size_t string_len_array(const CHAR_TYPE(&str)[N]) {
            BQ_LIKELY_IF(str[N - 1] == '\0') {
                return N - 1; //constexpr_len(str);
            }
            else {
                return N;
                }
        }
        template <typename STR_TYPE>
        bq_forceinline bq::enable_if_t<bq::is_pointer<STR_TYPE>::value, size_t> string_len(const STR_TYPE& str) {
            return string_len_ptr(str);
        }

        template <typename STR_TYPE>
        bq_forceinline bq::enable_if_t<bq::is_array<STR_TYPE>::value, size_t> string_len(const STR_TYPE& str) {
            return string_len_array(str);
        }


        template <typename CHAR_TYPE>
        inline const CHAR_TYPE* find_char(const CHAR_TYPE* str, CHAR_TYPE c)
        {
            if (!str) {
                return nullptr;
            }
            for (; *str; ++str) {
                if (*str == c) {
                    return str;
                }
            }
            return nullptr;
        }

        template <typename CHAR_TYPE>
        inline const CHAR_TYPE* find_str(const CHAR_TYPE* str1, const CHAR_TYPE* str2)
        {
            if (!str2) {
                return nullptr;
            }
            auto len2 = string_len(str2);
            if (0 == len2) {
            }
            auto len1 = string_len(str1);
            const CHAR_TYPE* begin_pos = find_char(str1, *str2);
            while (begin_pos && begin_pos + len2 <= str1 + len1) {
                if (memcmp(begin_pos, str2, len2 * sizeof(CHAR_TYPE)) == 0) {
                    return begin_pos;
                }
                begin_pos = find_char(begin_pos + 1, *str2);
            }
            return nullptr;
        }

        template <>
        inline const char* find_str(const char* str1, const char* str2)
        {
            if (str1 == nullptr || str2 == nullptr) {
                return nullptr;
            }
            return strstr(str1, str2);
        }


        template <typename CHAR_TYPE>
        struct inner_empty_str_def {};
        template <>
        struct inner_empty_str_def<char> { static const char* value() { return ""; } };
        template <>
        struct inner_empty_str_def<wchar_t> { static const wchar_t* value() { return L""; } };
        template <>
        struct inner_empty_str_def<char16_t> { static const char16_t* value() { return u""; } };
        template <>
        struct inner_empty_str_def<char32_t> { static const char32_t* value() { return U""; } };
    }
 }