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
#include "bq_log/types/buffer/miso_ring_buffer.h"
#include "bq_log/types/buffer/group_list.h"

namespace bq {
    class alignas(CACHE_LINE_SIZE) log_buffer {
    public:
#if BQ_ANDROID || BQ_IOS
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 8;
#else
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 16;
#endif
        static constexpr uint16_t MAX_RECOVERY_VERSION_RANGE = 10;
        static constexpr uint64_t HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_ = 1000; 
    public:
        BQ_PACK_BEGIN
        struct lp_buffer_head_misc {
            uint16_t saved_version_;
        } 
        BQ_PACK_END

        struct destruction_mark {
            bq::platform::spin_lock lock_;
            bool is_destructed_ = false;
        };

        struct alignas(CACHE_LINE_SIZE) log_tls_buffer_info {
            uint64_t last_update_epoch_ms_ = 0;
            uint64_t update_times_ = 0;
            block_node_head* cur_block_ = nullptr; // nullptr means using lp_buffer.
            log_buffer* buffer_ = nullptr;
            bq::shared_ptr<destruction_mark> destruction_mark_;
#if BQ_JAVA
            jobjectArray buffer_obj_for_lp_buffer_ = NULL; // miso_ring_buffer shared between low frequency threads;
            jobjectArray buffer_obj_for_hp_buffer_ = NULL; // siso_ring_buffer on block_node;
            block_node_head* buffer_ref_block_ = nullptr;
            int32_t buffer_offset_ = 0;
#endif
            // Fields frequently accessed by write(produce) thread.
            alignas(CACHE_LINE_SIZE) struct {
                uint32_t current_write_seq_ = 0;
            } wt_data_;
            // Fields frequently accessed by read(consumer) thread.
            alignas(CACHE_LINE_SIZE) struct {
                uint32_t current_read_seq_ = 0;
            } rt_data_;

            ~log_tls_buffer_info();
        };
        static_assert(sizeof(log_tls_buffer_info) % CACHE_LINE_SIZE == 0, "log_tls_buffer_info current_read_seq_ must be 64 bytes aligned");

        struct log_tls_info {
#if !BQ_LOG_BUFFER_DEBUG
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

        BQ_PACK_BEGIN 
        struct alignas(8) pointer_8_bytes_for_64_bits_system {
            log_tls_buffer_info* ptr;
        } BQ_PACK_END

        BQ_PACK_BEGIN 
        struct alignas(8) context_head {
        public:
            uint16_t version_;
            bool is_thread_finished_;
            char padding_;
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
        } BQ_PACK_END 
        static_assert(sizeof(context_head) == 16, "context_head size must be 16");
        static_assert(sizeof(context_head) % 8 == 0, "context_head size must be a multiple of 8");

        BQ_PACK_BEGIN
        struct alignas(8) block_misc_data {
            alignas(8) bool is_removed_;
            alignas(8) bool need_reallocate_;
            alignas(8) context_head context_;
        } BQ_PACK_END
    public:
        log_buffer(log_buffer_config& config);

        ~log_buffer();

        log_buffer_write_handle alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms);

        void commit_write_chunk(const log_buffer_write_handle& handle);

        log_buffer_read_handle read_chunk();

        void return_read_chunk(const log_buffer_read_handle& handle);
#if BQ_JAVA
        struct java_buffer_info {
            jobjectArray buffer_array_obj_;
            int32_t* offset_store_;
            const uint8_t* buffer_base_addr_;
        };
        java_buffer_info get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle);
#endif

#if BQ_UNIT_TEST
        uint32_t get_groups_count() const { return hp_buffer_.get_groups_count();}
        void garbage_collect() { hp_buffer_.garbage_collect(); }
        size_t get_garbage_count() { return hp_buffer_.get_garbage_count(); }
#endif
    private:
        bq::block_node_head* alloc_new_hp_block();

        enum class context_verify_result {
            valid,
            version_waiting,
            version_invalid,
            seq_pending,
        };

        enum class read_state {
            init,
            lp_buffer_reading,
            hp_block_reading,
            next_block_finding,
            next_group_finding,
            traversal_completed
        };

        context_verify_result verify_context(const context_head& context);
        void deregister_seq(const context_head& context);
        void prepare_and_fix_recovery_data();

        //For reading thread.
        bool rt_read_from_lp_buffer(log_buffer_read_handle& out_handle);
        bool rt_try_traverse_to_next_block_in_group(context_verify_result& out_verify_result);
        bool rt_try_traverse_to_next_group();
    private:
        friend struct log_tls_info;
        log_buffer_config config_;
        uint64_t id_; // unique id for log_buffer
        group_list hp_buffer_; // high performance buffer, each thread has its own block, but more memory usage.
        miso_ring_buffer lp_buffer_; // used to save memory for low frequency threads.
        const uint16_t version_ = 0;
        bq::shared_ptr<destruction_mark> destruction_mark_;
        struct alignas(CACHE_LINE_SIZE) {
            struct {
                group_list::iterator last_group_;     //empty means read from lp_buffer
                group_list::iterator cur_group_;
                block_node_head* last_block_ = nullptr;
                block_node_head* cur_block_ = nullptr;
                uint16_t version_ = 0;
                bq::array<bq::hash_map<void*, uint32_t>> recovery_records_; // <tls_buffer_info_ptr, seq> for each version, only works when reading recovering data    
                read_state state_ = read_state::init;
                block_node_head* traverse_end_block_ = nullptr;
            } current_reading_;

            // memory fragmentation optimize
            struct {
                uint32_t left_holes_num_ = 0;
                uint16_t cur_group_using_blocks_num_ = 0;
                bool is_block_marked_removed = false;
                context_verify_result verify_result;
            } mem_optimize_;
        } rt_cache_; // Cache that only access in read(consumer) thread.

#if BQ_LOG_BUFFER_DEBUG
        alignas(CACHE_LINE_SIZE) bq::platform::thread::thread_id empty_thread_id_ = 0;
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
            iter = log_map_->add(buffer->id_, new log_tls_buffer_info());
            iter->value()->destruction_mark_ = buffer->destruction_mark_;
            iter->value()->buffer_ = const_cast<log_buffer*>(buffer);
        }
        cur_buffer_info_ = iter->value();
        return *cur_buffer_info_;
    }

    bq_forceinline log_buffer::log_tls_buffer_info& log_buffer::log_tls_info::get_buffer_info_directly(const log_buffer* buffer)
    {
#if BQ_LOG_BUFFER_DEBUG
        assert(buffer->id_ == cur_log_buffer_id_ && "log_buffer::alloc and log_buffer::commit must use in pair");
#endif
        (void)buffer;
        return *cur_buffer_info_;
    }
}