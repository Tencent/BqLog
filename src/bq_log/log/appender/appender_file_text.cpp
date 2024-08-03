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
#include <assert.h>
#include "bq_log/log/appender/appender_file_text.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    // static char appender_file_text_end_line[] = { '\r', '\n' };    //use LF intead of CRLF

    void appender_file_text::log_impl(const log_entry_handle& handle)
    {
        appender_file_base::log_impl(handle);
        auto layout_result = layout_ptr_->do_layout(handle, is_gmt_time_, &parent_log_->get_categories_name());
        if (layout_result != layout::enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "text file layout error, result:%d, format str:%s", (int32_t)layout_result, handle.get_format_string_data());
            return;
        }
        const char* write_data = layout_ptr_->get_formated_str();
        size_t data_len = (size_t)layout_ptr_->get_formated_str_len();
        auto write_handle = write_with_cache_alloc(data_len + sizeof('\n'));
        memcpy(write_handle.data(), write_data, data_len);
        write_handle.data()[data_len] = (uint8_t)'\n';
        write_with_cache_commit(write_handle);
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

    static const bq::string log_file_ext_name_text = ".log";
    bq::string appender_file_text::get_file_ext_name()
    {
        return log_file_ext_name_text;
    }

}
