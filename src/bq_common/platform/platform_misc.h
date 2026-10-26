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
#pragma once

#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/fast_clock.h"
#if defined(BQ_POSIX)
#include "bq_common/platform/posix_misc.h"
#endif
#if defined(BQ_NAPI)
#include "bq_common/platform/napi_misc.h"
#endif
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
#if defined(BQ_OHOS)
#include "bq_common/platform/ohos_misc.h"
#endif
#if defined(BQ_PS) || defined(BQ_SWITCH)
#include "bq_common/platform/console_misc.h"
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

        // Only the handle that registered a file in the exclusive cache removes it on close.
        struct file_exclusive_entry {
            file_open_mode_enum mode_;
            platform_file_handle owner_;
        };

        struct base_dir_initializer {
        private:
            bq::string base_dir_0_;
            bq::string base_dir_1_;
            bool base_dir_0_set_ = false;
            bool base_dir_1_set_ = false;

        public:
            const bq::string& get_base_dir_0() const { return base_dir_0_; }
            const bq::string& get_base_dir_1() const { return base_dir_1_; }

            bool is_set(int32_t base_dir_type) const;

            void set_base_dir_0(const bq::string& dir);
            void set_base_dir_1(const bq::string& dir);

#if defined(BQ_UNIT_TEST)
            void test_clear_set_flag(int32_t base_dir_type);
#endif

            base_dir_initializer();
        };

        // Platform wall clock, implemented per platform.
        uint64_t system_epoch_ms();

        extern BQ_TLS fast_clock_thread_cache epoch_ms_cache_;

        // Hardware counter clock (fast_clock.h) where available, system_epoch_ms() otherwise.
        bq_forceinline uint64_t high_performance_epoch_ms()
        {
            uint64_t epoch_ms;
            BQ_LIKELY_IF(fast_clock_read_epoch_ms(epoch_ms_cache_, epoch_ms))
            {
                return epoch_ms;
            }
            return system_epoch_ms();
        }

        // Aborts when unset: a guessed path writes the log where nobody looks.
        bq::string get_base_dir(int32_t base_dir_type);

        bool try_get_base_dir(int32_t base_dir_type, bq::string& out_dir);

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

        uint64_t get_file_last_modified_epoch_ms(const char* path);

        bq::array<bq::string> get_all_sub_names(const char* path);

        enum class file_lock_result {
            locked,
            held_by_other,
            error,
        };

        // Whether a file can be locked until its open description closes, which keeps a second process out.
        bool is_cross_process_file_lock_supported();

        // Opens the file and locks it, then keeps the lock until the handle closes. The lock file is
        // never deleted: the lock lives in the kernel, so a stale file on disk locks nothing.
        file_lock_result try_lock_instance_file(const char* abs_path, platform_file_handle& out_handle);
        void unlock_instance_file(platform_file_handle& handle);

        bq::string get_lexically_path(const bq::string& original_path);

        bool is_absolute(const string& path);
        /// <summary>
        /// truncate file
        /// </summary>
        /// <param name="file_handle"></param>
        /// <param name="offset"></param>
        /// <returns>0 means success, otherwise means error code</returns>
        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset);

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count);
        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count);

        void* aligned_alloc(size_t alignment, size_t size);
        void aligned_free(void* ptr);

        // Commits every page of [addr, addr + size) without changing contents. No concurrent writers.
        void prefault_pages(void* addr, size_t size);

#if defined(BQ_UNIT_TEST)
        // Test-only fault-injection hooks for disk-full / OOM scenarios.
        // These let the unit tests reproduce ENOSPC and heap-exhaustion code paths
        // deterministically across all platforms without needing a real full disk.
        // Compiled out entirely in release builds.
        namespace test_inject {
            enum class fault_kind : int32_t {
                none = 0,
                enospc_on_open, // open_file() returns ENOSPC / ERROR_DISK_FULL
                enospc_on_write, // write_file() returns ENOSPC and writes zero bytes
            };
            void set_fault(fault_kind kind);
            fault_kind get_fault();

            // When a path filter is set, only paths that contain the filter substring
            // trigger the fault. nullptr / empty filter triggers on all paths.
            void set_path_filter(const char* substring);
            bool path_matches_filter(const char* path);

            // Independent OOM hook: makes bq::normal_buffer's heap fallback alloc
            // return null, simulating "mmap fails AND heap fails" on disk full + OOM.
            void set_normal_buffer_alloc_fail(bool fail);
            bool get_normal_buffer_alloc_fail();

            void set_base_dir_unset(bool unset);
        }
#endif
    }
}
