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
 *	1. `begin_read`
 *	2. Loop through the following one or more times:
 *	   1. `get_read_chunk` (consumer thread can read the data only after the producer thread has marked the chunk as ready for consumption)
 *	3. `end_read` (the data read won't be marked as consumed until `end_read` is called; once marked as consumed, the memory is considered released and becomes available for the producer to write new data)
 *
 * \author pippocao
 * \date 2024/11/19
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/ring_buffer_base.h"

namespace bq {

    class siso_ring_buffer {
    private:
        static constexpr size_t cache_line_size = 64;
        static constexpr size_t cache_line_size_log2 = 6;
        static_assert(cache_line_size >> cache_line_size_log2 == 1, "invalid cache line size information");

        union cursor_type {
            alignas(8) bq::platform::atomic<uint32_t> atomic_value;
            alignas(8) uint32_t odinary_value;
        };

        union block {
        public:
            BQ_STRUCT_PACK(struct
                {
                    uint32_t block_num;
                    uint32_t data_size;
                    uint8_t data[1];
                } block_head);
        private:
            uint8_t data[cache_line_size];
        };

        BQ_STRUCT_PACK(struct mmap_head {
            // and it is a snapshot of last read cursor when recovering from memory map file .
            uint32_t read_cursor_cache_;
            uint32_t write_cursor_cache_;
            uint32_t aligned_blocks_count_cache_;
        });
        static_assert(sizeof(block) == cache_line_size, "the size of block should be equal to cache line size");
        const ptrdiff_t data_block_offset = reinterpret_cast<ptrdiff_t>(((block*)0)->block_head.data);
        static_assert(sizeof(block::block_head) < sizeof(block), "the size of block_head should be less than that of block");


        BQ_STRUCT_PACK(struct cursors_set {
            cursor_type write_cursor_;
            char cache_line_padding0_[cache_line_size - sizeof(write_cursor_)];
            cursor_type read_cursor_;
            char cache_line_padding1_[cache_line_size - sizeof(read_cursor_)];

            // these cache variables used in reading thread are used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            uint32_t rt_writing_cursor_cache_;
            uint32_t rt_reading_cursor_tmp_;  
            char cache_line_padding2_[cache_line_size - sizeof(rt_reading_cursor_tmp_) - sizeof(rt_writing_cursor_cache_)];
            // this cache variable used in writing thread is used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            uint32_t wt_reading_cursor_cache_;  
            char cache_line_padding3_[cache_line_size - sizeof(wt_reading_cursor_cache_)];
        });

        char cursors_storage_[sizeof(cursors_set) + cache_line_size];
        cursors_set* cursors_;   //make sure it is aligned to cache line size.

        mmap_head* mmap_head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bool is_memory_mapped_;

#if BQ_RING_BUFFER_DEBUG
        char padding_[cache_line_size];
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
        /// but the prerequisite is that the result of the returned ring_buffer_write_handle must be success.
        /// </summary>
        /// <param name="size">the chunk size you desired</param>
        /// <returns>this will only be valid if result code is enum_buffer_result_code::success</returns>
        ring_buffer_write_handle alloc_write_chunk(uint32_t size);

        /// <summary>
        /// After you have requested a memory block by calling alloc_write_chunk,
        /// you need to invoke commit_write_chunk for the returned `ring_buffer_write_handle`,
        /// regardless of whether the allocation was successful or not.
        /// </summary>
        /// <param name="handle">the handle you want to commit</param>
        void commit_write_chunk(const ring_buffer_write_handle& handle);

        /// <summary>
        /// begin read as a consumer
        /// </summary>
        void begin_read();

        /// <summary>
        /// To read a block of memory prepared by the producer,
        /// the function must be called between begin_read and end_read.
        /// </summary>
        /// <returns>data only be valid if result code is enum_buffer_result_code::success</returns>
        ring_buffer_read_handle read();

        /// <summary>
        /// the data read won't be marked as consumed until `end_read` is called;
        /// once marked as consumed, the memory is considered released and becomes
        /// available for the producer to call `alloc_write_chunk` to alloc and write new data
        /// </summary>
        void end_read();

        /// <summary>
        /// Warning:ring buffer can only be read from one thread at same time.
        /// This option is only work in Debug build and will be ignored in Release build.
        /// </summary>
        /// <param name="in_enable"></param>
        void set_thread_check_enable(bool in_enable)
        {
#if BQ_RING_BUFFER_DEBUG
            check_thread_ = in_enable;
            if (!check_thread_) {
                read_thread_id_ = empty_thread_id_;
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
