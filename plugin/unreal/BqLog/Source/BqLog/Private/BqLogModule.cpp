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
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "Misc/Paths.h"
#include "bq_log/bq_log.h"

// UE4 does not have UE_LOG_SOURCE_FILE
#ifndef UE_LOG_SOURCE_FILE
#define UE_LOG_SOURCE_FILE(x) x
#endif

struct bq_log_category_key
{
    uint64_t log_id_;
    int32_t  category_idx_;

    FORCEINLINE bool operator==(const bq_log_category_key& other) const
    {
        return log_id_ == other.log_id_ && category_idx_ == other.category_idx_;
    }
};

FORCEINLINE uint32_t GetTypeHash(const bq_log_category_key& key)
{
    uint32_t h1 = (uint32_t)(key.log_id_ ^ (key.log_id_ >> 32));
    uint32_t h2 = (uint32_t)key.category_idx_;
    return HashCombine(h1, h2);
}

class FBqLogModule : public IModuleInterface
{
private:
    static TMap<bq_log_category_key, FLogCategoryBase*> category_cache_;
    static FCriticalSection cache_lock_;

    static FLogCategoryBase& get_category(uint64_t log_id, int32_t category_idx)
    {
        bq_log_category_key key { log_id, category_idx };

        {
            FScopeLock lock(&cache_lock_);
            FLogCategoryBase** found = category_cache_.Find(key);
            if (found)
            {
                return **found;
            }
        }

        FString category_name;
        bool got_name = false;

        if (category_idx >= 0)
        {
            bq::_api_string_def cat_str;
            if (bq::api::__api_get_log_category_name_by_index(log_id, (uint32_t)category_idx, &cat_str)
                && cat_str.str && cat_str.len > 0)
            {
                category_name = FString(cat_str.len, UTF8_TO_TCHAR(cat_str.str));
                got_name = true;
            }
            bq::_api_string_def name_str;
            if (bq::api::__api_get_log_name_by_id(log_id, &name_str)
                && name_str.str && name_str.len > 0)
            {
                category_name =  TEXT("Bq|") + FString(name_str.len, UTF8_TO_TCHAR(name_str.str)) + TEXT("|") + category_name;
                got_name = true;
            }
        }


        if (!got_name)
        {
            category_name = TEXT("BqLog");
        }

        FLogCategoryBase* new_cat = new FLogCategoryBase(*category_name, ELogVerbosity::Log, ELogVerbosity::All);

        // double-check: another thread may have inserted while we were building the name
        FScopeLock lock(&cache_lock_);
        FLogCategoryBase** found = category_cache_.Find(key);
        if (found)
        {
            delete new_cat;
            return **found;
        }
        category_cache_.Add(key, new_cat);
        return *new_cat;
    }

    static void BQ_STDCALL on_bq_log_console_callback(uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length)
    {
        const ANSICHAR* u8_str = reinterpret_cast<const ANSICHAR*>(content ? content : "");
        int32_t tchar_len = (length >= 0) ? length : FCStringAnsi::Strlen(u8_str);
        FUTF8ToTCHAR conv(u8_str, tchar_len);

        FLogCategoryBase& category = get_category(log_id, category_idx);
        const FName category_name = category.GetCategoryName();

        switch (log_level)
        {
        case bq::log_level::verbose:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::VeryVerbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::debug:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Verbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::info:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Display, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::warning:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Warning, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::error:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Error, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::fatal:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Fatal, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        default:
            FMsg::Logf_Internal(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, category_name, ELogVerbosity::Log, TEXT("Unknown Log Level: %.*s"), conv.Length(), conv.Get());
            break;
        }
    }
public:
    virtual void StartupModule() override
    {
#if WITH_EDITOR
        bq::log::register_console_callback(&on_bq_log_console_callback);
        bq::log::console(bq::log_level::info, "BqLog Editor console callback registered");
        bq::log::console(bq::log_level::info, bq::string("BqLog Version:") + bq::log::get_version());

        FString dir = FPaths::ProjectDir();
        FString full = FPaths::ConvertRelativePathToFull(dir);
        bq::log::reset_base_dir(0, bq::string(TCHAR_TO_UTF8(*full)));
        bq::log::reset_base_dir(1, bq::string(TCHAR_TO_UTF8(*full)));
#elif !BQ_HAS_DEFAULT_BASE_DIR
        // Consoles have no writable working directory, so use the engine's saved data.
        FString dir = FPaths::ProjectSavedDir();
        FString full = FPaths::ConvertRelativePathToFull(dir);
        bq::log::reset_base_dir(0, bq::string(TCHAR_TO_UTF8(*full)));
        bq::log::reset_base_dir(1, bq::string(TCHAR_TO_UTF8(*full)));
#endif
        bq::log::console(bq::log_level::info, (bq::string("UE BqLog Base Dir Type 0:") + bq::log::get_file_base_dir(0)).c_str());
        bq::log::console(bq::log_level::info, (bq::string("UE BqLog Base Dir Type 1:") + bq::log::get_file_base_dir(1)).c_str());
    }
    virtual void ShutdownModule() override
    {
        FScopeLock lock(&cache_lock_);
        for (auto& pair : category_cache_)
        {
            delete pair.Value;
        }
        category_cache_.Empty();
    }
};

TMap<bq_log_category_key, FLogCategoryBase*> FBqLogModule::category_cache_;
FCriticalSection FBqLogModule::cache_lock_;

IMPLEMENT_MODULE(FBqLogModule, BqLog)
