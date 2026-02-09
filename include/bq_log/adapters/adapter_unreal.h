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
 * This is the only header file you need include in your project.
 *
 * \brief
 *
 * \author pippocao
 * \date 2024.07.29
 */
#include <stdint.h>
#include "bq_common/bq_common_public_include.h"

/**
 * You can define BQ_LOG_DISABLE_ADAPTER_FOR_UE macro to disable default Unreal adapter
 */
#if defined(ENGINE_MAJOR_VERSION) && !defined(BQ_LOG_DISABLE_ADAPTER_FOR_UE)
#include "CoreMinimal.h"

inline size_t bq_log_format_str_size(const FString& str)
{
    return (size_t)str.Len();
}

inline const char16_t* bq_log_format_str_chars(const FString& str)
{
    return (char16_t*)str.GetCharArray().GetData();
}

inline size_t bq_log_format_str_size(const FName& str)
{
    auto display_entry = str.GetDisplayNameEntry();
    if (!display_entry) {
        return 0;
    }
    return (size_t)display_entry->GetNameLength();
}

template <typename T>
class __bq_log_format_helper_for_fname {
public:
    static BQ_TLS TCHAR chars_cache[NAME_SIZE];
    static BQ_TLS char ansi_chars_cache[NAME_SIZE];
};

template <typename T>
BQ_TLS TCHAR __bq_log_format_helper_for_fname<T>::chars_cache[NAME_SIZE];
template <typename T>
BQ_TLS char __bq_log_format_helper_for_fname<T>::ansi_chars_cache[NAME_SIZE];

inline const char16_t* bq_log_format_str_chars(const FName& str)
{
    auto display_entry = str.GetDisplayNameEntry();
    if (!display_entry) {
        return nullptr;
        ;
    }
#if ENGINE_MAJOR_VERSION == 4
    if (display_entry->IsWide()) {
        static_assert(sizeof(WIDECHAR) == sizeof(char16_t), "only utf16 WIDECHAR is supported for default BqLog Unreal 4 apdater!");
        display_entry->GetWideName((WIDECHAR(&)[NAME_SIZE])__bq_log_format_helper_for_fname<int32_t>::chars_cache);
    } else {
        display_entry->GetAnsiName(__bq_log_format_helper_for_fname<int32_t>::ansi_chars_cache);
        // converted string data is stored on a LTS buffer, so it is safe until next call to ANSI_TO_TCHAR
        return (const char16_t*)ANSI_TO_TCHAR(__bq_log_format_helper_for_fname<int32_t>::ansi_chars_cache);
    }
#else
    display_entry->GetUnterminatedName(__bq_log_format_helper_for_fname<int32_t>::chars_cache, 1024);
#endif
    return (char16_t*)&__bq_log_format_helper_for_fname<int32_t>::chars_cache;
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