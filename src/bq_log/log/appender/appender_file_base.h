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
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_types.h"
#include "bq_log/types/buffer/normal_buffer.h"

namespace bq {
    class appender_file_base : public appender_base {
        friend class appender_decoder_base;

    protected:
        static constexpr size_t DEFAULT_BUFFER_ALIGNMENT = 32;   //256 bites alignment to improve SIMD performance

        struct parse_file_context {
        private:
            bq::string file_name_;

        public:
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

        // flush appender output data from memory to Operation System IO.
        virtual void flush_cache();
        // flush appender file to physical disk.
        void flush_io();

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;

        virtual bool reset_impl(const bq::property_value& config_obj) override;

        virtual void log_impl(const log_entry_handle& handle) override;

        virtual bool parse_exist_log_file(parse_file_context& context) = 0;

        virtual bq::string get_file_ext_name() = 0;

        virtual void on_file_open(bool is_new_created);

        bool seek_read_file_absolute(size_t pos);

        void seek_read_file_offset(int32_t offset);

        void set_cache_write_data_alignment_offset(uint8_t offset);

        size_t get_current_file_size() const {return current_file_size_;}

        file_handle& get_file_handle() {return file_;}

        // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
        virtual read_with_cache_handle read_with_cache(size_t size);

        void clear_read_cache();

        write_with_cache_handle alloc_write_cache(size_t size);

        void return_write_cache(const write_with_cache_handle& handle);

        //Low performance function, try to use write cache first
        size_t direct_write(const void* data, size_t size, bq::file_manager::seek_option seek_opt, int64_t seek_offset);

        void mark_write_finished();

        virtual void on_appender_file_recovery_begin(){};
        virtual void on_appender_file_recovery_end(){};
    private:
        void set_basic_configs(const bq::property_value& config_obj);

        void refresh_head_size(bool need_recovery, const bq::string& mmap_file_abs_path);

        bool try_recover();

        void open_new_indexed_file_by_name();

        bool is_file_oversize();

        void clear_all_expired_files(); // retention limit

        void clear_all_limit_files(); // capacity limit

        void refresh_file_handle(const log_entry_handle& handle);

        bool open_file_with_write_exclusive(const bq::string& file_path);

        void resize_cache_write_entity(size_t new_size);

        bq::string get_mmap_file_path() const;

        void clean_cache_write();

        bool is_recovery_enabled() const;

        size_t get_total_used_write_cache_size() const {
            return head_size_ + get_cache_write_alignment_offset() + cache_write_cursor_;
        }
    private:
        bq::string config_file_name_;
        bool always_create_new_file_ = false;
        size_t max_file_size_;
        size_t current_file_size_;
        bq::file_handle file_;
        int32_t base_dir_type_ = false;
        uint64_t expire_time_ms_;
        uint64_t capacity_limit_ = 0;
        uint64_t current_file_expire_time_epoch_ms_;
        
    private:
        BQ_PACK_BEGIN
        struct mmap_head {
            uint64_t write_cache_size_;
            uint64_t cache_write_finished_cursor_;
            uint8_t cache_write_alignment_offset_;   
            char padding0_[sizeof(uint32_t) - sizeof(uint8_t)];
            uint32_t file_path_size_;
            char file_path_[1];
            char padding1_[DEFAULT_BUFFER_ALIGNMENT - sizeof(uint64_t) * 3 - sizeof(file_path_)];
        }BQ_PACK_END
        static_assert(sizeof(mmap_head) == DEFAULT_BUFFER_ALIGNMENT, "Invalid appender_file_base::mmap_head size");
        bq::unique_ptr<bq::normal_buffer> cache_write_entity_;
        mmap_head* head_ = nullptr;
        size_t head_size_ = sizeof(mmap_head);
        uint8_t* cache_write_ = nullptr;
        size_t cache_write_cursor_ = 0;
    protected:
        // Cache part
        // Caching can reduce calls to fwrite, enhancing file write performance.
        // Even though fwrite inherently possesses a caching mechanism,
        // the overhead of invoking fwrite itself is not to be overlooked.
#ifndef NDEBUG
        bool cache_write_already_allocated_ = false;
#endif
        bq::array<uint8_t> cache_read_;
        decltype(cache_read_)::size_type cache_read_cursor_ = 0;
        bool cache_read_eof_ = false;

        bq_forceinline size_t get_pendding_flush_written_size() const {
            return head_ ? static_cast<size_t>(head_->cache_write_finished_cursor_) : static_cast<size_t>(0);
        }

        bq_forceinline size_t get_written_size() const {
            return cache_write_cursor_;
        }

        bq_forceinline size_t get_cache_write_size() const {
            return head_ ? static_cast<size_t>(head_->write_cache_size_) : static_cast<size_t>(0);
        }

        bq_forceinline size_t get_cache_write_finished_cursor() const {
            return head_ ? static_cast<size_t>(head_->cache_write_finished_cursor_) : static_cast<size_t>(0);
        }

        bq_forceinline size_t get_cache_write_alignment_offset() const {
            return head_ ? static_cast<size_t>(head_->cache_write_alignment_offset_) : static_cast<size_t>(0);
        }

        bq_forceinline bool is_read_of_cache_eof() const {
            return cache_read_eof_;
        }

        uint8_t* get_cache_write_ptr_base() {
            return cache_write_;
        }

        decltype(cache_read_)::size_type get_cache_read_cursor() const {
            return cache_read_cursor_;
        }
    };
}
