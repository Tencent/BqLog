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
#include "template/category_log_template_base.h"

namespace bq {
    // Verse (UE6, BqLogVerse plugin): a module named after the class, holding create_log,
    // get_log_by_name, the log class (a bq.category_log) and the category tree reached
    // through log.cat.
    class category_log_template_verse : public category_log_template_base {
    public:
        category_log_template_verse(const bq::string& class_name)
            : category_log_template_base(class_name)
        {
        }

    protected:
        virtual bq::string get_template_content() const override;
        virtual bq::string format(const bq::string& template_string, const category_node& root_node) const override;

    private:
        // Field names of the children of node, in order: the segment itself, or with '_'
        // appended while it is taken in Verse or by an earlier sibling.
        bq::array<bq::string> child_field_names(const category_node& node) const;
        bool is_taken(const bq::string& name) const;
        bq::string node_class_name(const category_node& node) const;
        bq::string field_type(const category_node& node) const;
        static bq::string field_comment(const category_node& node, const bq::string& field_name);

        static bq::string category_names(const category_node& node);
        bq::string category_classes(const category_node& node) const;
        // The fields of the archetype of node: one per child, each with its index, numbered
        // in the order of category_names. index is the last index used so far.
        bq::string category_values(const category_node& node, uint64_t& index) const;
    };
}
