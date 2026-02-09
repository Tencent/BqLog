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
#include <iostream>
#include "bq_common/bq_common.h"

inline void output_config_file_format(std::ostream& stream)
{
    stream << "CategoryConfigFile format:\n"
           << "  - One category path per line.\n"
           << "  - Hierarchy separator: a dot (\".\").\n"
           << "  - Parent categories are auto-generated; you do NOT need to list them explicitly.\n"
           << "    For example: \"Shop.Manager\" automatically creates both \"Shop\" and \"Shop.Manager\".\n"
           << "  - Comments start with // anywhere on the line (inline or full-line comments).\n"
           << "  - Blank lines and leading/trailing spaces are ignored.\n"
           << "  - Duplicate entries are allowed; duplicates are de-duplicated.\n"
           << "  - Encoding is expected to be UTF-8.\n\n"
           << "Example CategoryConfigFile content:\n"
           << "// This configuration file supports comments using double slashes\n"
           << "Shop.Manager  // You don't need to list Shop separately; this will automatically generate both Shop and Shop.Manager categories\n"
           << "Shop.Seller\n"
           << "Factory.People.Manager\n"
           << "Factory.People.Worker\n"
           << "Factory.Machine\n"
           << "Factory.House\n"
           << "Transport.Vehicles.Driver\n"
           << "Transport.Vehicles.Maintenance\n"
           << "Transport.Trains\n\n"
           << "Notes:\n"
           << "  - Use simple dot-separated identifiers (e.g., Name or Name.Sub). Avoid empty segments such as \"A..B\".\n"
           << "  - The order of lines does not affect the result; the tool builds the full tree from the paths.\n\n";
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
