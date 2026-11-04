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
        // The alignment of the write/read caches and of the recovery mmap header. It was inherited
        // from the XOR cipher's SIMD buffer, which has left the runtime; the value cannot change
        // because the mmap header's size is part of the on-disk recovery file layout.
        static constexpr size_t DEFAULT_BUFFER_ALIGNMENT = 32;
        static_assert((DEFAULT_BUFFER_ALIGNMENT & (DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1))) == 0, "DEFAULT_BUFFER_ALIGNMENT must be power of two");

        // The only question the flush state word answers: is the write cache plaintext right now?
        // It records no position and no written length on purpose - recovery neither replays into the
        // old place nor repairs the file, so there is nothing to record (RD §2.4.3).
        enum flush_state : uint32_t {
            flush_idle = 0, // plaintext, no flush window in progress
            flush_transforming = 1, // a window is open; the contents are not interpretable
            // 2 and 3 are never produced by this version and are read as transforming.
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
        // Test-only. The crash-recovery tests need the exact on-disk state that exists at a given
        // instant of a flush, which a real kill -9 cannot reproduce repeatably. A hook that fires at
        // that instant and copies the appender files gives a deterministic snapshot instead.
        enum class flush_hook_point : int32_t {
            after_transform = 0, // buffer encrypted in place, write() not issued yet
            after_write = 1, // write() returned, cursors and file size not updated yet
        };
        // The sender identifies which appender is flushing. The hook is process-global, so without it
        // a test that snapshots one appender would also fire on an unrelated appender's flush and
        // capture its files at an arbitrary instant.
        typedef void (*flush_hook_fn)(const appender_file_base* sender, flush_hook_point point, void* user);

        static void set_flush_hook(flush_hook_fn fn, void* user);

        // Test-only. The flush state word is only reachable through the recovery mapping, so a test
        // that wants to look at it needs to know where it sits. mmap_head is private and stays that
        // way; the offset is all a test can meaningfully use.
        static size_t test_flush_state_offset();

        // Test-only. A producer that appends while a flush is in flight leaves bytes past the
        // finished cursor, and the flush has to carry them along with the bytes it could not write.
        // Writing that tail from a flush hook is the only way to reproduce the mix repeatably.
        void test_append_write_cache_tail(const void* data, size_t size);

        // Test-only. The flush state word as this appender sees it, for a test that holds a pointer to
        // it - a flush hook does - but cannot call the protected reader. Opening the mapping file from
        // outside is not a substitute: read() of a live shared mapping only sees what the platform
        // keeps coherent with it, and that differs between systems.
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

        // In-place XOR of [data, data + size) as it would sit at absolute file position file_pos.
        // The base class owns the window; a derived class only says whether it wants one at all
        // (transform_enabled_) and what the XOR is. Both directions are the same call, which is what
        // lets the flush revert the part that write() did not take.
        virtual void on_flush_transform(uint8_t* data, size_t size, uint64_t file_pos);

        // Last chance to refuse a window while the buffer is still plaintext. Returning false
        // abandons this flush and leaves the pending bytes in memory; the implementer is expected to
        // have invalidated the file handle so the next entry starts a new one.
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

        void set_cache_write_padding(uint8_t new_padding);

        virtual bool on_appender_file_recovery_begin();

        virtual void on_appender_file_recovery_end();

        void set_flush_when_destruct(bool flush);

        // Derived classes flush from their own destructor, where the override is still reachable,
        // and must ask whether the caller wanted that at all (recovery tests turn it off on purpose).
        bool get_flush_when_destruct() const;

        void open_new_indexed_file_by_name();
#ifdef BQ_UNIT_TEST
    protected:
#else
    private:
#endif
        void set_basic_configs(const bq::property_value& config_obj);

        void refresh_cache_write_head_size(bool need_recovery, const bq::string& mmap_file_abs_path);

        // Drives the flush window in RD §2.4.2. set_flush_state_transforming() opens it and
        // set_flush_state_idle() closes it; both are no-ops for a plaintext appender, which never
        // touches the word. The barrier in the first and the release in the second are what make the
        // ordering against the transform and the accounting hold.
        bq_forceinline bool is_flush_window_needed() const { return transform_enabled_; }

        void set_flush_state_transforming();

        void set_flush_state_idle();

        // True when the buffer was left mid-window by a previous process. Values this version never
        // writes are treated as transforming: an unknown state is not evidence of plaintext.
        bool read_flush_state_is_transforming() const;

        void clear_flush_state();

        bool try_recover();

        // Every failed recovery exits through this, so the appender is left exactly as it was before
        // the attempt. Callers of try_recover() must not have to reason about which failure happened.
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
            // Whether the buffer is in the middle of a flush window. Recovery only needs to know
            // whether the buffer is plaintext or not interpretable; it records no position and no
            // written length, because a crash inside the window is handled by discarding it.
            uint32_t flush_state_;
            uint32_t file_path_size_;
            char file_path_[1];
            char padding1_[DEFAULT_BUFFER_ALIGNMENT - sizeof(uint64_t) * 3 - sizeof(file_path_)];
        } BQ_PACK_END
        static_assert(sizeof(mmap_head) == DEFAULT_BUFFER_ALIGNMENT, "Invalid appender_file_base::mmap_head size");
        // The state word reuses the four bytes that were reserved here before v11, so the head keeps
        // its size and every other offset stays where older processes left them.
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

        // Non-virtual on purpose: the plaintext path costs one load and one branch per flush, not an
        // indirect call, and that path is the one that must not regress.
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
