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
 * This is a high-performance asynchronous buffer that balanced between performance and memory usage.
 *
 * HP = High Performance, LP = Low Performance.
 * In fact, LP is only considered lower performance relative to HP;
 * it is still a high-performance multi-producer single-consumer ring buffer.
 * HP has an independent block for each thread, while LP has multiple threads sharing a single ring buffer.
 *
 * \author pippocao
 * \date 2024/12/17
 */
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/miso_linked_list.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"
#include "bq_log/types/buffer/miso_ring_buffer.h"
#include "bq_log/types/buffer/group_list.h"
#include "bq_log/types/buffer/normal_buffer.h"

namespace bq {
    class alignas(BQ_CACHE_LINE_SIZE) log_buffer {
    public:
#if defined(BQ_MOBILE_PLATFORM)
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 2;
#else
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 16;
#endif
#if defined(BQ_UNIT_TEST)
        static constexpr uint16_t MAX_RECOVERY_VERSION_RANGE = 5;
#else
        static constexpr uint16_t MAX_RECOVERY_VERSION_RANGE = 2;
#endif
        static constexpr uint64_t HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL = 1000;
        static constexpr uint64_t OVERSIZE_BUFFER_RECYCLE_INTERVAL_MS = 1000;

    public:
        BQ_PACK_BEGIN
        struct lp_buffer_head_misc {
            uint16_t saved_version_;
        } BQ_PACK_END

            struct destruction_mark {
            bq::platform::spin_lock lock_;
            bool is_destructed_ = false;
        };

        struct oversize_buffer_obj_def {
            bq::normal_buffer buffer_;
            bq::platform::spin_lock_rw_crazy buffer_lock_;
            uint64_t last_used_epoch_ms_;
            oversize_buffer_obj_def(uint32_t size, bool need_recovery, const bq::string& mmap_file_abs_path)
                : buffer_(size, need_recovery, mmap_file_abs_path)
                , last_used_epoch_ms_(0)
            {
            }
        };

        struct alignas(BQ_CACHE_LINE_SIZE) log_tls_buffer_info {
#if defined(BQ_JAVA)
            struct java_info {
                jobjectArray buffer_obj_for_lp_buffer_ = NULL; // miso_ring_buffer shared between low frequency threads;
                jobjectArray buffer_obj_for_hp_buffer_ = NULL; // siso_ring_buffer on block_node;
                jobjectArray buffer_obj_for_oversize_buffer_ = NULL; // oversize buffer;
                block_node_head* buffer_ref_block_ = nullptr;
                normal_buffer* buffer_ref_oversize = nullptr;
                uint32_t size_ref_oversize = 0;
                int32_t buffer_offset_ = 0;
            };
#endif
            uint64_t last_update_epoch_ms_ = 0;
            uint64_t update_times_ = 0;
            block_node_head* cur_block_ = nullptr; // nullptr means using lp_buffer.
            log_buffer* buffer_ = nullptr;
            log_buffer_write_handle oversize_parent_handle_;
            oversize_buffer_obj_def* oversize_target_buffer_;
            bq::shared_ptr<destruction_mark> destruction_mark_;
#if defined(BQ_JAVA)
            java_info java_;
#endif
            // Fields frequently accessed by write(produce) thread.
            alignas(BQ_CACHE_LINE_SIZE) struct {
                uint32_t current_write_seq_ = 0;
            } wt_data_;
            // Fields frequently accessed by read(consumer) thread.
            alignas(BQ_CACHE_LINE_SIZE) struct {
                uint32_t current_read_seq_ = 0;
            } rt_data_;

            ~log_tls_buffer_info();
        };
        static_assert(sizeof(log_tls_buffer_info) % BQ_CACHE_LINE_SIZE == 0, "log_tls_buffer_info current_read_seq_ must be 64 bytes aligned");

        struct log_tls_info {
#if !defined(BQ_LOG_BUFFER_DEBUG)
        private:
#endif
            bq::hash_map_inline<uint64_t, log_tls_buffer_info*>* log_map_ = nullptr;
            uint64_t cur_log_buffer_id_ = 0;
            log_tls_buffer_info* cur_buffer_info_ = nullptr;

