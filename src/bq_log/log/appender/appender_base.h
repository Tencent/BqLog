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
#include "bq_log/log/log_types.h"
#include "bq_log/log/layout.h"
#include "bq_log/log/log_level_bitmap.h"

namespace bq {
    class log_imp;
    class appender_base {
    public:
        enum appender_type {
            console,
            text_file,
            raw_file,
            compressed_file,
            type_count
        };

        appender_base();
        virtual ~appender_base();

    public:
        static const bq::string& get_config_name_by_type(const appender_type type);
        virtual void set_enable(bool enable);
        virtual bool get_enable();
        void clear();
        bool init(const bq::string& name, const bq::property_value& config_obj, const log_imp* parent_log);
        void log(const log_entry_handle& handle);

        inline log_level_bitmap get_log_level_bitmap() const
        {
            return log_level_bitmap_;
        }

        inline const bq::string& get_name() const
        {
            return name_;
        }

        inline appender_type get_type() const
        {
            return type_;
        }

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) = 0;
        virtual void log_impl(const log_entry_handle& handle) = 0;

    protected:
        bool is_gmt_time_;
        const log_imp* parent_log_;
        layout* layout_ptr_;
        appender_type type_;
        bool appenders_enable = true;
        bq::array<bq::string> categories_mask_config_;
        bq::array_inline<uint8_t> categories_mask_array_;

    private:
        log_level_bitmap log_level_bitmap_;
        bq::string name_;
    };
}
