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
#include "bq_log/log/appender/appender_file_base.h"

namespace bq {
    class appender_file_text : public appender_file_base {
    protected:
        virtual void log_impl(const log_entry_handle& handle) override;
        virtual bool parse_exist_log_file(parse_file_context& context) override;
        virtual void on_file_open(bool is_new_created) override;
        virtual bq::string get_file_ext_name() override;
        virtual void on_appender_file_recovery_begin() override;
        virtual void on_appender_file_recovery_end() override;
        virtual void on_log_item_recovery_begin() override;
        virtual void on_log_item_recovery_end() override;
    };
}
