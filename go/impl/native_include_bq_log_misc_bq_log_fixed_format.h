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
#include "native_include_bq_common_bq_common_public_include.h"
#include "native_include_bq_log_misc_bq_log_def.h"

namespace bq {
    BQ_MODULE_EXPORT_BEGIN
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

    // A template's static data member is exported through STB_GNU_UNIQUE otherwise, and the format
    // string is part of its symbol. Undone at the end of bq_log_fast_info.h.
#if defined(BQ_MSVC) || defined(BQ_VISUAL_STUDIO)
#define BQ_FAST_INFO_HIDDEN
#else
#define BQ_FAST_INFO_HIDDEN __attribute__((visibility("hidden")))
#endif

    // A string literal as a non-type template parameter: copied in, packed so the mangled names stay
    // short, and re-validated at compile time. The encoding follows the literal's character type:
    // char and char8_t are UTF-8, char16_t is UTF-16, char32_t is UTF-32, and wchar_t is whichever
    // of the two its width makes it.
    template <size_t N, typename C>
    struct fixed_format {
        using storage_type = bq::condition_type_t<sizeof(C) == 1, char, C>;
        static constexpr size_t length = N;
        static constexpr size_t units_per_word = sizeof(uint64_t) / sizeof(storage_type);
        static constexpr uint64_t unit_mask = ~static_cast<uint64_t>(0) >> (64 - 8 * sizeof(storage_type));

        uint64_t words[(N + units_per_word - 1) / units_per_word];

        constexpr fixed_format(const C (&s)[N])
            : words {}
        {
            static_assert(sizeof(C) == 1 || sizeof(C) == 2 || sizeof(C) == 4,
                "fast_*: the format string literal must be char, char8_t, char16_t, char32_t or wchar_t");
            bool non_ascii = false;
            for (size_t i = 0; i < N; ++i) {
                const uint64_t unit = static_cast<uint64_t>(static_cast<storage_type>(s[i])) & unit_mask;
                words[i / units_per_word] |= unit << (8 * sizeof(storage_type) * (i % units_per_word));
                non_ascii = non_ascii || unit >= 0x80;
            }
            // a u8/u/U/L literal fixes its own encoding; only a narrow one follows the charset
            if (bq::is_same<C, char>::value && non_ascii && !fast_info_detail::exec_charset_is_utf8()) {
                bq_fast_format_requires_utf8_execution_charset(); // MSVC: compile with /utf-8
            }
        }

        constexpr storage_type at(size_t i) const
        {
            return static_cast<storage_type>((words[i / units_per_word] >> (8 * sizeof(storage_type) * (i % units_per_word))) & unit_mask);
        }
    };

    // The packed literal as the array the slow path hands to do_log and to the registry.
    template <fixed_format F>
    struct BQ_FAST_INFO_HIDDEN fixed_format_chars {
        using char_type = typename decltype(F)::storage_type;
        struct storage {
            char_type value[decltype(F)::length];
        };
        static constexpr storage make()
        {
            storage s {};
            for (size_t i = 0; i < decltype(F)::length; ++i) {
                s.value[i] = F.at(i);
            }
            return s;
        }
        static constexpr storage data = make();
    };

    namespace fast_info_detail {
        // friend of bq::log, so fast_impl may read its private level words, buffer id and log id
        struct log_access;

        // declared here so the member templates of bq::log may call it; defined in bq_log_fast_info.h,
        // where bq::log is complete
        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_forceinline bool fast_impl(const bq::log& l, const Args&... args);
    }
    BQ_MODULE_EXPORT_END
}

#endif