        public:
            bq_forceinline log_tls_buffer_info& get_buffer_info(const log_buffer* buffer);
            bq_forceinline log_tls_buffer_info& get_buffer_info_directly(const log_buffer* buffer);
            ~log_tls_info();
        };

        BQ_PACK_BEGIN
        struct alignas(8) pointer_8_bytes_for_32_bits_system {
            log_tls_buffer_info* ptr;
            uintptr_t dummy;
        } BQ_PACK_END

            BQ_PACK_BEGIN struct alignas(8) pointer_8_bytes_for_64_bits_system {
            log_tls_buffer_info* ptr;
        } BQ_PACK_END

            BQ_PACK_BEGIN struct alignas(8) context_head {
        public:
            uint16_t version_;
            bool is_thread_finished_;
            bool is_external_ref_; // only works in lp_buffer when alloc size is larger than buffer size.
            uint32_t seq_;
            bq::condition_type_t<sizeof(void*) == 8, pointer_8_bytes_for_64_bits_system, pointer_8_bytes_for_32_bits_system> tls_info_; // Only meaningful when get_ver() equals the current version of log_buffer.

            bq_forceinline log_tls_buffer_info* get_tls_info() const
            {
                return tls_info_.ptr;
            }
            bq_forceinline void set_tls_info(log_tls_buffer_info* tls_info)
            {
                tls_info_.ptr = tls_info;
            }
        } BQ_PACK_END static_assert(sizeof(context_head) == 16, "context_head size must be 16");
        static_assert(sizeof(context_head) % 8 == 0, "context_head size must be a multiple of 8");

        BQ_PACK_BEGIN
        struct alignas(8) block_misc_data {
            alignas(8) bool is_removed_;
            alignas(8) bool need_reallocate_;
            alignas(8) context_head context_;
    } BQ_PACK_END public : log_buffer(log_buffer_config& config);

        ~log_buffer();

        log_buffer_write_handle alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms);

        void commit_write_chunk(const log_buffer_write_handle& handle);

        log_buffer_read_handle read_chunk();

        void return_read_chunk(const log_buffer_read_handle& handle);
#if defined(BQ_JAVA)
        struct java_buffer_info {
            jobjectArray buffer_array_obj_;
            int32_t* offset_store_;
            const uint8_t* buffer_base_addr_;
        };
        java_buffer_info get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle);
#endif
        bq_forceinline const log_buffer_config& get_config() const
        {
            return config_;
        }

#if defined(BQ_UNIT_TEST)
        const log_tls_buffer_info& get_buffer_info_for_this_thread() const;

        int32_t get_groups_count() const { return hp_buffer_.get_groups_count(); }
        void garbage_collect() { hp_buffer_.garbage_collect(); }
        size_t get_garbage_count() { return hp_buffer_.get_garbage_count(); }
