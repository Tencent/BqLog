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
 * \file memory_map.h
 * simple wrapper class of mmap
 * \author pippocao
 * \date 2023
 *
 *
 */
#include "bq_common/bq_common.h"
#include "bq_common/utils/file_manager.h"
namespace bq {
    struct file_handle;
    class memory_map_handle {
    private:
        friend class memory_map;
        uint8_t platform_data_[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        void* mapped_data_ = nullptr;
        void* real_data_ = nullptr;
        size_t size_ = 0;
        int32_t error_code_ = 0;
        file_handle file_;

    public:
        inline void* get_mapped_data() const
        {
            return mapped_data_;
        }
        inline size_t get_mapped_size() const
        {
            return size_;
        }
        inline int32_t get_error_code() const
        {
            return error_code_;
        }
        inline bool has_been_mapped() const
        {
            return mapped_data_;
        }
    };

    class memory_map {
    public:
        static bool is_platform_support();

        static size_t get_memory_map_alignedment();

        static memory_map_handle create_memory_map(const bq::file_handle& map_file, const size_t offset, const size_t size);

        static void flush_memory_map(const memory_map_handle& handle);

        static void release_memory_map(memory_map_handle& handle);

    private:
        static size_t get_real_map_offset(size_t offset)
        {
            size_t alignment_offset = offset % get_memory_map_alignedment();
            size_t real_mapping_offset = offset - alignment_offset;
            return real_mapping_offset;
        }
        static size_t get_real_map_size(size_t offset, size_t size)
        {
            size_t alignment_offset = offset % get_memory_map_alignedment();
            size_t real_mapping_size = size + alignment_offset;
            real_mapping_size += (get_memory_map_alignedment() - 1);
            real_mapping_size -= (real_mapping_size % get_memory_map_alignedment());
            return real_mapping_size;
        }

    public:
        static size_t get_min_size_of_memory_map_file(size_t offset, size_t size)
        {
            return get_real_map_offset(offset) + get_real_map_size(offset, size);
        }
    };
}
