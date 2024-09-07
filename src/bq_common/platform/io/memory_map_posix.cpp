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
#include "bq_common/bq_common.h"
#if BQ_POSIX
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

namespace bq {
    static size_t get_memory_map_size_unit()
    {
        return (size_t)getpagesize();
    }

    static size_t __memory_map_size_unit = get_memory_map_size_unit();

    bool memory_map::is_platform_support()
    {
        static_assert(sizeof(size_t) <= sizeof(memory_map_handle::platform_data_), "memory_map_handle::platform_data_ size not enough");
#if BQ_ANDROID || BQ_APPLE || BQ_LINUX || BQ_UNIX
        return true;
#else
        return false;
#endif
    }

    size_t memory_map::get_memory_map_alignedment()
    {
        return __memory_map_size_unit;
    }

    memory_map_handle memory_map::create_memory_map(const bq::file_handle& map_file, const size_t offset, const size_t size)
    {
        memory_map_handle result;
        if (!map_file.is_valid()) {
            result.error_code_ = EBADF;
            bq::util::log_device_console_plain_text(log_level::error, "create_memory_map with invalid map_file");
            return result;
        }

        auto fd = map_file.platform_handle();
        auto current_size = lseek(fd, 0, SEEK_END);

        // alignment
        size_t alignment_offset = offset % __memory_map_size_unit;

        size_t real_mapping_offset = offset - alignment_offset;
        size_t real_mapping_size = size + alignment_offset;
        real_mapping_size += (__memory_map_size_unit - 1);
        real_mapping_size -= (real_mapping_size % __memory_map_size_unit);

        if ((size_t)current_size < real_mapping_offset + real_mapping_size) {
            if (0 != ftruncate(fd, (off_t)(real_mapping_offset + real_mapping_size))) {
                bq::util::log_device_console(log_level::warning, "create_memory_map ftruncate() file failed, path:%s, error_code:%d", map_file.abs_file_path().c_str(), errno);
            }
        }

        result.real_data_ = mmap(NULL, real_mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, real_mapping_offset);

        if (MAP_FAILED == result.real_data_) {
            result.error_code_ = errno;
            bq::util::log_device_console(log_level::error, "create_memory_map file failed, path:%s, error_code:%d", map_file.abs_file_path().c_str(), result.error_code_);
            return result;
        }

        result.mapped_data_ = (void*)((uint8_t*)result.real_data_ + alignment_offset);
        result.file_ = map_file;
        result.size_ = size;
        *(size_t*)result.platform_data_ = real_mapping_size;
        return result;
    }

    void memory_map::flush_memory_map(const memory_map_handle& handle)
    {
#ifndef NDEBUG
        assert(handle.has_been_mapped() && "flush_memory_map can not be called without create_memory_map and map_to_memory");
#endif
        if (0 != msync(handle.real_data_, *(size_t*)handle.platform_data_, MS_SYNC)) {
            bq::util::log_device_console(log_level::error, "flush_memory_map file failed, error_code:%d", errno);
        }
    }

    void memory_map::release_memory_map(memory_map_handle& handle)
    {
        if (!handle.has_been_mapped()) {
            return;
        }
        if (0 != munmap(handle.real_data_, *(size_t*)handle.platform_data_)) {
            bq::util::log_device_console(log_level::error, "release_memory_map file failed, error_code:%d", errno);
        }
        handle.file_.invalid();
        memset(&handle.platform_data_, 0, sizeof(handle.platform_data_));
        handle.mapped_data_ = nullptr;
        handle.real_data_ = nullptr;
        handle.size_ = 0;
        handle.error_code_ = 0;
    }
}

#endif