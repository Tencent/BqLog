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
#include "bq_common/bq_common.h"
#include "common_header.h"
namespace bq {
    class category_log_template_base {
    protected:
        const bq::string class_name_;

    protected:
        virtual bq::string get_template_content() const = 0;
        // Languages that can be imported return their module interface text here; an empty
        // string means the language has no module entry and no file is generated for it.
        virtual bq::string get_module_template_content() const { return bq::string(); }
        virtual bq::string format(const bq::string& template_string, const category_node& root_node) const = 0;

        static bq::string uint64_to_string(uint64_t value);

        static bq::string replace_with_tab_format(const bq::string& content, const bq::string& token, const bq::string& replace_str);

    public:
        category_log_template_base(const bq::string& class_name);
        bq::string generate(const category_node& root_node) const;
        // The module interface unit, or an empty string when this language has none.
        bq::string generate_module(const category_node& root_node) const;
    };
}
