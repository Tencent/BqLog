#pragma once
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
#include "template/category_log_template_base.h"

namespace bq {
    class category_log_template_cpp : public category_log_template_base {
    public:
        category_log_template_cpp(const bq::string& class_name)
            : category_log_template_base(class_name)
        {
        }

    protected:
        virtual bq::string get_template_content() const override;
        virtual bq::string format(const bq::string& template_string, const category_node& root_node) const override;

    private:
        bq::string get_category_names_code_recursive(const category_node& node) const;
        bq::string get_category_class_code_recursive(const category_node& node, const bq::string& tab, uint64_t& index) const;

        bq::string get_category_names_code(const category_node& root_node) const;
        bq::string get_category_class_root_define_code(const category_node& root_node) const;
    };
}
