/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_types.h"
#include "bq_log/types/buffer/normal_buffer.h"

namespace bq {
    class appender_file_base : public appender_base {
        friend class appender_decoder_base;

    protected:
        // Part of the recovery mmap layout through sizeof(mmap_head).
        static constexpr size_t DEFAULT_BUFFER_ALIGNMENT = 32;
        static_assert((DEFAULT_BUFFER_ALIGNMENT & (DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1))) == 0, "DEFAULT_BUFFER_ALIGNMENT must be power of two");

        // Whether the write cache is plaintext. Any value other than flush_idle reads as transforming.
        enum flush_state : uint32_t {
            flush_idle = 0,
            flush_transforming = 1,
        };

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
        virtual void flush_write_cache();
        // flush appender file to physical disk.
        void flush_write_io();

#if defined(BQ_UNIT_TEST)
        // Test-only: lets crash tests snapshot the files at a fixed instant of a flush.
        enum class flush_hook_point : int32_t {
            after_transform = 0, // buffer encrypted in place, write() not issued yet
            after_write = 1, // write() returned, cursors and file size not updated yet
        };
        typedef void (*flush_hook_fn)(const appender_file_base* sender, flush_hook_point point, void* user);

        static void set_flush_hook(flush_hook_fn fn, void* user);

        static size_t test_flush_state_offset();

        // Appends bytes past the finished cursor, as a producer does while a flush is in flight.
        void test_append_write_cache_tail(const void* data, size_t size);

        uint32_t test_read_flush_state() const;
#endif

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;

#if defined(BQ_UNIT_TEST)
        void invoke_flush_hook(flush_hook_point point) const;
#endif

        virtual bool reset_impl(const bq::property_value& config_obj) override;

        // Hot path: disk healthy, branch never taken. When the previous flush hit ENOSPC, entries are dropped at the
        // door so the in-memory cache cannot grow without bound; what is already cached stays and is retried.
        virtual bool log_impl(const log_entry_handle& handle) override
        {
            BQ_UNLIKELY_IF(should_drop_due_to_io_failure())
            {
                return false;
            }
            return refresh_file_handle(handle);
        }

        bq_forceinline bool should_drop_due_to_io_failure() const { return disk_full_drop_; }

        virtual bool parse_exist_log_file(parse_file_context& context) = 0;

        virtual bq::string get_file_ext_name() = 0;

        virtual void on_file_open(bool is_new_created);

        // In-place, self-inverse transform of bytes that will sit at file_pos. Only called when transform_enabled_.
        virtual void on_flush_transform(uint8_t* data, size_t size, uint64_t file_pos);

        // Returning false skips this flush and keeps the bytes pending.
        virtual bool on_flush_before_transform(size_t pending_size, uint64_t file_pos);

        virtual bool seek_read_file_absolute(uint64_t pos);

        virtual void seek_read_file_offset(int32_t offset);

        virtual bool on_log_item_recovery_begin(bq::log_entry_handle& read_handle) override;

        virtual void on_log_item_recovery_end() override;

        virtual bool on_log_item_new_begin(bq::log_entry_handle& read_handle) override;

        uint64_t get_current_file_size() const { return current_file_size_; }

        file_handle& get_file_handle() { return file_; }

        // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
        virtual read_with_cache_handle read_with_cache(size_t size);

        void clear_read_cache();

        bq_forceinline write_with_cache_handle alloc_write_cache(size_t size)
        {
#ifndef NDEBUG
            assert(!cache_write_already_allocated_ && "duplicate call alloc_write_cache in log file appender");
            cache_write_already_allocated_ = true;
#endif
            BQ_UNLIKELY_IF(cache_write_cursor_ + static_cast<uint64_t>(size) > get_cache_write_size())
            {
                make_room_in_write_cache(size);
            }
            write_with_cache_handle result_handle;
            result_handle.data_ = cache_write_ + static_cast<ptrdiff_t>(cache_write_cursor_);
            result_handle.alloc_len_ = size;
            result_handle.used_len_ = size;
            return result_handle;
        }

        bq_forceinline void return_write_cache(const write_with_cache_handle& handle)
        {
#ifndef NDEBUG
            assert(cache_write_already_allocated_ && "call return_write_cache without calling alloc_write_cache in log file appender");
            assert(handle.used_len_ <= handle.alloc_len_ && "used data length greater than allocated length in log file appender");
            cache_write_already_allocated_ = false;
#endif
            cache_write_cursor_ += static_cast<uint64_t>(handle.used_len_);
        }

        // Low performance function, try to use write cache first
        size_t direct_write(const void* data, size_t size, bq::file_manager::seek_option seek_opt, int64_t seek_offset);

        bq_forceinline void mark_write_finished()
        {
#ifndef NDEBUG
            assert(!cache_write_already_allocated_ && "mark_write_finished must be called after return_write_cache()");
#endif
            cache_write_head_->cache_write_finished_cursor_ = cache_write_cursor_;
        }

        virtual bool on_appender_file_recovery_begin();

        virtual void on_appender_file_recovery_end();

        void set_flush_when_destruct(bool flush);

        bool get_flush_when_destruct() const;

        void open_new_indexed_file_by_name();
#ifdef BQ_UNIT_TEST
    protected:
#else
    private:
#endif
        void set_basic_configs(const bq::property_value& config_obj);

