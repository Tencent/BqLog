/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
#pragma once
#include "template/category_log_template_base.h"

namespace bq {
    class category_log_template_go : public category_log_template_base {
    public:
        explicit category_log_template_go(const bq::string& class_name)
            : category_log_template_base(class_name)
        {
        }

    protected:
        bq::string get_template_content() const override;
        bq::string format(const bq::string& template_string, const category_node& root_node) const override;

    private:
        bq::string category_names(const category_node& node) const;
        bq::string category_types(const category_node& node, const bq::string& type_name) const;
        bq::string category_init(const category_node& node, uint64_t& index) const;
        bq::string field_path(const category_node& node) const;
        bq::string node_type_name(const category_node& node) const;
        static bq::string field_name(const bq::string& segment);
    };
}
