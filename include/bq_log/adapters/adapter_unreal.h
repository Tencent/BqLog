/* Copyright (C) 2025 Tencent.
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
/*!
 * This is the only header file you need include in your project.
 *
 * \brief
 *
 * \author pippocao
 * \date 2024.07.29
 */
#if !defined(BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED)
#include <stdint.h>
#endif
#include "bq_common/bq_common_public_include.h"

#if defined(BQ_LOG_UE_PLUGIN)
#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

inline size_t bq_log_format_str_size(const FString& str)
{
    return (size_t)str.Len();
}

inline const char16_t* bq_log_format_str_chars(const FString& str)
{
    return (char16_t*)str.GetCharArray().GetData();
}

// An FName such as "Actor_12" is stored as the entry "Actor" plus a number, and FName::ToString
// appends "_12" for it. Length of that suffix: at most 11 characters, 0 when the name has no number.
inline int32 __bq_log_fname_suffix_len(int32 internal_number)
{
    if (internal_number == NAME_NO_NUMBER_INTERNAL) {
        return 0;
    }
    uint32 value = static_cast<uint32>(NAME_INTERNAL_TO_EXTERNAL(internal_number));
    int32 len = 2;
    while (value >= 10U) {
        value /= 10U;
        ++len;
    }
    return len;
}

// Writes the suffix_len characters measured by __bq_log_fname_suffix_len to out.
inline void __bq_log_fname_write_suffix(int32 internal_number, int32 suffix_len, TCHAR* out)
{
    if (suffix_len == 0) {
        return;
    }
    uint32 value = static_cast<uint32>(NAME_INTERNAL_TO_EXTERNAL(internal_number));
    out[0] = TEXT('_');
    for (int32 i = suffix_len - 1; i > 0; --i) {
        out[i] = static_cast<TCHAR>(TEXT('0') + value % 10U);
        value /= 10U;
    }
}

inline size_t bq_log_format_str_size(const FName& str)
{
    auto display_entry = str.GetDisplayNameEntry();
    if (!display_entry) {
        return 0;
    }
    return static_cast<size_t>(display_entry->GetNameLength() + __bq_log_fname_suffix_len(str.GetNumber()));
}

template <typename T>
class __bq_log_format_helper_for_fname {
public:
    // The longest entry plus a number suffix.
    static BQ_TLS TCHAR chars_cache[NAME_SIZE + 16];
    static BQ_TLS char ansi_chars_cache[NAME_SIZE];
};

template <typename T>
BQ_TLS TCHAR __bq_log_format_helper_for_fname<T>::chars_cache[NAME_SIZE + 16];
template <typename T>
BQ_TLS char __bq_log_format_helper_for_fname<T>::ansi_chars_cache[NAME_SIZE];

inline const char16_t* bq_log_format_str_chars(const FName& str)
{
    auto display_entry = str.GetDisplayNameEntry();
    if (!display_entry) {
        return nullptr;
    }
    TCHAR* chars = __bq_log_format_helper_for_fname<int32_t>::chars_cache;
    const int32 name_len = display_entry->GetNameLength();
// FNameEntry::GetUnterminatedName(TCHAR*, uint32) was not CORE_API-exported
// until UE 5.2. On UE 4.x and UE 5.0/5.1 we take the stable GetWideName /
// GetAnsiName path
#if ENGINE_MAJOR_VERSION == 4 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2)
    if (display_entry->IsWide()) {
        static_assert(sizeof(WIDECHAR) == sizeof(char16_t), "only utf16 WIDECHAR is supported for default BqLog Unreal apdater!");
        display_entry->GetWideName(*reinterpret_cast<WIDECHAR(*)[NAME_SIZE]>(chars));
    } else {
        // Widened into chars_cache as well. ANSI_TO_TCHAR would return a pointer into a temporary
        // that is destroyed when this function returns, and the caller would then copy whatever
        // has reused that stack memory.
        display_entry->GetAnsiName(__bq_log_format_helper_for_fname<int32_t>::ansi_chars_cache);
        const char* ansi_chars = __bq_log_format_helper_for_fname<int32_t>::ansi_chars_cache;
        for (int32 i = 0; i < name_len; ++i) {
            chars[i] = static_cast<TCHAR>(static_cast<uint8>(ansi_chars[i]));
        }
    }
#else
    display_entry->GetUnterminatedName(chars, NAME_SIZE);
#endif
    const int32 number = str.GetNumber();
    __bq_log_fname_write_suffix(number, __bq_log_fname_suffix_len(number), chars + name_len);
    return reinterpret_cast<const char16_t*>(chars);
}

inline size_t bq_log_format_str_size(const FText& text)
{
    return bq_log_format_str_size(text.ToString());
}

inline const char16_t* bq_log_format_str_chars(const FText& text)
{
    return bq_log_format_str_chars(text.ToString());
}
#endif