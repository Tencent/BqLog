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
 * \class bq::siso_ring_buffer
 *
 * `siso`("single thread in and single thread out").
 * 
 * This high-performance ring buffer supports single-threaded writes and single-threaded reads simultaneously.
 * The buffer size is determined during initialization and cannot be changed.
 * Data in the ring buffer chunks are stored contiguously.
 * The implementation is designed for extremely high concurrent performance and is optimized to be CPU cache friendly.
 *
 * Usage:
 * Producer Thread (Single Thread): Loop through the sequence of the following steps:
 *	1. `alloc_write_chunk`
 *	2. Copy data to the chunk if allocation is successful
 *	3. `commit_write_chunk` (regardless of allocation success or failure, the allocated chunk won't be marked as ready for consumption until `end_read` is called)
 * Consumer Thread (Single Thread): Loop through the sequence of the following steps:
 *	1. `read_chunk`
 *	2. Read data from chunk if #1 returns successful.
 *	3. `return_read_trunk` (the data read won't be marked as consumed until `return_read_trunk` is called; once marked as consumed, the memory is considered released and becomes available for the producer to write new data)
 *
 * \author pippocao
 * \date 2024/11/19
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {

    class siso_ring_buffer {
    private:
        static_assert(CACHE_LINE_SIZE >> CACHE_LINE_SIZE_LOG2 == 1, "invalid cache line size information");

        union block {
        public:
            BQ_PACK_BEGIN
            struct alignas(4) chunk_head_def { //alignas(4) can make sure compiler generate more effective code when access int fields
                uint32_t block_num;
                uint32_t data_size;
                uint8_t data[1];
                uint8_t padding[3];
            }
            BQ_PACK_END
        public:
            chunk_head_def chunk_head;
        private:
            uint8_t data[CACHE_LINE_SIZE];
        };


        BQ_PACK_BEGIN
        struct alignas(4) head
        {
            // and it is a snapshot of last read cursor when recovering from memory map file .
            uint32_t read_cursor_cache_;
            uint32_t write_cursor_cache_;
            uint32_t aligned_blocks_count_cache_;
        }
        BQ_PACK_END

        static_assert(sizeof(block) == CACHE_LINE_SIZE, "the size of block should be equal to cache line size");
        const ptrdiff_t data_block_offset = reinterpret_cast<ptrdiff_t>(((block*)0)->chunk_head.data);
        static_assert(sizeof(block::chunk_head) < sizeof(block), "the size of chunk_head should be less than that of block");


        BQ_PACK_BEGIN
        struct alignas(4) cursors_set
        {
            uint32_t write_cursor_;
            char cache_line_padding0_[CACHE_LINE_SIZE - sizeof(write_cursor_)];
            uint32_t read_cursor_;
            char cache_line_padding1_[CACHE_LINE_SIZE - sizeof(read_cursor_)];

            // these cache variables used in reading thread are used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            // rt is short name of "read thread", which means this variable is accessed in read thread.
            uint32_t rt_writing_cursor_cache_;
            char cache_line_padding2_[CACHE_LINE_SIZE - sizeof(rt_writing_cursor_cache_)];
            // this cache variable used in writing thread is used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            // wt is short name of "write thread", which means this variable is accessed in write thread.
            uint32_t wt_reading_cursor_cache_;  
            char cache_line_padding3_[CACHE_LINE_SIZE - sizeof(wt_reading_cursor_cache_)];
        }
        BQ_PACK_END
        static_assert(sizeof(cursors_set) % CACHE_LINE_SIZE == 0, "invalid cursors_set size");

        char cursors_storage_[sizeof(cursors_set) + CACHE_LINE_SIZE];
        cursors_set* cursors_;   //make sure it is aligned to cache line size even in placement new.

        head* head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bool is_memory_mapped_;

#if BQ_LOG_BUFFER_DEBUG
        char padding_[CACHE_LINE_SIZE];
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id write_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
        bq::platform::atomic<uint32_t> result_code_statistics_[(int32_t)enum_buffer_result_code::result_code_count];
        bq::platform::atomic<uint64_t> total_write_bytes_;
        bq::platform::atomic<uint64_t> total_read_bytes_;
#endif
    public:
        siso_ring_buffer(void* buffer, size_t buffer_size, bool is_memory_mapped);

        siso_ring_buffer(const siso_ring_buffer& rhs) = delete;

        /// de-constuctor;
        ~siso_ring_buffer();

        /// <summary>
        /// A producer can request a block of memory by calling alloc_write_chunk for writing data,
        /// but the prerequisite is that the result of the returned log_buffer_write_handle must be success.
        /// </summary>
        /// <param name="size">the chunk size you desired</param>
        /// <returns>this will only be valid if result code is enum_buffer_result_code::success</returns>
        log_buffer_write_handle alloc_write_chunk(uint32_t size);

        /// <summary>
        /// After you have requested a memory block by calling alloc_write_chunk,
        /// you need to invoke commit_write_chunk for the returned `log_buffer_write_handle`,
        /// regardless of whether the allocation was successful or not.
        /// </summary>
        /// <param name="handle">the handle you want to commit</param>
        void commit_write_chunk(const log_buffer_write_handle& handle);

        /// <summary>
        /// To read a block of memory prepared by the producer,
        /// </summary>
        /// <returns>data only be valid if result code is enum_buffer_result_code::success</returns>
        log_buffer_read_handle read_chunk();

        /// <summary>
        /// the chunk date read by `read_chunk` won't be marked as consumed until `return_read_trunk` is called;
        /// once marked as consumed, the memory is considered released and becomes
        /// available for the producer to call `alloc_write_chunk` to alloc and write new data
        /// </summary>
        /// <param name=""></param>
        void return_read_trunk(const log_buffer_read_handle& handle);

        /// <summary>
        /// Traverses all data written, invoking the provided callback for each chunk from the consumer thread.
        /// </summary>
        /// <param name="in_callback">A function pointer to the callback that will be called with the data and its size.</param>
        /// <param name="in_user_data">user defined data passed to in_callback.</param>
        void data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data);

        /// <summary>
        /// Calculates the minimum buffer size required to ensure the final buffer 
        /// has the desired memory available when passed to the constructor.
        /// </summary>
        /// <param name="expected_buffer_size">The desired memory size to be guaranteed</param>
        /// <returns>The minimum buffer size required</returns>
        static uint32_t calculate_min_size_of_memory(uint32_t expected_buffer_size);

        /// <summary>
        /// Warning:ring buffer can only be read from one thread at same time.
        /// This option is only work in Debug build and will be ignored in Release build.
        /// </summary>
        /// <param name="in_enable"></param>
        void set_thread_check_enable(bool in_enable)
        {
#if BQ_LOG_BUFFER_DEBUG
            check_thread_ = in_enable;
            if (!check_thread_) {
                read_thread_id_ = empty_thread_id_;
                write_thread_id_ = empty_thread_id_;
            }
#else
            (void)in_enable;
#endif
        }

        bq_forceinline const uint8_t* get_buffer_addr() const
        {
            return (uint8_t*)aligned_blocks_;
        }

        bq_forceinline uint32_t get_block_size() const
        {
            return sizeof(block);
        }

        bq_forceinline uint32_t get_total_blocks_count() const
        {
            return aligned_blocks_count_;
        }

        bq_forceinline bool get_is_memory_mapped() const
        {
            return is_memory_mapped_;
        }

    private:
        bq_forceinline block& cursor_to_block(uint32_t cursor)
        {
            return aligned_blocks_[cursor & (aligned_blocks_count_ - 1)];
        }

        bool try_recover_from_exist_memory_map(void* buffer, size_t buffer_size);

        void init_with_memory_map(void* buffer, size_t buffer_size);

        void init_with_memory(void* buffer, size_t buffer_size);
    };

}
