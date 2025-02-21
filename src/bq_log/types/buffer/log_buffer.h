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
    class log_buffer {
    public:
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 16;

        static constexpr uint64_t HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_ = 1000; 
        static constexpr size_t HP_BUFFER_CALL_FREQUENCY_CHECK_THRESHOLD_ = 1000; // 1000 times in HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_.
        
#if BQ_WIN
        static constexpr uint64_t THREAD_ALIVE_UPDATE_INTERVAL = 200;
        static constexpr uint64_t BLOCK_THREAD_VALID_CHECK_THRESHOLD = THREAD_ALIVE_UPDATE_INTERVAL * 2;
#endif

    public:
        log_buffer(log_buffer_config& config);

        ~log_buffer();

        log_buffer_write_handle alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms);

        void commit_write_chunk(const log_buffer_write_handle& handle);

        /// <summary>
        ///
        /// </summary>
        /// <param name="current_epoch_ms"></param>
        /// <returns></returns>
        log_buffer_read_handle read_chunk(uint64_t current_epoch_ms);

        void return_read_trunk(const log_buffer_read_handle& handle);

        /// <summary>
        /// Warning:ring buffer can only be read from one thread at same time.
        /// This option is only work in Debug build and will be ignored in Release build.
        /// </summary>
        /// <param name="in_enable"></param>
        void set_thread_check_enable(bool in_enable);

#if BQ_JAVA
        struct java_buffer_info {
            jobjectArray buffer_array_obj_;
            int32_t* offset_store_;
            const uint8_t* buffer_base_addr_;
        };
        java_buffer_info get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle);
#endif
    private:
        void optimize_memory_begin(uint64_t current_epoch_ms);
        void optimize_memory_for_group(const group_list::iterator& group, uint64_t current_epoch_ms);
        void optimize_memory_for_block(block_node_head* block, uint64_t current_epoch_ms);
        void optimize_memory_end();

    private:
        log_buffer_config config_;
        group_list high_perform_buffer_;
        miso_ring_buffer lp_buffer_; // used to save memory for low frequency threads.
        struct {
            struct {
                group_list::iterator group_;
                block_node_head* block_ = nullptr;
            } current_reading_;

            // memory fragmentation optimize
            struct {
                group_list::iterator last_group_;
                block_node_head* last_block_ = nullptr;
                uint32_t left_holes_num_ = 0;
                uint16_t cur_group_using_blocks_num_ = 0;
                bool is_block_marked_removed = false;
            } mem_optimize_;
        } rt_cache_; // Cache that only access in read(consumer) thread.
#if BQ_LOG_BUFFER_DEBUG
        char padding_[CACHE_LINE_SIZE];
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
#endif
    };
}