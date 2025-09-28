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
#include "bq_log/log/decoder/appender_decoder_base.h"
#include "bq_log/log/appender/appender_file_compressed.h"

namespace bq {
    class appender_decoder_compressed : public appender_decoder_base {
        struct decoder_log_template {
            uint32_t category_idx = (uint32_t)-1;
            bq::log_level level = bq::log_level::log_level_max;
            uint64_t epoch_ms = (uint64_t)(-1);
            bq::string fmt_string;
        };
        struct decoder_thread_info_template {
            uint64_t thread_id;
            bq::string thread_name;
        };

    protected:
        virtual appender_decode_result init_private() override;

        virtual appender_decode_result decode_private() override;

        virtual uint32_t get_binary_format_version() const override;

    private:
        bq::tuple<appender_decode_result, appender_file_compressed::item_type, appender_decoder_base::read_with_cache_handle> read_item_data();

        appender_decode_result parse_log_entry(const appender_decoder_base::read_with_cache_handle& read_handle);

        appender_decode_result parse_formate_template(const appender_decoder_base::read_with_cache_handle& read_handle);

        appender_decode_result parse_thread_info_template(const appender_decoder_base::read_with_cache_handle& read_handle);

    private:
        uint64_t last_log_entry_epoch_;
        bq::array<decoder_log_template> log_templates_array_;
        bq::hash_map<uint64_t, decoder_thread_info_template> thread_info_templates_map_;
        bq::array<uint8_t> raw_data_;
    };
}
