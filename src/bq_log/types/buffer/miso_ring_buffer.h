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
 *	1. `read_chunk`
 *	2. Read data from chunk if #1 returns successful.
 *	3. `return_read_chunk` (the data read won't be marked as consumed until `return_read_chunk` is called; once marked as consumed, the memory is considered released and becomes available for the producer to write new data)
 *
 * \author pippocao
 * \date 2023/10/08
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {

    class alignas(CACHE_LINE_SIZE) miso_ring_buffer {
    private:
        static constexpr size_t CACHE_LINE_SIZE_LOG2 = 6;
        static_assert(CACHE_LINE_SIZE >> CACHE_LINE_SIZE_LOG2 == 1, "invalid cache line size information");
        enum class block_status : uint8_t {
            unused, // data section begin from this block is unused, can not be read
            used, // data section begin from this block has already finished writing, and can be read.
            invalid // data section begin from this block is invalid, it should be skipped by reading thread.
        };
        union block {
        public:
            BQ_PACK_BEGIN
            struct alignas(4) chunk_head_def { // alignas(4) can make sure compiler generate more effective code when access int32_t fields
            private:
                char block_num_[3]; // 24 bits block number
            public:
                block_status status; // 8 bits status
                uint32_t data_size;
                uint8_t data[1];
                uint8_t padding[3];

            public:
                bq_forceinline uint32_t get_block_num() const
                {
                    return (*(const uint32_t*)block_num_) & (0xFFFFFF);
                }
                bq_forceinline void set_block_num(uint32_t num)
                {
                    *(uint32_t*)block_num_ = num;
                }
        } BQ_PACK_END public : static constexpr uint32_t MAX_BLOCK_NUM_PER_CHUNK = 0xFFFFFF;
            chunk_head_def chunk_head;

        private:
            uint8_t data[CACHE_LINE_SIZE];
        };

        // frequently modified by read thread, so don't access it in write thread too often.
        BQ_PACK_BEGIN
        struct alignas(4) head {
            // it is a snapshot of last read cursor when recovering from memory map file .
            uint32_t read_cursor_cache_;
            uint32_t read_cursor_start_cache_;
            uint64_t log_checksum_;
            char mmap_misc_data_[CACHE_LINE_SIZE - sizeof(read_cursor_cache_) - sizeof(log_checksum_) - sizeof(read_cursor_start_cache_)];
        } BQ_PACK_END static_assert(sizeof(head) == CACHE_LINE_SIZE, "the size of head should be equal to cache line size");

        static_assert(sizeof(block) == CACHE_LINE_SIZE, "the size of block should be equal to cache line size");
        const ptrdiff_t data_block_offset = reinterpret_cast<ptrdiff_t>(((block*)0)->chunk_head.data);
        static_assert(sizeof(block::chunk_head) < sizeof(block), "the size of chunk_head should be less than that of block");

        struct alignas(CACHE_LINE_SIZE) cursors_set {
            bq::platform::atomic<uint32_t> write_cursor_;
            char cache_line_padding0_[CACHE_LINE_SIZE - sizeof(write_cursor_)];
            bq::platform::atomic<uint32_t> read_cursor_;
            char cache_line_padding1_[CACHE_LINE_SIZE - sizeof(write_cursor_)];
        };
        static_assert(sizeof(cursors_set) == CACHE_LINE_SIZE * 2, "invalid cursors_set size");

        log_buffer_config config_;
        memory_map_buffer_state mmap_buffer_state_;
        cursors_set cursors_; // make sure it is aligned to cache line size even in placement new.

        head* head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
#if defined(BQ_LOG_BUFFER_DEBUG)
        alignas(CACHE_LINE_SIZE) bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
        bq::platform::atomic<uint32_t> result_code_statistics_[(int32_t)enum_buffer_result_code::result_code_count];
        bq::platform::atomic<uint64_t> total_write_bytes_;
        bq::platform::atomic<uint64_t> total_read_bytes_;
        bool is_read_chunk_waiting_for_return_ = false;
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
        /// <returns>data only be valid if result code is enum_buffer_result_code::success</returns>
        log_buffer_read_handle read_chunk();

        /// <summary>
        /// A ugly hack, used to return a empty chunk even when the buffer is not empty
        /// </summary>
        /// <returns>result code is enum_buffer_result_code::err_empty_log_buffer</returns>
        log_buffer_read_handle read_an_empty_chunk();

        /// <summary>
        /// Some times you just want a peak of the data, and you don't want to consume it.
        /// So you can call the function instead of `return_read_chunk` to discard the read handle.
        /// And you will get the same data next time you call `read_chunk`.
        /// Note : the result code of the handle will be set to `enum_buffer_result_code::err_empty_log_buffer` after calling this function.
        /// </summary>
        /// <param name="handle"></param>
        void discard_read_chunk(log_buffer_read_handle& handle);

        /// <summary>
        /// the chunk date read by `read_chunk` won't be marked as consumed until `return_read_chunk` is called;
        /// once marked as consumed, the memory is considered released and becomes
        /// available for the producer to call `alloc_write_chunk` to alloc and write new data
        /// </summary>
        /// <param name=""></param>
        void return_read_chunk(const log_buffer_read_handle& handle);

        /// <summary>
        /// Traverses all data written, invoking the provided callback for each chunk from the consumer thread.
        /// </summary>
        /// <param name="in_callback">A function pointer to the callback that will be called with the data and its size.</param>
        /// <param name="in_user_data">user defined data passed to in_callback.</param>
        void data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data);

        /// <summary>
        /// Warning:ring buffer can only be read from one thread at same time.
        /// This option is only work in Debug build and will be ignored in Release build.
        /// </summary>
        /// <param name="in_enable"></param>
        void set_thread_check_enable(bool in_enable)
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            check_thread_ = in_enable;
            if (!check_thread_) {
                read_thread_id_ = empty_thread_id_;
            }
#else
            (void)in_enable;
#endif
        }

        bq_forceinline const log_buffer_config& get_config() const
        {
            return config_;
        }

        bq_forceinline memory_map_buffer_state get_memory_map_buffer_state() const
        {
            return mmap_buffer_state_;
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

        template <typename T>
        bq_forceinline T& get_mmap_misc_data()
        {
            static_assert(sizeof(T) <= sizeof(head_->mmap_misc_data_), "size of T too large");
            return *((T*)(void*)head_->mmap_misc_data_);
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