        void refresh_cache_write_head_size(bool need_recovery, const bq::string& mmap_file_abs_path);

        void set_flush_state_transforming();

        void set_flush_state_idle();

        bool read_flush_state_is_transforming() const;

        void clear_flush_state();

        bool try_recover();

        // Every failed recovery exits through this and leaves the appender as before the attempt.
        bool discard_recovery_cache();

        bq_forceinline bool is_file_oversize() const
        {
            return max_file_size_ > 0 && current_file_size_ >= max_file_size_;
        }

        void clear_all_expired_files(); // retention limit

        void clear_all_limit_files(); // capacity limit

        bq_forceinline bool refresh_file_handle(const log_entry_handle& handle)
        {
            BQ_LIKELY_IF(file_ && !is_file_oversize()
                && (!enable_rolling_log_file_ || handle.get_log_head().timestamp_epoch <= current_file_expire_time_epoch_ms_))
            {
                return true;
            }
            return open_file_for_entry(handle);
        }

        bool open_file_for_entry(const log_entry_handle& handle);

        void make_room_in_write_cache(size_t size);

        bool open_file_with_write_exclusive(const bq::string& file_path);

        void resize_cache_write_entity(size_t new_size);

        void refresh_cache_write_ptr();

        bq::string get_mmap_file_path() const;

        void clean_cache_write();

        bool is_recovery_enabled() const;

        bq_forceinline size_t get_total_used_write_cache_size() const
        {
            return cache_write_head_size_ + static_cast<size_t>(cache_write_padding_) + cache_write_cursor_;
        }

    private:
        bq::string config_file_name_;
        bool always_create_new_file_;
        uint64_t max_file_size_;
        uint64_t current_file_size_;
        bq::file_handle file_;
        int32_t base_dir_type_;
        bool enable_rolling_log_file_;
        uint64_t expire_time_ms_;
        uint64_t capacity_limit_;
        size_t write_cache_target_size_;
        uint64_t current_file_expire_time_epoch_ms_;
        bool flush_when_destruct_ = true;
        bool disk_full_drop_ = false;

    private:
        BQ_PACK_BEGIN
        struct mmap_head {
            uint64_t write_cache_size_;
            uint64_t cache_write_finished_cursor_;
            uint32_t flush_state_;
            uint32_t file_path_size_;
            char file_path_[1];
            char padding1_[DEFAULT_BUFFER_ALIGNMENT - sizeof(uint64_t) * 3 - sizeof(file_path_)];
        } BQ_PACK_END
        static_assert(sizeof(mmap_head) == DEFAULT_BUFFER_ALIGNMENT, "Invalid appender_file_base::mmap_head size");
        static_assert(offsetof(mmap_head, flush_state_) == sizeof(uint64_t) * 2, "flush state moved out of the reserved slot");
        static_assert(offsetof(mmap_head, file_path_size_) == sizeof(uint64_t) * 2 + sizeof(uint32_t), "file path size moved");
        bq::unique_ptr<bq::normal_buffer> cache_write_entity_;
        mmap_head* cache_write_head_ = nullptr;
        size_t cache_write_head_size_ = sizeof(mmap_head);
        uint8_t* cache_write_ = nullptr;
        size_t cache_write_cursor_ = 0;
        uint8_t cache_write_padding_ = 0;

    protected:
        // Cache part
        // Caching can reduce calls to fwrite, enhancing file write performance.
        // Even though fwrite inherently possesses a caching mechanism,
        // the overhead of invoking fwrite itself is not to be overlooked.
#ifndef NDEBUG
        bool cache_write_already_allocated_ = false;
#endif
        bq::array<uint8_t> cache_read_;
        size_t cache_read_cursor_ = 0;
        uint64_t read_file_pos_ = 0;
        bool cache_read_eof_ = false;

        // A flag rather than a virtual call, so a plaintext flush pays one branch.
        bool transform_enabled_ = false;

        bq_forceinline size_t get_pendding_flush_written_size() const
        {
            return cache_write_head_ ? static_cast<size_t>(cache_write_head_->cache_write_finished_cursor_) : static_cast<size_t>(0);
        }

        bq_forceinline size_t get_written_size() const
        {
            return cache_write_cursor_;
        }

        bq_forceinline size_t get_cache_write_size() const
        {
            return cache_write_head_ ? static_cast<size_t>(cache_write_head_->write_cache_size_) : static_cast<size_t>(0);
        }

        bq_forceinline size_t get_cache_write_finished_cursor() const
        {
            return cache_write_head_ ? static_cast<size_t>(cache_write_head_->cache_write_finished_cursor_) : static_cast<size_t>(0);
        }

        bq_forceinline bool is_read_of_cache_eof() const
        {
            return cache_read_eof_;
        }

        bq_forceinline size_t get_cache_write_head_size() const
        {
            return cache_write_head_size_;
        }

        bq_forceinline uint8_t* get_cache_write_ptr_base()
        {
            return cache_write_;
        }

        bq_forceinline size_t get_cache_read_cursor() const
        {
            return cache_read_cursor_;
        }

        bq_forceinline uint64_t get_read_file_pos() const
        {
            return read_file_pos_;
        }
    };
}
