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
#if defined(BQ_WIN)
#include <Windows.h>

namespace bq {
    static size_t get_memory_map_size_unit()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return (size_t)sysInfo.dwAllocationGranularity;
    }


    bool memory_map::is_platform_support()
    {
        static_assert(sizeof(HANDLE) <= sizeof(memory_map_handle::platform_data_), "memory_map_handle::platform_data_ size not enough");
        return true;
    }

    size_t memory_map::get_memory_map_alignedment()
    {
        static size_t __memory_map_size_unit = get_memory_map_size_unit();
        return __memory_map_size_unit;
    }

    memory_map_handle memory_map::create_memory_map(const bq::file_handle& map_file, const size_t offset, const size_t size)
    {
        memory_map_handle result;
        if (!map_file.is_valid()) {
            result.error_code_ = ERROR_INVALID_HANDLE;
            bq::util::log_device_console_plain_text(log_level::error, "create_memory_map with invalid map_file");
            return result;
        }
        HANDLE file_handle = map_file.platform_handle();

        HANDLE& memory_map_handle = *(HANDLE*)(result.platform_data_);

        // alignment
        size_t real_mapping_offset = get_real_map_offset(offset);
        size_t real_mapping_size = get_real_map_size(offset, size);
        size_t alignment_offset = offset - get_real_map_offset(offset);
        size_t real_min_file_size = get_min_size_of_memory_map_file(offset, size);

        DWORD file_size_high = 0;
        DWORD file_size_low = GetFileSize(file_handle, &file_size_high);
        size_t current_file_size = ((size_t)file_size_high << 32) | (size_t)file_size_low;
        if (current_file_size < real_min_file_size) {
            size_t new_size = real_min_file_size;
            LONG new_size_high = (LONG)(new_size >> 32);
            LONG new_size_low = (LONG)(new_size & 0xFFFFFFFF);
            SetFilePointer(file_handle, new_size_low, &new_size_high, FILE_BEGIN);
            SetEndOfFile(file_handle);
        }

        DWORD new_size_high = (DWORD)(real_min_file_size >> 32);
        DWORD new_size_low = (DWORD)(real_min_file_size & 0xFFFFFFFF);

        memory_map_handle = CreateFileMapping(file_handle, NULL, PAGE_READWRITE, new_size_high, new_size_low, NULL);
        if (!memory_map_handle) {
            result.error_code_ = static_cast<int32_t>(GetLastError());
            bq::util::log_device_console(log_level::error, "create_memory_map file failed, path:%s, error_code:%d", map_file.abs_file_path().c_str(), result.error_code_);
            return result;
        }

        result.real_data_ = MapViewOfFile(memory_map_handle, FILE_MAP_READ | FILE_MAP_WRITE, (DWORD)(real_mapping_offset >> 32), (DWORD)(real_mapping_offset & 0x00000000FFFFFFFF), real_mapping_size);
        if (!result.real_data_) {
            CloseHandle(memory_map_handle);
            memory_map_handle = 0;
            result.error_code_ = static_cast<int32_t>(GetLastError());
            bq::util::log_device_console(log_level::error, "map_to_memory file failed, path:%s, error_code:%d", map_file.abs_file_path().c_str(), result.error_code_);
        }
        result.mapped_data_ = (void*)((uint8_t*)result.real_data_ + alignment_offset);
        result.file_ = map_file;
        result.size_ = size;
        return result;
    }

    void memory_map::flush_memory_map(const memory_map_handle& handle)
    {
#ifndef NDEBUG
        assert(handle.has_been_mapped() && "flush_memory_map can not be called without create_memory_map and map_to_memory");
#endif
        FlushViewOfFile(handle.real_data_, handle.size_ + static_cast<size_t>((uint8_t*)handle.mapped_data_ - (uint8_t*)handle.real_data_));
    }

    void memory_map::release_memory_map(memory_map_handle& handle)
    {
        if (!handle.has_been_mapped()) {
            return;
        }
        UnmapViewOfFile(handle.real_data_);
        HANDLE& memory_map_handle = *(HANDLE*)(handle.platform_data_);
        CloseHandle(memory_map_handle);
        handle.file_.invalid();
        memset(&handle.platform_data_, 0, sizeof(handle.platform_data_));
        handle.mapped_data_ = nullptr;
        handle.real_data_ = nullptr;
        handle.size_ = 0;
        handle.error_code_ = 0;
    }
}

#endif