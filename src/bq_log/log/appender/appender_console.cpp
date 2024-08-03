/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_console.h"

namespace bq {
    appender_console::appender_console()
    {
    }

    void appender_console::register_console_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::platform::scoped_mutex lock(get_console_mutex());
        get_console_callbacks()[callback] = true;
    }

    void appender_console::unregister_console_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::platform::scoped_mutex lock(get_console_mutex());
        auto iter = get_console_callbacks().find(callback);
        if (iter != get_console_callbacks().end()) {
            get_console_callbacks().erase(iter);
        }
    }

    bool appender_console::init_impl(const bq::property_value& config_obj)
    {
        (void)config_obj;
        log_name_prefix_ += "[";
        log_name_prefix_ += parent_log_->get_name();
        log_name_prefix_ += "]\t";
        log_entry_cache_ = log_name_prefix_;
        return true;
    }

    void appender_console::log_impl(const log_entry_handle& handle)
    {
        auto layout_result = layout_ptr_->do_layout(handle, is_gmt_time_, &parent_log_->get_categories_name());
        if (layout_result != layout::enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "console layout error, result:%d, format str:%s", (int32_t)layout_result, handle.get_format_string_data());
            return;
        }
        log_entry_cache_.erase(log_entry_cache_.begin() + log_name_prefix_.size(), (log_entry_cache_.size() - log_name_prefix_.size()));
        const char* text_log_data = layout_ptr_->get_formated_str();
        uint32_t log_text_len = layout_ptr_->get_formated_str_len();
        log_entry_cache_.insert_batch(log_entry_cache_.end(), text_log_data, log_text_len);
        auto level = handle.get_level();

#if !BQ_UNIT_TEST
        util::log_device_console_plain_text(level, log_entry_cache_.c_str());
#endif
        if (get_console_callbacks().size() > 0) {
            bq::platform::scoped_mutex lock(get_console_mutex());
            for (auto iter = get_console_callbacks().begin(); iter != get_console_callbacks().end(); ++iter) {
                bq::type_func_ptr_console_callback callback = iter->key();
                callback(parent_log_->id(), handle.get_category_idx(), (int32_t)level, log_entry_cache_.c_str(), (int32_t)log_entry_cache_.size());
            }
        }
    }

    bq::platform::mutex& appender_console::get_console_mutex()
    {
        static bq::platform::mutex mutex_;
        return mutex_;
    }
    bq::hash_map<bq::type_func_ptr_console_callback, bool>& appender_console::get_console_callbacks()
    {
        static bq::hash_map<bq::type_func_ptr_console_callback, bool> callbacks_;
        return callbacks_;
    }
}
