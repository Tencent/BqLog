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
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "bq_common/bq_common.h"
#include "bq_log/log/appender/appender_file_binary.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    bool appender_file_binary::parse_exist_log_file(parse_file_context& context)
    {
        uint32_t format_version = get_binary_format_version();

        auto size_current = get_current_file_size();
        if (size_current == 0) {
            return true;
        }
        if (size_current <= sizeof(_binary_appender_head_def)) {
            context.log_parse_fail_reason("file size to small, it should be larger than binary appender head");
            return false;
        }
        seek_read_file_absolute(0);
        auto read_handle = read_with_cache(sizeof(_binary_appender_head_def));
        if (read_handle.len() < sizeof(_binary_appender_head_def)) {
            context.log_parse_fail_reason("read binary appender head failed");
            return false;
        }
        _binary_appender_head_def head;
        memcpy(&head, read_handle.data(), sizeof(head));
        if (head.version != format_version) {
            context.log_parse_fail_reason("format version incompatible");
            return false;
        }
        if (head.format != get_appender_format()) {
            context.log_parse_fail_reason("format incompatible");
            return false;
        }
        if (head.category_count != parent_log_->get_categories_count()) {
            context.log_parse_fail_reason("category count miss match");
            return false;
        }
        context.parsed_size = sizeof(head);
        uint32_t category_count = head.category_count;
        for (uint32_t i = 0; i < category_count; ++i) {
            read_handle = read_with_cache(sizeof(uint32_t));
            if (read_handle.len() < sizeof(uint32_t)) {
                context.log_parse_fail_reason("read category name length failed");
                return false;
            }
            uint32_t name_len;
            memcpy(&name_len, read_handle.data(), sizeof(name_len));
            if ((size_t)name_len != parent_log_->get_categories_name()[i].size()) {
                context.log_parse_fail_reason("category name length mismatch");
                return false;
            }
            context.parsed_size += sizeof(name_len);
            read_handle = read_with_cache((size_t)name_len);
            if (read_handle.len() < (size_t)name_len) {
                context.log_parse_fail_reason("read category name mismatch");
                return false;
            }
            if (memcmp(read_handle.data(), parent_log_->get_categories_name()[i].c_str(), (size_t)name_len) != 0) {
                context.log_parse_fail_reason("category name mismatch");
                return false;
            }
            context.parsed_size += (size_t)name_len;
        }
        return true;
    }

    void appender_file_binary::on_file_open(bool is_new_created)
    {
        appender_file_base::on_file_open(is_new_created);
        if (is_new_created) {
            write_file_header();
        }
    }

    void appender_file_binary::write_file_header()
    {
        _binary_appender_head_def head;
        head.format = get_appender_format();
        head.version = get_binary_format_version();
        head.is_gmt = is_gmt_time_;
        head.category_count = parent_log_->get_categories_count();
        auto handle = write_with_cache_alloc(sizeof(head));
        memcpy(handle.data(), &head, handle.allcoated_len());
        write_with_cache_commit(handle);
        for (uint32_t i = 0; i < head.category_count; ++i) {
            const bq::string& category_name = parent_log_->get_categories_name()[i];
            uint32_t name_len = (uint32_t)category_name.size();
            handle = write_with_cache_alloc(sizeof(name_len) + name_len);
            memcpy(handle.data(), &name_len, sizeof(name_len));
            memcpy(handle.data() + sizeof(name_len), category_name.c_str(), name_len);
            write_with_cache_commit(handle);
        }
    }
}
