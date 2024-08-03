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
#include <iostream>
#include "bq_common/bq_common.h"

inline void output_config_file_format(std::ostream& stream)
{
    stream << "format of category_config_file:" << std::endl;
    stream << "####################config file start#################" << std::endl;
    stream << "\tModuleA //comment" << std::endl;
    stream << "\tModuleA.SystemA " << std::endl;
    stream << "\tModuleA.SystemB.functionC //comment " << std::endl;
    stream << "\tModuleC //comment" << std::endl;
    stream << "\tModuleD //comment" << std::endl;
    stream << "####################config file end#################" << std::endl;
    stream << "important: only character, digit and _ is allowed for category parts, category parts was split by ., and first character of each part must be an English letter, just like variable name" << std::endl;
}

struct category_node {
private:
    bq::string name_;
    bq::string comment_;
    bq::array<bq::unique_ptr<category_node>> child_nodes_;
    const category_node* parent_ = nullptr;

public:
    category_node& get_or_add_child_node(const bq::string& category)
    {
        bq::array<bq::string> category_parts = category.split(".");
        category_node* target = this;
        for (decltype(category_parts)::size_type i = 0; i < category_parts.size(); ++i) {
            const bq::string& part = category_parts[i];
            decltype(child_nodes_)::iterator it = target->child_nodes_.end();
            for (auto j = target->child_nodes_.begin(); j != target->child_nodes_.end(); ++j) {
                if ((*j)->name_ == part) {
                    it = j;
                    break;
                }
            }
            if (it == target->child_nodes_.end()) {
                bq::unique_ptr<category_node> new_node = bq::make_unique<category_node>();
                new_node->name_ = part;
                new_node->parent_ = target;
                target->child_nodes_.push_back(bq::move(new_node));
                target = (target->child_nodes_[target->child_nodes_.size() - 1]).operator->();
            } else {
                target = (*it).operator->();
            }
        }
        return *target;
    }

    size_t get_all_nodes_count() const
    {
        size_t count = 1;
        for (const auto& child : get_all_children()) {
            count += child->get_all_nodes_count();
        }
        return count;
    }

    const bq::array<bq::unique_ptr<category_node>>& get_all_children() const
    {
        return child_nodes_;
    }

    const category_node* parent() const
    {
        return parent_;
    }

    const bq::string& name() const
    {
        return name_;
    }

    void set_comment(const bq::string& comment)
    {
        comment_ = comment;
    }

    const bq::string& comment() const
    {
        return comment_;
    }

    bq::string full_name() const
    {
        if (!parent_) {
            return "";
        }
        bq::string result = name_;
        const category_node* target = parent_;
        while (target) {
            if (!target->name_.is_empty()) {
                result = target->name_ + "." + result;
            }
            target = target->parent_;
        }
        return result;
    }
};
