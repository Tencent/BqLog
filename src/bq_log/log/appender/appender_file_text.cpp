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
#include "bq_log/log/appender/appender_file_text.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/global/log_vars.h"

namespace bq {

    void appender_file_text::log_impl(const log_entry_handle& handle)
    {
        appender_file_base::log_impl(handle);
        auto layout_result = layout_ptr_->do_layout(handle, time_zone_, &parent_log_->get_categories_name());
        if (layout_result != layout::enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "text file layout error, result:%d, format str:%s", (int32_t)layout_result, handle.get_format_string_data());
            return;
        }
        const char* write_data = layout_ptr_->get_formated_str();
        size_t data_len = (size_t)layout_ptr_->get_formated_str_len();
        auto write_handle = alloc_write_cache(data_len + sizeof('\n'));
        memcpy(write_handle.data(), write_data, data_len);
        write_handle.data()[data_len] = (uint8_t)'\n';
        layout_ptr_->tidy_memory();
        return_write_cache(write_handle);
        mark_write_finished();
    }

    bool appender_file_text::parse_exist_log_file(parse_file_context& context)
    {
        (void)context;
        return true;
    }

    void appender_file_text::on_file_open(bool is_new_created)
    {
        appender_file_base::on_file_open(is_new_created);
    }

    bq::string appender_file_text::get_file_ext_name()
    {
        return ".log";
    }

    void appender_file_text::on_appender_file_recovery_begin() {
        auto write_handle = alloc_write_cache(strlen(log_global_vars::get().log_recover_start_str_) + sizeof('\n'));
        memcpy(write_handle.data(), log_global_vars::get().log_recover_start_str_, strlen(log_global_vars::get().log_recover_start_str_));
        write_handle.data()[strlen(log_global_vars::get().log_recover_start_str_)] = (uint8_t)'\n';
        return_write_cache(write_handle);
    }

    void appender_file_text::on_appender_file_recovery_end() {
        auto write_handle = alloc_write_cache(strlen(log_global_vars::get().log_recover_end_str_) + sizeof('\n'));
        memcpy(write_handle.data(), log_global_vars::get().log_recover_end_str_, strlen(log_global_vars::get().log_recover_end_str_));
        write_handle.data()[strlen(log_global_vars::get().log_recover_end_str_)] = (uint8_t)'\n';
        return_write_cache(write_handle);
    }

    void appender_file_text::on_log_item_recovery_begin() {
        auto write_handle = alloc_write_cache(strlen(log_global_vars::get().log_recover_start_str_) + sizeof('\n'));
        memcpy(write_handle.data(), log_global_vars::get().log_recover_start_str_, strlen(log_global_vars::get().log_recover_start_str_));
        write_handle.data()[strlen(log_global_vars::get().log_recover_start_str_)] = (uint8_t)'\n';
        return_write_cache(write_handle);
    }

    void appender_file_text::on_log_item_recovery_end() {
        auto write_handle = alloc_write_cache(strlen(log_global_vars::get().log_recover_end_str_) + sizeof('\n'));
        memcpy(write_handle.data(), log_global_vars::get().log_recover_end_str_, strlen(log_global_vars::get().log_recover_end_str_));
        write_handle.data()[strlen(log_global_vars::get().log_recover_end_str_)] = (uint8_t)'\n';
        return_write_cache(write_handle);
    }

}
