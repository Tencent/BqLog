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
/*!
 * \class appender_file_compressed
 * \author pippocao
 * \
 *
 * Appender for compressed log file.
 * This appender might consume slightly more CPU than the appender_file_raw,
 * but in most cases, it can significantly reduce the size of log files.
 *
 * Most of the time, the same combination of log format, level, and category appears multiple times in a single log file,
 * with the only difference between different log entries being the parameters that follow.
 * Therefore, we can create a template for them and use an index to reference them.
 * Additionally, using VLQ encoding extensively can help reduce the file size.
 * this appender may be slight
 *
 * Structure of compressed log file:
 * [Common Binary Header][Data Section]
 *
 *
 * 【Data Section 】
 * The compressed log's data section consists of data items, which can be of two types distinguished by their data_item_header.
 * Here are the data structures for the two data item types:
 * 1. Log Template:
 *	[data_item_header][data(Log Template)]
 * 2. Log Entry:
 *	[data_item_header][data(Log Entry)]
 *
 * The structure of the data_item_header is as follows:   (Final data block length: If data_len_extra (7 bits) is all zeros, then it is VLQ_decode(data_len_base); otherwise, it is VLQ_decode([[0(1 bit), data_len_extra(7 bits)], data_len_base]).)
 *	[[type(1 bit), data_len_extra(7 bits)], data_len_base(VLQ (4 bytes max), don't include data_len self)]

 * The data structures for the two types of data are:
 * 1. data(Log Template):
 *  [sub type(1 byte][sub type data(see sub types bellow)]
 *  there are two sub types of Log Template:
 *	1.1 Format Template: [level(1 byte), category_idx(VLQ), utf8_fmt_data(str, 0 bytes or more)]
 *	1.2 Thread Info Template: [thread_info_template idx(VLQ), thread_id(VLQ 64bits), thread name str utf-8]
 * 2. data(Log Entry):
 *	(epoch offset milliseconds)(VLQ), [(formate_template idx)(VLQ), (thread_info_template idx)(VLQ), [param_type(1 byte), param(same as raw data, not aligned) ...]]

 */
#include "bq_log/log/appender/appender_file_binary.h"

namespace bq {
    class appender_file_compressed : public appender_file_binary {
        friend class appender_decoder_compressed;
        enum item_type : uint8_t {
            log_template = 0,
            log_entry = 128, // 0b10000000
        };
        enum template_sub_type : uint8_t {
            format_template = 0,
            thread_info_template = 1
        };

    public:
        static constexpr uint32_t format_version = 8;

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;

        virtual void log_impl(const log_entry_handle& handle) override;

        virtual bool parse_exist_log_file(parse_file_context& context) override;

        virtual void on_file_open(bool is_new_created) override;

        virtual appender_file_binary::appender_format_type get_appender_format() const override
        {
            return appender_file_binary::appender_format_type::compressed;
        }

        virtual bq::string get_file_ext_name() override;

        virtual uint32_t get_binary_format_version() const override;

    private:
        bq::tuple<bool, appender_file_compressed::item_type, appender_file_base::read_with_cache_handle> read_item_data(parse_file_context& context);

        bool parse_log_entry(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle);

        bool parse_formate_template(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle);

        bool parse_thread_info_template(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle);

        uint64_t get_format_template_hash(bq::log_level level, uint32_t category_idx, uint64_t fmt_str_hash);

        void reset();

    private:
        bq::hash_map_inline<uint64_t, uint32_t> format_templates_hash_cache_;
        uint32_t current_format_template_max_index_;
        bq::hash_map_inline<uint64_t, uint32_t> thread_info_hash_cache_;
        uint32_t current_thread_info_max_index_;
        uint64_t last_log_entry_epoch_;
        bq::u16string utf16_trans_cache_;
    };
}
