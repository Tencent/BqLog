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
#include "bq_common/platform/io/memory_map_posix.h"
#if defined(BQ_POSIX)
#include <unistd.h>
#include <sys/mman.h>

namespace bq {
    static size_t get_memory_map_size_unit()
    {
        return common_global_vars::get().page_size_;
    }

    bool memory_map::is_platform_support()
    {
        static_assert(sizeof(size_t) <= sizeof(memory_map_handle::platform_data_), "memory_map_handle::platform_data_ size not enough");
#if defined(BQ_ANDROID) || defined(BQ_APPLE) || defined(BQ_LINUX) || defined(BQ_UNIX) || defined(BQ_PS)
        return true;
#else
        return false;
#endif
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
            result.error_code_ = EBADF;
            bq::util::log_device_console_plain_text(log_level::error, "create_memory_map with invalid map_file");
            return result;
        }

        auto fd = map_file.platform_handle();

        // alignment
        size_t real_mapping_offset = get_real_map_offset(offset);
        size_t real_mapping_size = get_real_map_size(offset, size);
        size_t alignment_offset = offset - get_real_map_offset(offset);
        size_t real_min_file_size = get_min_size_of_memory_map_file(offset, size);
        assert(real_min_file_size == real_mapping_offset + real_mapping_size);

        // Materialize the whole mapping range with real disk blocks before mapping.
        // ftruncate only changes the logical file size and leaves holes. Writing to
        // a hole page through a MAP_SHARED mapping needs a block allocation at page
        // fault time, which raises SIGBUS when the disk is full. So we rewrite the
        // range block by block instead: existing content is read and written back
        // unchanged (recovery data is preserved), holes and the range beyond EOF
        // become zero-filled allocated blocks, and ENOSPC is reported here as a
        // normal error so the caller can fall back to heap memory.
        {
            char chunk[4096];
            size_t pos = 0;
            while (pos < real_min_file_size) {
                size_t chunk_size = bq::min_value(sizeof(chunk), real_min_file_size - pos);
                ssize_t read_bytes = pread(fd, chunk, chunk_size, (off_t)pos);
                if (read_bytes < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    // a real read error (e.g. EIO) would also fault later through the
                    // mapping, and writing zeros back would destroy recoverable data,
                    // so fail here and let the caller fall back to heap memory
                    result.error_code_ = errno;
                    bq::util::log_device_console(log_level::warning, "create_memory_map materialize file failed, path:%s, error_code:%" PRId32, map_file.abs_file_path().c_str(), result.error_code_);
                    return result;
                }
                // a short read only happens at EOF for regular files, pad with zeros
                if ((size_t)read_bytes < chunk_size) {
                    memset(chunk + read_bytes, 0, chunk_size - (size_t)read_bytes);
                }
                size_t written = 0;
                while (written < chunk_size) {
                    ssize_t write_bytes = pwrite(fd, chunk + written, chunk_size - written, (off_t)(pos + written));
                    if (write_bytes < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        result.error_code_ = errno;
                        bq::util::log_device_console(log_level::warning, "create_memory_map materialize file failed, path:%s, error_code:%" PRId32, map_file.abs_file_path().c_str(), result.error_code_);
                        return result;
                    }
                    written += (size_t)write_bytes;
                }
                pos += chunk_size;
            }
        }

        result.real_data_ = mmap(NULL, real_mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(real_mapping_offset));

        if (MAP_FAILED == result.real_data_) {
            result.error_code_ = errno;
            bq::util::log_device_console(log_level::error, "create_memory_map file failed, path:%s, error_code:%" PRId32, map_file.abs_file_path().c_str(), result.error_code_);
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
        if (0 != msync(handle.real_data_, *(const size_t*)handle.platform_data_, MS_SYNC)) {
            bq::util::log_device_console(log_level::error, "flush_memory_map file failed, error_code:%" PRId32, errno);
        }
    }

    void memory_map::release_memory_map(memory_map_handle& handle)
    {
        if (!handle.has_been_mapped()) {
            return;
        }
        if (0 != munmap(handle.real_data_, *(const size_t*)handle.platform_data_)) {
            bq::util::log_device_console(log_level::error, "release_memory_map file failed, error_code:%" PRId32, errno);
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