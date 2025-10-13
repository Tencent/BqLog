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
 *	3. `return_read_chunk` (the data read won't be marked as consumed until `return_read_chunk` is called; once marked as consumed, the memory is considered released and becomes available for the producer to write new data)
 *
 * \author pippocao
 * \date 2024/11/19
 */

#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {

    class siso_ring_buffer {
    private:
        static constexpr size_t BLOCK_SIZE = 8;
        static constexpr size_t BLOCK_SIZE_LOG2 = 3;

        BQ_PACK_BEGIN
        struct alignas(4) chunk_head_def { // alignas(4) can make sure compiler generate more effective code when access int32_t fields
            uint32_t block_num;
            uint32_t data_size;
            uint8_t data[1];
            uint8_t padding[3];
        } BQ_PACK_END

            static_assert(sizeof(chunk_head_def) == 12, "the size of chunk_head_def should be equal to 12 bytes");

        struct block {
            uint8_t padding[BLOCK_SIZE];
            bq_forceinline chunk_head_def& to_chunk_head()
            {
                return *reinterpret_cast<chunk_head_def*>(this);
            }
        };
        static_assert(sizeof(block) == BLOCK_SIZE, "invalid siso buffer block size");
        const ptrdiff_t data_block_offset = static_cast<ptrdiff_t>(BQ_POD_RUNTIME_OFFSET_OF(chunk_head_def, data));

        BQ_PACK_BEGIN
        struct alignas(4) head {
            uint32_t aligned_blocks_count_cache_; // This field is used as snapshot when recovering from memory map file .

            // these cache variables used in reading thread are used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            // rt is short name of "read thread", which means this variable is accessed in read thread.
            uint32_t rt_reading_cursor_cache_; // This field is used as snapshot when recovering from memory map file .
            uint32_t rt_writing_cursor_cache_;
            char cache_line_padding0_[BQ_CACHE_LINE_SIZE - 3 * sizeof(uint32_t)];
            // this cache variable used in writing thread is used to reduce atomic loading, this can improve MESI performance in high concurrency scenario.
            // wt is short name of "write thread", which means this variable is accessed in write thread.
            uint32_t wt_reading_cursor_cache_;
            uint32_t wt_writing_cursor_cache_; // This field is used as snapshot when recovering from memory map file .
            char cache_line_padding1_[BQ_CACHE_LINE_SIZE - 2 * sizeof(uint32_t)];

            uint32_t reading_cursor_; // Used to sync data between read thread and write thread in run-time.
            char cache_line_padding2_[BQ_CACHE_LINE_SIZE - sizeof(uint32_t)];
            uint32_t writing_cursor_; // Used to sync data between read thread and write thread in run-time.
            char cache_line_padding3_[BQ_CACHE_LINE_SIZE - sizeof(uint32_t)];

        } BQ_PACK_END static_assert(sizeof(head) == 4 * BQ_CACHE_LINE_SIZE, "the size of head should be equal to 2 X cache line size");

        head* head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bool is_memory_recovery_;
        memory_map_buffer_state mmap_buffer_state_;

#if defined(BQ_LOG_BUFFER_DEBUG)
        char padding_[BQ_CACHE_LINE_SIZE];
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id write_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
        bq::platform::atomic<uint32_t> result_code_statistics_[(int32_t)enum_buffer_result_code::result_code_count];
        bq::platform::atomic<uint64_t> total_write_bytes_;
        bq::platform::atomic<uint64_t> total_read_bytes_;
        bool is_read_chunk_waiting_for_return_ = false;
#endif
    public:
        struct siso_buffer_batch_read_handle {
            friend class siso_ring_buffer;
            enum_buffer_result_code result = enum_buffer_result_code::err_empty_log_buffer;

        private:
            class siso_ring_buffer* parent_;
            uint32_t start_cursor_;
            uint32_t current_cursor_;
            uint32_t end_cursor_;
#if defined(BQ_LOG_BUFFER_DEBUG)
            uint32_t last_cursor_ = UINT32_MAX;
#endif
        public:
            bool has_next() const
            {
                return current_cursor_ != end_cursor_;
            }
            log_buffer_read_handle next()
            {
                log_buffer_read_handle handle;
                handle.result = result;
                block& current_block = parent_->cursor_to_block(current_cursor_);
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(result == enum_buffer_result_code::success && "siso_buffer_batch_read_handle::next() called when read failed");
                assert(has_next() && "siso_buffer_batch_read_handle::next() called when no more data to read");
                auto block_count = current_block.to_chunk_head().block_num;
                assert(block_count > 0 && (static_cast<uint32_t>(parent_->head_->rt_writing_cursor_cache_ - parent_->head_->rt_reading_cursor_cache_) >= block_count));
#endif
                handle.data_size = current_block.to_chunk_head().data_size;
                handle.data_addr = parent_->get_block_data_addr(current_block);
#if defined(BQ_LOG_BUFFER_DEBUG)
                last_cursor_ = current_cursor_;
#endif
                current_cursor_ += current_block.to_chunk_head().block_num;
                parent_->head_->rt_reading_cursor_cache_ = current_cursor_;
                return handle;
            }

#if defined(BQ_LOG_BUFFER_DEBUG)
            bool verify_chunk(const log_buffer_read_handle& handle) const
            {
                if (result != enum_buffer_result_code::success) {
                    return false;
                }
                block& last_block = parent_->cursor_to_block(last_cursor_);
                auto corret_data_addr = parent_->get_block_data_addr(last_block);
                if (handle.data_addr != corret_data_addr) {
                    return false;
                }
                return true;
            }
#endif
        };
        friend struct siso_buffer_batch_read_handle;

    public:
        siso_ring_buffer() = delete;

        siso_ring_buffer(void* buffer, size_t buffer_size, bool is_memory_recovery);

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
        /// To read all available data in the ring buffer at once.
        /// </summary>
        /// <returns>all available data, you can call next() to traverse it. </returns>
        siso_buffer_batch_read_handle batch_read();

        /// <summary>
        /// the chunk date read by `batch_read` won't be marked as consumed until `return_batch_read_chunks` is called;
        /// once marked as consumed, the memory is considered released and becomes
        /// available for the producer to call `alloc_write_chunk` to alloc and write new data
        /// </summary>
        /// <param name="handle"></param>
        void return_batch_read_chunks(const siso_buffer_batch_read_handle& handle);

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
#if defined(BQ_LOG_BUFFER_DEBUG)
            check_thread_ = in_enable;
            if (!check_thread_) {
                read_thread_id_ = empty_thread_id_;
                write_thread_id_ = empty_thread_id_;
            }
#else
            (void)in_enable;
#endif
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

        bq_forceinline bool get_is_memory_recovery() const
        {
            return is_memory_recovery_;
        }

    private:
        bq_forceinline block& cursor_to_block(uint32_t cursor)
        {
            return aligned_blocks_[cursor & (aligned_blocks_count_ - 1)];
        }

        bq_forceinline uint8_t* get_block_data_addr(block& current_block)
        {
            if ((uint8_t*)&current_block.to_chunk_head().data + current_block.to_chunk_head().data_size > (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) {
                // split
                return (uint8_t*)aligned_blocks_;
            } else {
                return current_block.to_chunk_head().data;
            }
        }

        bool try_recover_from_exist_memory_map(void* buffer, size_t buffer_size);

        void init_with_memory_map(void* buffer, size_t buffer_size);

        void init_with_memory(void* buffer, size_t buffer_size);
    };

}
