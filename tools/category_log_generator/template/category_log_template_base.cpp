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
#include <inttypes.h>
#include <stdio.h>
#include "template/category_log_template_base.h"

namespace bq {

    bq::string category_log_template_base::uint64_to_string(uint64_t value)
    {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "%" PRIu64 "", value);
        return tmp;
    }

    bq::string category_log_template_base::replace_with_tab_format(const bq::string& content, const bq::string& token, const bq::string& replace_str)
    {
        bq::string result;
        size_t search_start = 0;
        while (search_start < content.size()) {
            size_t end_pos = content.find(token, search_start);
            if (bq::string::npos == end_pos) {
                result += content.substr(search_start, content.size() - search_start);
                break;
            }
            result += content.substr(search_start, end_pos - search_start);

            size_t last_br_pos = result.find_last("\n");
            if (bq::string::npos == last_br_pos) {
                last_br_pos = 0;
            } else {
                ++last_br_pos;
            }
            size_t space_count = result.size() - last_br_pos;

            bq::string tab;
            for (size_t i = 0; i < space_count; ++i) {
                tab.push_back(' ');
            }

            size_t br_search_start = 0;
            while (br_search_start < replace_str.size()) {
                size_t br_end_pos = replace_str.find("\n", br_search_start);
                if (bq::string::npos == br_end_pos) {
                    br_end_pos = replace_str.size();
                } else {
                    ++br_end_pos;
                }
                if (br_search_start > 0) {
                    result += tab;
                }
                result += replace_str.substr(br_search_start, br_end_pos - br_search_start);
                br_search_start = br_end_pos;
            }

            search_start = end_pos + token.size();
        }
        return result;
    }

    category_log_template_base::category_log_template_base(const bq::string& class_name)
        : class_name_(class_name)
    {
    }

    bq::string category_log_template_base::generate(const category_node& root_node) const
    {
        const bq::string& template_content = get_template_content();
        return format(template_content, root_node);
    }

}