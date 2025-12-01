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
 * \class bq::normal_buffer
 *
 * buffer for common use scenarios. It can backed by memory-mapped file or heap memory.
 *
 * \author pippocao
 * \date 2025/07/26
 */
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {
    class normal_buffer {
    private:
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
        void* buffer_data_;
        size_t buffer_size_;
        create_memory_map_result mmap_result_;
        bool delete_mmap_when_destruct_;
    public:
        normal_buffer(size_t size, const bq::string& mmap_file_abs_path = "", bool auto_create = true);

        virtual ~normal_buffer();

        bq::string get_mmap_file_path() const
        {
            return memory_map_file_.abs_file_path();
        }

        bq_forceinline void* data() { return buffer_data_; }

        bq_forceinline const void* data() const { return buffer_data_; }

        bq_forceinline size_t size() const { return buffer_size_; }

        bq_forceinline create_memory_map_result get_mmap_result() const { return mmap_result_; }

        bq_forceinline void set_delete_mmap_when_destruct(bool delete_mmap) { delete_mmap_when_destruct_ = delete_mmap; }

        void resize(size_t new_size);

        bool is_memory_mapped() const
        {
            return memory_map_handle_.has_been_mapped();
        }

    private:
        create_memory_map_result create_memory_map(size_t size, const bq::string& mmap_file_abs_path, bool auto_create);
    };
}
