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
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/log_types.h"

namespace bq {
    class appender_file_binary : public appender_file_base {
    public:
        enum class appender_format_type : uint8_t {
            raw = 1,
            compressed
        };

        BQ_PACK_BEGIN
        struct _binary_appender_head_def {
            uint32_t version;
            appender_format_type format;
            bool is_gmt;
            uint32_t category_count;
        } BQ_PACK_END

    protected:
        virtual bool parse_exist_log_file(parse_file_context& context) override;
        virtual void on_file_open(bool is_new_created) override;
        virtual appender_format_type get_appender_format() const = 0;
        virtual uint32_t get_binary_format_version() const = 0;

    private:
        void write_file_header();
    };
}
