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
#include <stdio.h>
#include "bq_common/misc/assert.h"
#include "bq_common/platform/macros.h"
#include "bq_common/platform/platform_misc.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"
#include "bq_common/platform/thread/mutex.h"
#include "bq_common/platform/atomic/atomic.h"

namespace bq {
    struct file_handle {
        friend class file_manager;
        friend class memory_map;

    private:
        static constexpr uint32_t invalid_idx = 0xFFFFFFFF;
        uint32_t idx_;
        uint32_t seq_;
        bq::platform::platform_file_handle* handle_ptr_;
        bq::string file_path_;

    private:
        inline void clear()
        {
            idx_ = invalid_idx;
            seq_ = 0;
            handle_ptr_ = nullptr;
            file_path_.reset();
        }

    public:
        bq_forceinline bq::platform::platform_file_handle platform_handle() const
        {
            return *handle_ptr_;
        }
        file_handle()
        {
            clear();
        }

        file_handle(const file_handle& rhs);

        file_handle(file_handle&& rhs) noexcept;

        ~file_handle();

        file_handle& operator=(const file_handle& rhs);

        file_handle& operator=(file_handle&& rhs) noexcept;

        void invalid();

        // this is more strict but lower performance
        bool is_valid() const;

        bq_forceinline bool operator==(const file_handle& rhs) const
        {
            return idx_ == rhs.idx_ && seq_ == rhs.seq_;
        }

        // this is less strict but higher performance
        bq_forceinline operator bool() const
        {
            return idx_ != invalid_idx && seq_ > 0 && bq::platform::is_platform_handle_valid(platform_handle());
        }

