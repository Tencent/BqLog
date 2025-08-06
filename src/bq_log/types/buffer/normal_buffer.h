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
 * \class bq::normal_buffer
 *
 * buffer for common use scenarios, for example where data chunks are larger than
 * default buffer size of miso_buffer or siso_buffer.
 * 
 * \author pippocao
 * \date 2025/07/26
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    class normal_buffer {
    private:
        static constexpr size_t HEAD_SIZE = CACHE_LINE_SIZE;
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
        uint8_t* buffer_data_;
        uint32_t required_size_;
        bool need_recovery_;
        bq::string mmap_file_abs_path_;
        alignas(8) char siso_buffer_obj_[sizeof(bq::siso_ring_buffer)];
    public :
        normal_buffer(uint32_t size, bool need_recovery, const bq::string& mmap_file_abs_path);

        ~normal_buffer();

        template <typename T>
        bq_forceinline T& get_misc_data()
        {
            static_assert(sizeof(T) <= HEAD_SIZE, "size of T too large");
            return *reinterpret_cast<T*>((void*)buffer_data_);
        }

        static uint32_t calculate_size_of_memory(uint32_t required_size);

        bq_forceinline log_buffer_write_handle alloc_write_chunk(uint32_t size)
        {
            return get_buffer().alloc_write_chunk(size);
        }

        bq_forceinline void commit_write_chunk(const log_buffer_write_handle& handle)
        {
            get_buffer().commit_write_chunk(handle);
        }

        bq_forceinline log_buffer_read_handle read_chunk()
        {
            return get_buffer().read_chunk();
        }

        bq_forceinline log_buffer_read_handle read_an_empty_chunk()
        {
            return get_buffer().read_an_empty_chunk();
        }

        bq_forceinline void discard_read_chunk(log_buffer_read_handle& handle)
        {
            get_buffer().discard_read_chunk(handle);
        }

        bq_forceinline void return_read_chunk(const log_buffer_read_handle& handle)
        {
            get_buffer().return_read_chunk(handle);
        }

        bq_forceinline siso_ring_buffer::siso_buffer_batch_read_handle batch_read()
        {
            return get_buffer().batch_read();
        }

        bq_forceinline void return_batch_read_chunks(const siso_ring_buffer::siso_buffer_batch_read_handle& handle)
        {
            get_buffer().return_batch_read_chunks(handle);
        }

        bq::string get_mmap_file_path() const
        {
            return memory_map_file_.abs_file_path();
        }

        bq_forceinline const uint8_t* get_buffer_addr() const
        {
            return get_buffer().get_buffer_addr();
        }

        bq_forceinline uint32_t get_block_size() const
        {
            return get_buffer().get_block_size();
        }

        bq_forceinline uint32_t get_total_blocks_count() const
        {
            return get_buffer().get_total_blocks_count();
        }
    private:

        void init_buffer_data();

        create_memory_map_result create_memory_map();

        bq_forceinline siso_ring_buffer& get_buffer() { return *(siso_ring_buffer*)siso_buffer_obj_; }

        bq_forceinline const siso_ring_buffer& get_buffer() const { return *(const siso_ring_buffer*)siso_buffer_obj_; }

        bool try_recover_from_exist_memory_map();

        void init_with_memory_map();

        void init_with_memory();

        bool uninit_memory_map();
    };
}
