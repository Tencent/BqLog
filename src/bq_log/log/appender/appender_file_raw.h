#pragma once
/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_log/log/appender/appender_file_binary.h"

namespace bq {
    class appender_file_raw : public appender_file_binary {
        friend class appender_decoder_raw;

    public:
        static constexpr uint32_t format_version = 5;

    protected:
        virtual void log_impl(const log_entry_handle& handle) override;
        virtual bq::string get_file_ext_name() override;
        virtual uint32_t get_binary_format_version() const override;

        virtual appender_file_binary::appender_format_type get_appender_format() const override
        {
            return appender_file_binary::appender_format_type::raw;
        }
    };
}