        const bq::string& abs_file_path() const
        {
            return file_path_;
        }
    };
    enum class file_open_mode_enum {
        read = (int32_t)bq::platform::file_open_mode_enum::read,
        write = (int32_t)bq::platform::file_open_mode_enum::write,
        read_write = (int32_t)bq::platform::file_open_mode_enum::read_write,
        auto_create = (int32_t)bq::platform::file_open_mode_enum::auto_create,
        exclusive = (int32_t)bq::platform::file_open_mode_enum::exclusive
    };
    bq_forceinline file_open_mode_enum operator|(file_open_mode_enum lhs, file_open_mode_enum rhs)
    {
        return static_cast<file_open_mode_enum>(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
    }
    bq_forceinline file_open_mode_enum operator&(file_open_mode_enum lhs, file_open_mode_enum rhs)
    {
        return static_cast<file_open_mode_enum>(static_cast<int32_t>(lhs) & static_cast<int32_t>(rhs));
    }

    struct common_global_vars;

    class file_manager {
        friend file_handle;
        friend struct common_global_vars;

    private:
        struct file_descriptor {
            bq::platform::platform_file_handle* handle_ptr;
            uint32_t seq;
            bq::platform::atomic<int32_t> ref_cout;
            bq::string file_path;

            file_descriptor();

            bool is_empty() const;

            void inc_ref();

            void dec_ref();

            void clear();
        };

    private:
        file_manager();
        ~file_manager();

    public:
        enum class seek_option : uint8_t {
            current = (uint8_t)bq::platform::file_seek_option::current, // seek from the current file position
            begin = (uint8_t)bq::platform::file_seek_option::begin, // seek from the beginning of file
            end = (uint8_t)bq::platform::file_seek_option::end // seek from end of file
        };

    public:
        static file_manager& instance();

    public:
        // Functions with file path
        // tool functions IMPORTANT: all the path parameter is relative to platform::get_base_dir(path of the executable file on Win, Linux and Mac, app path on mobile devices)
        static bq::string get_base_dir(int32_t base_dir_type);

        static string trans_process_relative_path_to_absolute_path(const bq::string& relative_path, int32_t base_dir_type);
        static string trans_process_absolute_path_to_relative_path(const bq::string& absolute_path, int32_t base_dir_type);

        static bool create_directory(const bq::string& path);

        static bq::string get_directory_from_path(const bq::string& path);

        static bq::string get_file_name_from_path(const bq::string& path);

        static bq::string combine_path(const bq::string& path1, const bq::string& path2);

        static bq::string get_lexically_path(const bq::string& original_path);

        static bool is_absolute(const string& path);

        static bool is_dir(const bq::string& path);

        static bool is_file(const bq::string& path);

        static bool remove_file_or_dir(const bq::string& path);

        static bq::array<bq::string> get_sub_dirs_and_files_name(const bq::string& path);

        static void get_all_files(const string& path, bq::array<string>& out_list, bool recursion = true);
        // return 0 if file is not found.
        static uint64_t get_file_last_modified_epoch_ms(const bq::string& path);

        static size_t get_file_size(const bq::string& path);

        static string read_all_text(const bq::string& path);

        static void append_all_text(const bq::string& path, string content);

        static void write_all_text(const bq::string& path, string content);

        static int32_t get_and_clear_last_file_error();

    public:
        // Functions withs file file_handle.

        /// <summary>
        /// whether a handle is valid
        /// </summary>
        /// <param name="handle"></param>
        /// <returns></returns>
        bool is_handle_valid(const file_handle& handle) const;

        /// <summary>
        /// open a file in binary mode, and return file handle
        /// </summary>
        /// <param name="relative_path_to_base_dir">file path and name relative from the base directory</param>
        /// <returns>file handle, may be empty handle if file is not exist and auto_create is set to false, and also could be empty handle if file is already locked by another process</returns>
        /// @warning SEEK_END is used by default inside the function
        file_handle open_file(const bq::string& path, file_open_mode_enum open_mode);

        /// <summary>
        /// write file to current seek position
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <param name="data">data you want to write in</param>
        /// <param name="length">data length</param>
        /// <param name="seek_option"></param>
        /// <param name="seek_offset"></param>
        /// <returns>the number of objects written successfully, which may be less than count if an error occurs.</returns>
        size_t write_file(const file_handle& handle, const void* data, size_t length, seek_option opt = seek_option::current, int64_t seek_offset = 0);

        /// <summary>
        /// truncate file, and the file cursor will be at end of file after calling this
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <param name="offset">truncat offset, 0 will clear the content of file</param>
        /// <returns>true : success, false : failed</returns>
        bool truncate_file(const file_handle& handle, size_t offset);

        /// <summary>
        /// read data from file from current seek position, please make sure there is enough space in target data.
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <param name="dest_data">target data</param>
        /// <param name="length">expected read size</param>
        /// <param name="seek_option"></param>
        /// <param name="seek_offset"></param>
        /// <returns>real size read from file</returns>
        size_t read_file(const file_handle& handle, void* dest_data, size_t length, seek_option opt = seek_option::current, int64_t seek_offset = 0);

        /// <summary>
        /// get the absolute file path of a file handle
        /// </summary>
        /// <param name="handle"></param>
        /// <returns>is handle is invalid, an empty string will be returned</returns>
        string get_file_absolute_path(const file_handle& handle);

        /// <summary>
        /// seek position
        /// </summary>
        /// <param name="handle"></param>
        /// <param name="opt"></param>
        /// <param name="offset">only work when opt is not seek_option::do_not_seek</param>
        /// <returns></returns>
        bool seek(const file_handle& handle, seek_option opt, int64_t offset);

        /// <summary>
        /// set file write exclusive
        /// </summary>
        /// <param name="handle"></param>
        /// <returns>true if set success, false if file does not exist or has already be locked by another process</returns>
        bool lock_file(const file_handle& handle);

        /// <summary>
        /// read text data from file.
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <returns>content</returns>
        bq::string read_text(const file_handle& handle);

        /// <summary>
        /// flush file.
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <returns></returns>
        bool flush_file(const file_handle& handle);

        /// <summary>
        /// flush all the opened file synchronously, make sure they are written to file.
        /// </summary>
        /// <returns></returns>
        bool flush_all_opened_files();

        /// <summary>
        /// close a file handle, file is really closed only when all the handle is closed
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <returns>true means call this function without any error</returns>
        bool close_file(file_handle& handle);

        /// <summary>
        /// get size of file by file handle
        /// </summary>
        /// <param name="handle">file handle</param>
        /// <returns>file size. if return 0, it means file is empty, or file not exist, or file handle is closed</returns>
        size_t get_file_size(const file_handle& handle) const;

    private:
        int32_t get_file_descriptor_index_by_handle(const file_handle& handle) const;
        void inc_ref(const file_handle& handle);

    private:
        uint32_t seq_generator;

        array<file_descriptor> file_descriptors;

        bq::platform::mutex mutex;
    };
#ifndef TO_ABSOLUTE_PATH
#define TO_ABSOLUTE_PATH(path, base_dir_type) bq::file_manager::trans_process_relative_path_to_absolute_path(path, base_dir_type)
#endif
#ifndef TO_RELATIVE_PATH
#define TO_RELATIVE_PATH(path, base_dir_type) bq::file_manager::trans_process_absolute_path_to_relative_path(path, base_dir_type)
#endif
}
