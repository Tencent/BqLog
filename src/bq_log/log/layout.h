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
/*!
 * \file layout.h
 *
 *
 * This is text formatter of Bq log.
 *
 * Important!: do_layout use static members to storage format content.
 * So the function "do_layout" must not be called from different threads at the same time.
 */

#include "bq_common/bq_common.h"
#include "bq_log/log/log_types.h"

namespace bq {
    class layout {
        struct format_info {
            bool used = false;
            char fill = ' ';
            char align = '<'; // <>^=
            char sign = '-'; //+ - default(-)
            char prefix = ' ';
            uint32_t offset = 0;
            uint32_t width = 0; // total width
            uint32_t precision = 0xFFFFFFFF; // float width
            char type = 'd'; // b d x f e
            void reset(){
                used = false;
                fill = ' ';
                align = '<'; // <>^=
                sign = '-'; //+ - default(-)
                prefix = ' ';
                offset = 0;
                width = 0; // total width
                precision = 0xFFFFFFFF; // float width
                type = 'd'; // b d x f e
            }
        };

    public:
        enum class enum_layout_result {
            finished, // layout succeed, all the contents have been formated.
            to_be_continue, // cache not enough, read the formated content and call layout again to get the rest content
            parse_error, // error
        };

    public:
        layout();

        enum_layout_result do_layout(const bq::log_entry_handle& log_entry, bool gmt_time, const bq::array<bq::string>* categories_name_array_ptr);

        inline const char* get_formated_str()
        {
            if (format_content.is_empty()) {
                return nullptr;
            }
            return &format_content[0];
        }

        inline uint32_t get_formated_str_len() const
        {
            return format_content_cursor;
        }

    private:
        enum_layout_result layout_prefix(const bq::log_entry_handle& log_entry);

        enum_layout_result insert_time(const bq::log_entry_handle& log_entry);

        enum_layout_result insert_thread_info(const bq::log_entry_handle& log_entry);

        /// <summary>
        /// example    ("This is param0:{0}, param1:{1}, param0, param1")
        /// </summary>
        /// <param name="log_item"></param>
        void python_style_format_content(const bq::log_entry_handle& log_entry);

        void python_style_format_content_utf8(const bq::log_entry_handle& log_entry);

        void python_style_format_content_utf16(const bq::log_entry_handle& log_entry);

        void clear_format_content();

        template <typename T>
        format_info c20_format(const T* style, int32_t len);

        void fill_and_alignment(uint32_t wirte_begin_pos);

        bq_forceinline void expand_format_content_buff_size(uint32_t new_size);

        //------------------------- insert functions begin ----------------------//
        uint32_t insert_str_utf8(const char* str, const uint32_t len);

        uint32_t insert_str_utf16(const char* str, const uint32_t len);

        void insert_pointer(const void* ptr);

        void insert_bool(bool value);

        void insert_char(char value);

        void insert_char16(char16_t value);

        void insert_char32(char32_t value);

        void insert_integral_unsigned(uint64_t value, int32_t base = 10);

        void insert_integral_signed(int64_t value, int32_t base = 10);

        void insert_decimal(float value);

        void insert_decimal(double value);

        void revert(uint32_t begin_cursor, uint32_t end_cursor);
        //------------------------- insert functions end ----------------------//
    private:
        bool is_gmt_time_;
        static constexpr uint32_t MAX_TIME_STR_LEN = 128;
        char time_cache_[MAX_TIME_STR_LEN + 1];
        size_t time_cache_len_;
        uint64_t last_time_epoch_cache_ = 0;
        const bq::array<bq::string>* categories_name_array_ptr_;
        // todo: use bq::string instead, but need optimize performance
        bq::array<char> format_content;
        uint32_t format_content_cursor;
        bq::hash_map<uint64_t, bq::string> thread_names_cache_;
        format_info format_info_;
    };
}
