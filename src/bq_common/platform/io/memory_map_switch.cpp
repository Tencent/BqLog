/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/platform/io/memory_map.h"
// Nintendo Switch has no memory-mapped file IO on either development
// environment: newlib on devkitA64 ships no <sys/mman.h>, and the official
// SDK's nn::fs has no mapping concept. Report "unsupported" so that log
// buffers fall back to heap memory.
#if defined(BQ_SWITCH)
namespace bq {
    bool memory_map::is_platform_support()
    {
        return false;
    }

    size_t memory_map::get_memory_map_alignedment()
    {
        return 4096; // Horizon page size
    }

    memory_map_handle memory_map::create_memory_map(const bq::file_handle& map_file, const size_t offset, const size_t size)
    {
        (void)map_file;
        (void)offset;
        (void)size;
        memory_map_handle result;
        result.error_code_ = ENOSYS;
        return result;
    }

    void memory_map::flush_memory_map(const memory_map_handle& handle)
    {
        (void)handle;
    }

    void memory_map::release_memory_map(memory_map_handle& handle)
    {
        (void)handle;
    }
}
#endif
