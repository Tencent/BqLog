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
#include "bq_common/bq_common.h"
#include "common_header.h"
#include "category_generator.h"
#include "template/category_log_template_cpp.h"
#include "template/category_log_template_csharp.h"
#include "template/category_log_template_java.h"
#include "template/category_log_template_typescript.h"

namespace bq {
    bool category_generator::parse_config_file(const bq::string& file_path, category_node& category_root)
    {
        bq::array<bq::string> lines;
        bq::string abs_path = TO_ABSOLUTE_PATH(file_path, true);
        if (!bq::file_manager::is_file(abs_path)) {
            bq::util::log_device_console(bq::log_level::error, "failed to read CategoryConfigFile:%s", abs_path.c_str());
            return false;
        }
        bq::string content = bq::file_manager::instance().read_all_text(abs_path);
        content = content.replace("\r\n", "\n");
        if (content.is_empty()) {
            bq::util::log_device_console(bq::log_level::error, "failed to read CategoryConfigFile:%s, file content is empty", abs_path.c_str());
            return false;
        }
        size_t search_start = 0;
        size_t line_num = 0;
        while (search_start < content.size()) {
            size_t end_pos = content.find("\n", search_start);
            if (bq::string::npos == end_pos) {
                end_pos = content.size();
            }
            string line = content.substr(search_start, end_pos - search_start);
            search_start = end_pos + 1;
            bq::string comment;
            bq::string category;
            auto pos = line.find("//");
            if (pos != bq::string::npos) {
                comment = line.substr(pos + 2);
                category = line.substr(0, pos);
            }
            else {
                category = line;
            }
            category = category.trim();
            if (category.is_empty()) {
                continue;
            }
            for (bq::string::size_type check_index = 0; check_index < category.size(); ++check_index) {
                bq::string::value_type c = category[check_index];
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_')) {
                    continue;
                }
                if ((c >= '0' && c <= '9') && check_index > 0) {
                    continue;
                }
                if (check_index > 0 && c == '.') {
                    continue;
                }
                bq::util::log_device_console(bq::log_level::error, "category config file format error at line %" PRIu64 ", content is :%s", static_cast<uint64_t>(line_num), line.c_str());
                output_config_file_format(std::cerr);
                return false;
            }
            category_node& child_node = category_root.get_or_add_child_node(category);
            child_node.set_comment(comment);
            search_start = end_pos + 1;
            ++line_num;
        }
        return true;
    }

    bool category_generator::general_log_class(const bq::string& class_name, const bq::string& config_file, const bq::string& output_path)
    {
        category_node root_node;
        if (!parse_config_file(config_file, root_node)) {
            return false;
        }
        bq::string code;
        bq::string abs_dir = TO_ABSOLUTE_PATH(output_path, true);

        category_log_template_cpp cpp(class_name);
        code = cpp.generate(root_node);
        bq::file_manager::instance().write_all_text(bq::file_manager::combine_path(abs_dir, class_name + ".h"), code);
        bq::util::log_device_console(log_level::info, "code generated:%s", bq::file_manager::combine_path(abs_dir, class_name + ".h").c_str());

        category_log_template_csharp csharp(class_name);
        code = csharp.generate(root_node);
        bq::file_manager::instance().write_all_text(bq::file_manager::combine_path(abs_dir, class_name + ".cs"), code);
        bq::util::log_device_console(log_level::info, "code generated:%s", bq::file_manager::combine_path(abs_dir, class_name + ".cs").c_str());

        category_log_template_java java(class_name);
        code = java.generate(root_node);
        bq::file_manager::instance().write_all_text(bq::file_manager::combine_path(abs_dir, class_name + ".java"), code);
        bq::util::log_device_console(log_level::info, "code generated:%s", bq::file_manager::combine_path(abs_dir, class_name + ".java").c_str());
        
        category_log_template_typescript ts(class_name);
        code = ts.generate(root_node);
        bq::file_manager::instance().write_all_text(bq::file_manager::combine_path(abs_dir, class_name + ".ts"), code);
        bq::util::log_device_console(log_level::info, "code generated:%s", bq::file_manager::combine_path(abs_dir, class_name + ".ts").c_str());

        return true;
    }
}
