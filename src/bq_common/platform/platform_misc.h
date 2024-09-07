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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

#if defined(BQ_JAVA)
#include "bq_common/platform/java_misc.h"
#endif
#if defined(BQ_WIN)
#include "bq_common/platform/win64_misc.h"
#endif
#if defined(BQ_ANDROID)
#include "bq_common/platform/android_misc.h"
#endif
#if defined(BQ_LINUX)
#include "bq_common/platform/linux_misc.h"
#endif
#if defined(BQ_UNIX)
#include "bq_common/platform/unix_misc.h"
#endif
#if defined(BQ_MAC)
#include "bq_common/platform/mac_misc.h"
#endif
#if defined(BQ_IOS)
#include "bq_common/platform/ios_misc.h"
#endif
#if defined(BQ_PS)
#include "bq_common/platform/ps_misc.h"
#endif
#if defined(BQ_POSIX)
#include "bq_common/platform/posix_misc.h"
#endif

namespace bq {
    namespace platform {
        enum class file_open_mode_enum {
            read = 1,
            write = 1 << 1,
            read_write = read | write,
            auto_create = 1 << 2,
            exclusive = 1 << 3
        };
        enum class file_seek_option : uint8_t {
            current, // seek from the current file position
            begin, // seek from the beginning of file
            end // seek from end of file
        };
        bq_forceinline file_open_mode_enum operator|(file_open_mode_enum lhs, file_open_mode_enum rhs)
        {
            return static_cast<file_open_mode_enum>(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
        }
        bq_forceinline file_open_mode_enum operator&(file_open_mode_enum lhs, file_open_mode_enum rhs)
        {
            return static_cast<file_open_mode_enum>(static_cast<int32_t>(lhs) & static_cast<int32_t>(rhs));
        }

        // TODO optimize use TSC
        uint64_t high_performance_epoch_ms();

        const bq::string& get_base_dir(bool is_sandbox);

        int32_t get_file_size(const char* file_path, size_t& size_ref);

        int32_t get_file_size(const platform_file_handle& file_handle, size_t& size_ref);

        bool is_dir(const char* path);

        bool is_regular_file(const char* path);

        int32_t make_dir(const char* path);

        int32_t remove_dir_or_file(const char* path);

        int32_t open_file(const char* path, file_open_mode_enum mode, platform_file_handle& out_file_handle);

        int32_t close_file(platform_file_handle& in_out_file_handle);

        int32_t read_file(const platform_file_handle& file_handle, void* target_addr, size_t read_size, size_t& out_real_read_size);

        int32_t write_file(const platform_file_handle& file_handle, const void* src_addr, size_t write_size, size_t& out_real_write_size);

        int32_t seek_file(const platform_file_handle& file_handle, file_seek_option opt, int64_t offset);

        int32_t flush_file(const platform_file_handle& file_handle);

        bq::array<bq::string> get_all_sub_names(const char* path);

        bool lock_file(const platform_file_handle& file_handle); // make file write exclusive

        bool unlock_file(const platform_file_handle& file_handle); // undo file write exclusive

        /// <summary>
        /// truncate file
        /// </summary>
        /// <param name="file_handle"></param>
        /// <param name="offset"></param>
        /// <returns>0 means success, otherwise means error code</returns>
        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset);

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count);
        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count);
    }
}
