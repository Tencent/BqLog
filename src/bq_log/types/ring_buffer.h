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
 * \class bq::ring_buffer V3
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

#ifndef NDEBUG
#define BQ_RING_BUFFER_DEBUG 1
#else
#define BQ_RING_BUFFER_DEBUG 0
#endif

namespace bq {
    struct ring_buffer_handle_base {
        enum_buffer_result_code result = enum_buffer_result_code::err_alloc_size_invalid;
        uint8_t* data_addr = nullptr;
    };

    struct ring_buffer_write_handle : public ring_buffer_handle_base {
        uint32_t approximate_used_blocks_count = 0; // just approximate because of multi-thread
    };

    struct ring_buffer_read_handle : public ring_buffer_handle_base {
        uint32_t data_size;
    };

    class ring_buffer {
    private:
        static constexpr size_t cache_line_size = 64;
        static constexpr size_t cache_line_size_log2 = 6;
        static_assert(cache_line_size >> cache_line_size_log2 == 1, "invalid cache line size information");
        union cursor_type {
            alignas(8) bq::platform::atomic<uint32_t> atomic_value;
            alignas(8) volatile uint32_t volatile_value;
            alignas(8) uint32_t odinary_value;
            cursor_type()
            {
                atomic_value.store(0);
            }
            cursor_type(const cursor_type& rhs)
                : odinary_value(rhs.atomic_value.load(bq::platform::memory_order::acquire))
            {
            }
        };
        static_assert(offsetof(cursor_type, atomic_value) == offsetof(cursor_type, odinary_value), "invalid cursor_type size");
        enum block_status {
            unused, // data section begin from this block is unused, can not be read
            used, // data section begin from this block has already finished writing, and can be read.
            invalid // data section begin from this block is invalid, it should be skipped by reading thread.
        };
        union block {
        public:
            BQ_STRUCT_PACK(struct
                {
                    union {
                        bq::platform::atomic<block_status> status;
                        block_status odinary_status;
                    };
                    uint32_t block_num;
                    uint32_t data_size;
                    uint8_t data[1];
                } data_section_head);

        private:
            uint8_t data[cache_line_size];
        };
        BQ_STRUCT_PACK(struct buffer_head {
            // used in reading thread to avoid load atomic variable read_cursor_, this can improve performance in high concurrency scenario.
            // and it is a snapshot of last read cursor when recovering from memory map file .
            uint32_t read_cursor_consumer_cache_;
        });
        static_assert(sizeof(block) == cache_line_size, "size of data_section_head should be equal-or-less than size of block");
        const ptrdiff_t data_block_offset = reinterpret_cast<ptrdiff_t>(((block*)0)->data_section_head.data);
        static constexpr size_t padding_byte = cache_line_size - sizeof(cursor_type);
        static constexpr size_t padding_count = padding_byte >> 2; // divide sizeof(uint32_t)
        // CAS version of allocation is slower in high concurrency environment but more stable
        static constexpr bool use_cas_alloc = false;

        char cache_line_padding0_[cache_line_size];
        alignas(8) cursor_type write_cursor_;
        char cache_line_padding1_[cache_line_size - sizeof(write_cursor_)];
        alignas(8) cursor_type read_cursor_;
        char cache_line_padding2_[cache_line_size - sizeof(write_cursor_)];
        uint32_t current_reading_cursor_;
        char cache_line_padding3_[cache_line_size - sizeof(write_cursor_)];
        uint8_t* real_buffer_;
        buffer_head* buffer_head_;
        block* aligned_blocks_;
        uint32_t aligned_blocks_count_; // the max size of aligned_blocks_count_ will not exceed (INT32_MAX / sizeof(block))
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
        uint8_t padding_[cache_line_size];

#if BQ_RING_BUFFER_DEBUG
        bool check_thread_ = true;
        bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
        bq::platform::atomic<uint32_t> result_code_statistics_[(int32_t)enum_buffer_result_code::result_code_count];
        bq::platform::atomic<uint64_t> total_write_bytes_;
        bq::platform::atomic<uint64_t> total_read_bytes_;
#endif
    public:
        /// <summary>
        /// constructor and initializer of ring_buffer
        /// </summary>
        /// <param name="capacity">desired max size of this ring_buffer(in bytes)</param>
        /// <param name="serialize_id">ring buffer will try to serialize self to disk
        ///  if memory map is supported on running platform and serialize_id is not 0.
        ///  so that if program is killed without process the left data in ring_buffer. it will not
        ///  be lost in most cases and can be recovered when program is relaunched.</param>
        ring_buffer(uint32_t capacity, uint64_t serialize_id = 0);

        ring_buffer(const ring_buffer& rhs) = delete;

        /// de-constuctor;
        ~ring_buffer();

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

    private:
        bq_forceinline block& cursor_to_block(uint32_t cursor)
        {
            return aligned_blocks_[cursor & (aligned_blocks_count_ - 1)];
        }

        enum create_memory_map_result {
            failed,
            use_existed,
            new_created
        };

        create_memory_map_result create_memory_map(uint32_t capacity, uint64_t serialize_id);

        bool try_recover_from_exist_memory_map();

        void init_with_memory_map();

        void init_with_memory(uint32_t capacity);

        bool uninit_memory_map();
    };
}
