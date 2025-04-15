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
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_types.h"

namespace bq {
    class appender_file_base : public appender_base {
        friend class appender_decoder_base;

    protected:
        struct parse_file_context {
        private:
            bq::string file_name_;

        public:
            size_t parsed_size = 0;

            parse_file_context(const bq::string& file_name)
                : file_name_(file_name)
            {
            }

            void log_parse_fail_reason(const char* msg) const;
        };
        struct write_with_cache_handle {
            friend class appender_file_base;

        private:
            uint8_t* data_;
            size_t alloc_len_;
            size_t used_len_;

        public:
            inline uint8_t* data() const { return data_; }
            inline size_t allcoated_len() const { return alloc_len_; }
            inline void reset_used_len(size_t new_len) { used_len_ = new_len; }
        };
        struct read_with_cache_handle {
            friend class appender_file_base;

        private:
            uint8_t* data_;
            size_t len_;

        public:
            bq_forceinline const uint8_t* data() const { return data_; }
            bq_forceinline size_t len() const { return len_; }
            bq_forceinline read_with_cache_handle offset(size_t offset)
            {
                read_with_cache_handle offset_handle;
                offset_handle.data_ = data_ + offset;
                offset_handle.len_ = len_ - offset;
                return offset_handle;
            }
        };

    public:
        virtual ~appender_file_base();

        bool is_output_file_in_sandbox() const;

        // flush appender output data from memory to Operation System IO.
        void flush_cache();
        // flush appender file to physical disk.
        void flush_io();

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;

        virtual void log_impl(const log_entry_handle& handle) override;

        virtual bool parse_exist_log_file(parse_file_context& context) = 0;

        virtual bq::string get_file_ext_name() = 0;

        virtual void on_file_open(bool is_new_created);

        void seek_read_file_absolute(size_t pos);

        void seek_read_file_offset(int32_t offset);

        size_t get_current_file_size();

        // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
        read_with_cache_handle read_with_cache(size_t size);

        void clear_read_cache();

        write_with_cache_handle write_with_cache_alloc(size_t size);

        void write_with_cache_commit(const write_with_cache_handle& handle);

    private:
        void open_new_indexed_file_by_name();

        bool is_file_oversize();

        void clear_all_expired_files(); // retention limit
        void clear_all_limit_files(); // capacity limit

        void refresh_file_handle(const log_entry_handle& handle);

        bool open_file_with_write_exclusive(const bq::string& file_path);

    private:
        bq::string config_file_name_;
        bool always_create_new_file_ = false;
        size_t max_file_size_;
        size_t current_file_size_;
        bq::file_handle file_;
        bool is_in_sandbox_ = false;
        uint64_t expire_time_ms_;
        uint64_t capacity_limit_ = 0;
        uint64_t current_file_expire_time_epoch_ms_;
        int64_t time_zone_diff_to_gmt_ms_;

        // Cache part
        // Caching can reduce calls to fwrite, enhancing file write performance.
        // Even though fwrite inherently possesses a caching mechanism,
        // the overhead of invoking fwrite itself is not to be overlooked.
#ifndef NDEBUG
        bool cache_write_already_allocated_ = false;
#endif
        bq::array<uint8_t> cache_read_;
        decltype(cache_read_)::size_type cache_read_cursor_ = 0;
        bq::array<uint8_t> cache_write_;
    };
}
