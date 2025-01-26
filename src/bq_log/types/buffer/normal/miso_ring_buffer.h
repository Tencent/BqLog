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
 * \class bq::miso_ring_buffer V3
 *
 * `miso`("multi threads in and single thread out").
 * 
 * This high-performance ring buffer supports multi-threaded writes and single-threaded reads simultaneously.
 * The buffer size is determined during initialization and cannot be changed.
 * Data in the ring buffer chunks are stored contiguously.
 * The implementation is designed for extremely high concurrent performance and is optimized to be CPU cache friendly.
 *
 * V2 performance improvements:
 *   1. Reorganize the data structure by dividing the ring buffer into blocks,
 *      with each block having the size of the CPU's cache line, thus reducing False Sharing.
 *   2. Significantly reduce inter-thread synchronization to minimize CPU cache latency.
 *
 * V3 Memory Map supported:
 *   1. If a "memory map" (like mmap) is supported by the running platform and the serialize_id is not 0,
 *      the ring buffer attempts to serialize itself to disk.
 *      This helps ensure that even if the program is terminated without processing the
 *      remaining data in the ring buffer, the data would not be lost in most cases.
 *      Also, it tries to recover the data automatically when the program is relaunched.
 *      (This ability is not 100% guaranteed but works well in most cases.
 *       Failures may occur due to various reasons, such as OS behavior, IO latency, physical power-off, etc.)
 *
 *
 * Usage:
 * Producer Threads (Multiple Threads): Loop through the sequence of the following steps:
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
 * \date 2023/10/08
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {

    class miso_ring_buffer {
    private:
        static constexpr size_t CACHE_LINE_SIZE = 64;
        static constexpr size_t CACHE_LINE_SIZE_LOG2 = 6;
        static_assert(CACHE_LINE_SIZE >> CACHE_LINE_SIZE_LOG2 == 1, "invalid cache line size information");
        enum class block_status : int32_t{
            unused, // data section begin from this block is unused, can not be read
            used, // data section begin from this block has already finished writing, and can be read.
            invalid // data section begin from this block is invalid, it should be skipped by reading thread.
        };
        union block {
        public:
            BQ_STRUCT_PACK(struct
                {
                    block_status status;
                    uint32_t block_num;
                    uint32_t data_size;
                    uint8_t data[1];
                } block_head);

        private:
            uint8_t data[CACHE_LINE_SIZE];
        };
        BQ_STRUCT_PACK(struct mmap_head {
            // it is a snapshot of last read cursor when recovering from memory map file .
            uint32_t read_cursor_cache_;
            uint64_t log_checksum_;
        });
        static_assert(sizeof(block) == CACHE_LINE_SIZE, "the size of block should be equal to cache line size");
        const ptrdiff_t data_block_offset = reinterpret_cast<ptrdiff_t>(((block*)0)->block_head.data);
        static_assert(sizeof(block::block_head) < sizeof(block), "the size of block_head should be less than that of block");

        BQ_STRUCT_PACK(struct cursors_set {
            uint32_t write_cursor_;
            char cache_line_padding0_[CACHE_LINE_SIZE - sizeof(write_cursor_)];
            uint32_t read_cursor_;
            char cache_line_padding1_[CACHE_LINE_SIZE - sizeof(read_cursor_)];

            // these cache variables used in reading thread are used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            // rt is short name of "read thread", which means this variable is accessed in read thread.
            uint32_t rt_reading_cursor_tmp_;
            uint32_t rt_reading_count_in_batch_;
            char cache_line_padding2_[CACHE_LINE_SIZE - sizeof(rt_reading_cursor_tmp_) - sizeof(rt_reading_count_in_batch_)];
        });
        log_buffer_config config_;
        char cursors_storage_[sizeof(cursors_set) + CACHE_LINE_SIZE];
        cursors_set* cursors_; // make sure it is aligned to cache line size.

        uint8_t* real_buffer_;
        mmap_head* mmap_head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
#if BQ_JAVA
        bq::platform::mutex java_mutex_;
        jobject direct_byte_array_obj_;
#endif
#if BQ_LOG_BUFFER_DEBUG
        uint8_t padding_[CACHE_LINE_SIZE];
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
        bq::platform::atomic<uint32_t> result_code_statistics_[(int32_t)enum_buffer_result_code::result_code_count];
        bq::platform::atomic<uint64_t> total_write_bytes_;
        bq::platform::atomic<uint64_t> total_read_bytes_;
#endif
    public:
        miso_ring_buffer(const log_buffer_config& config);

        miso_ring_buffer(const miso_ring_buffer& rhs) = delete;

        /// de-constuctor;
        ~miso_ring_buffer();

        /// <summary>
        /// A producer can request a block of memory by calling alloc_write_chunk for writing data,
        /// but the prerequisite is that the result of the returned ring_buffer_write_handle must be success.
        /// </summary>
        /// <param name="size">the chunk size you desired</param>
        /// <returns>this will only be valid if result code is enum_buffer_result_code::success</returns>
        log_buffer_write_handle alloc_write_chunk(uint32_t size);

        /// <summary>
        /// After you have requested a memory block by calling alloc_write_chunk,
        /// you need to invoke commit_write_chunk for the returned `ring_buffer_write_handle`,
        /// regardless of whether the allocation was successful or not.
        /// </summary>
        /// <param name="handle">the handle you want to commit</param>
        void commit_write_chunk(const log_buffer_write_handle& handle);

        /// <summary>
        /// To read a block of memory prepared by the producer,
        /// </summary>
        /// <param name="current_epoch_ms"></param>
        /// <returns>data only be valid if result code is enum_buffer_result_code::success</returns>
        log_buffer_read_handle read_chunk(uint64_t current_epoch_ms);

        /// <summary>
        /// the chunk date read by `read_chunk` won't be marked as consumed until `return_read_trunk` is called;
        /// once marked as consumed, the memory is considered released and becomes
        /// available for the producer to call `alloc_write_chunk` to alloc and write new data
        /// </summary>
        /// <param name=""></param>
        void return_read_trunk(const log_buffer_read_handle& handle);

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
            }
#else
            (void)in_enable;
#endif
        }

#if BQ_JAVA
        java_buffer_info get_java_buffer_info(JavaVM* jvm, const log_buffer_write_handle& handle);
#endif

        bq_forceinline uint32_t get_block_size() const
        {
            return sizeof(block);
        }

        bq_forceinline uint32_t get_total_blocks_count() const
        {
            return aligned_blocks_count_;
        }

    private:
        bq_forceinline block& cursor_to_block(uint32_t cursor)
        {
            return aligned_blocks_[cursor & (aligned_blocks_count_ - 1)];
        }

        create_memory_map_result create_memory_map();

        bool try_recover_from_exist_memory_map();

        void init_with_memory_map();

        void init_with_memory();

        bool uninit_memory_map();
    };
}