#endif
    private:
        bq::block_node_head* alloc_new_hp_block();

        enum class context_verify_result {
            valid,
            version_pending,
            version_invalid,
            seq_pending,
            seq_invalid,
        };

        enum class read_state {
            lp_buffer_reading,
            hp_block_reading,
            next_block_finding,
            next_group_finding,
            traversal_completed
        };

        bq_forceinline bool is_version_valid(uint16_t version)
        {
            return static_cast<uint16_t>(version_ - version) <= static_cast<uint16_t>(version_ - rt_cache_.current_reading_.version_);
        }

        context_verify_result verify_context(const context_head& context);
        context_verify_result verify_oversize_context(const context_head& parent_context, const context_head& oversize_context);
        void deregister_seq(const context_head& context);
        void prepare_and_fix_recovery_data();
        void clear_recovery_data();

        // For reading thread.
        bool rt_read_from_lp_buffer(log_buffer_read_handle& out_handle);
        bool rt_try_traverse_to_next_block_in_group(context_verify_result& out_verify_result);
        bool rt_try_traverse_to_next_group();
        void rt_try_traverse_to_next_version();
        void refresh_traverse_end_mark();

        // For oversize data.
        log_buffer_write_handle wt_alloc_oversize_write_chunk(uint32_t size, uint64_t current_epoch_ms);
        void wt_commit_oversize_write_chunk(const log_buffer_write_handle& oversize_handle);
        bool rt_read_oversize_chunk(const log_buffer_read_handle& parent_handle, log_buffer_read_handle& out_oversize_handle);
        void rt_return_oversize_read_chunk(const log_buffer_read_handle& oversize_handle);
        void rt_recycle_oversize_buffers();

    private:
        friend struct log_tls_info;
        log_buffer_config config_;
        uint64_t id_; // unique id for log_buffer
        group_list hp_buffer_; // high performance buffer, each thread has its own block, but more memory usage.
        miso_ring_buffer lp_buffer_; // used to save memory for low frequency threads.
        const uint16_t version_ = 0;
        bq::shared_ptr<destruction_mark> destruction_mark_;

        struct alignas(BQ_CACHE_LINE_SIZE) {
            bq::platform::spin_lock_rw_crazy array_lock_;
            bq::array<bq::unique_ptr<oversize_buffer_obj_def>> buffers_array_;
#if defined(BQ_JAVA)
            jobject java_buffer_obj_ = nullptr;
#endif
        } temprorary_oversize_buffer_; // used when allocating a large chunk of data that exceeds the size of lp_buffer or hp_buffer.
        bq::platform::atomic<uint64_t> current_oversize_buffer_index_;

        struct alignas(BQ_CACHE_LINE_SIZE) {
            struct {
                group_list::iterator last_group_; // empty means read from lp_buffer
                group_list::iterator cur_group_;
                block_node_head* last_block_ = nullptr;
                block_node_head* cur_block_ = nullptr;
                uint16_t version_ = 0;
                bq::array<bq::hash_map<void*, uint32_t>> recovery_records_; // <tls_buffer_info_ptr, seq> for each version, only works when reading recovering data
#ifdef BQ_UNIT_TEST
                bq::array<bq::hash_map<void*, bq::hash_map<uint32_t, uint16_t>>> recovery_seq_records_;
#endif
                read_state state_ = read_state::lp_buffer_reading;
                bool traverse_end_block_is_working_ = false;
                block_node_head* traverse_end_block_ = nullptr;
                siso_ring_buffer::siso_buffer_batch_read_handle hp_handle_cache_;
            } current_reading_;

            // memory fragmentation optimize
            struct {
                uint32_t left_holes_num_ = 0;
                uint16_t cur_group_using_blocks_num_ = 0;
                bool is_block_marked_removed = false;
                context_verify_result verify_result;
            } mem_optimize_;
        } rt_cache_; // Cache that only access in read(consumer) thread.
#if defined(BQ_LOG_BUFFER_DEBUG)
        alignas(BQ_CACHE_LINE_SIZE) bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
#endif
    };

    bq_forceinline log_buffer::log_tls_buffer_info& log_buffer::log_tls_info::get_buffer_info(const log_buffer* buffer)
    {
        if (buffer->id_ == cur_log_buffer_id_) {
            return *cur_buffer_info_;
        }
        if (!log_map_) {
            log_map_ = new bq::hash_map_inline<uint64_t, log_tls_buffer_info*>();
            log_map_->set_expand_rate(4);
        }
        cur_log_buffer_id_ = buffer->id_;
        auto iter = log_map_->find(buffer->id_);
        if (iter == log_map_->end()) {
            iter = log_map_->add(buffer->id_, bq::util::aligned_new<log_tls_buffer_info>(BQ_CACHE_LINE_SIZE));
            iter->value()->destruction_mark_ = buffer->destruction_mark_;
            iter->value()->buffer_ = const_cast<log_buffer*>(buffer);
        }
        cur_buffer_info_ = iter->value();
        return *cur_buffer_info_;
    }

    bq_forceinline log_buffer::log_tls_buffer_info& log_buffer::log_tls_info::get_buffer_info_directly(const log_buffer* buffer)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(buffer->id_ == cur_log_buffer_id_ && "log_buffer::alloc and log_buffer::commit must use in pair");
#endif
        (void)buffer;
        return *cur_buffer_info_;
    }
}