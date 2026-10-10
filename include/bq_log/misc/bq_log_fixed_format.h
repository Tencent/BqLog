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
// C++20 only: the non-macro fast log interface's fixed-format type and compile-time helpers.
// bq_log_entry.h includes this while <log> is still being defined, so everything here must
// depend on bq_common and the type declarations of bq_log_def.h only.
#if defined(BQ_CPP_20)
#include "bq_common/bq_common_public_include.h"
#include "bq_log/misc/bq_log_def.h"

namespace bq {
    // never defined: reaching it during constant evaluation is a compile error
    void bq_fast_format_requires_utf8_execution_charset();

    namespace fast_info_detail {
        constexpr bool exec_charset_is_utf8()
        {
            // U+4E2D as a universal character name: the compiler converts it to the execution
            // character set, so this encodes to 0xE4 0xB8 0xAD only under a UTF-8 charset.
            // (A "\xE4\xB8\xAD" hex escape would be charset independent and always pass.)
            return static_cast<unsigned char>("\u4e2d"[0]) == 0xE4 && sizeof("\u4e2d") == 4;
        }

        template <typename...>
        struct always_false {
            static constexpr bool value = false;
        };

        // Whether T is a generated category type; the probe itself lives in bq_log_def.h as
        // bq::log_category_index, shared with the macro path.
        template <typename T>
        struct is_category {
            static constexpr bool value = bq::log_category_index<T>::value != 0;
        };
    }

    // A string literal as a non-type template parameter. Copying into chars makes the object
    // usable as a constant expression, and the copy is re-validated (encoding, charset) at
    // compile time.
    template <size_t N>
    struct fixed_format {
        char chars[N];

        template <typename C>
        constexpr fixed_format(const C (&s)[N])
            : chars {}
        {
            static_assert(bq::is_same<C, char>::value || bq::is_same<C, char8_t>::value,
                "fast_*: only narrow or u8 string literals are supported, use BQ_LOG_FAST_* for UTF-16/32 formats");
            bool non_ascii = false;
            for (size_t i = 0; i < N; ++i) {
                chars[i] = static_cast<char>(s[i]);
                non_ascii = non_ascii || static_cast<unsigned char>(s[i]) >= 0x80;
            }
            if (bq::is_same<C, char>::value && non_ascii && !fast_info_detail::exec_charset_is_utf8()) {
                bq_fast_format_requires_utf8_execution_charset(); // MSVC: compile with /utf-8
            }
        }
    };

    namespace fast_info_detail {
        // friend of bq::log, so fast_impl may read its private level words, buffer id and log id
        struct log_access;

        // declared here so the member templates of bq::log may call it; defined in bq_log_fast_info.h,
        // where bq::log is complete
        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_forceinline bool fast_impl(const bq::log& l, const Args&... args);
    }
}

#endif
