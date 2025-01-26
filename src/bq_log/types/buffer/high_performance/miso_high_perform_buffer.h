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
 * \class bq::miso_high_perform_buffer
 *
 * \author pippocao
 * \date 2024/12/17
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_types.h"
#include "bq_log/types/buffer/high_performance/group_list.h"
#include "bq_log/types/buffer/normal/miso_ring_buffer.h"

namespace bq {
    class miso_high_perform_buffer {
        static constexpr uint64_t HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_ = 1000;
        static constexpr size_t HP_BUFFER_CALL_FREQUENCY_CHECK_THRESHOLD_ = 1000; // 1000 times in HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_.

        static constexpr uint64_t THREAD_ALIVE_UPDATE_INTERVAL = 200;
        static constexpr uint64_t BLOCK_THREAD_VALID_CHECK_THRESHOLD = THREAD_ALIVE_UPDATE_INTERVAL * 2;
    public:
        miso_high_perform_buffer(log_buffer_config& config);

        ~miso_high_perform_buffer();

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
        java_buffer_info get_java_buffer_info(JavaVM* jvm, const log_buffer_write_handle& handle);
#endif
    private:
        block_node_head* alloc_new_high_performance_block();

        void optimize_memory_begin(uint64_t current_epoch_ms);
        void optimize_memory_for_group(const group_list::iterator& next_group, uint64_t current_epoch_ms);
        void optimize_memory_for_block(block_node_head* next_block, uint64_t current_epoch_ms);
        void optimize_memory_end();

    private:
        log_buffer_config config_;
        group_list high_perform_buffer_;
        miso_ring_buffer normal_buffer_; //used to save memory for low frequency threads.
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
        } rt_cache_;   //Cache that only access in read(consumer) thread.
#if BQ_LOG_BUFFER_DEBUG
        char padding_[CACHE_LINE_SIZE];
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
#endif
    };
}
