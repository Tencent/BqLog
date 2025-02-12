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
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifndef BQ_PS
#include <dirent.h>
#endif
#if !defined(BQ_ANDROID) && !defined(BQ_IOS)
#include <execinfo.h>
#endif

namespace bq {
    namespace platform {
#ifdef BQ_PS
        // TODO
#else
        class posix_dir_stack_holder {
        private:
            DIR* dp = nullptr;

        public:
            posix_dir_stack_holder(DIR* in_dp)
                : dp(in_dp)
            {
            }
            ~posix_dir_stack_holder()
            {
                if (dp) {
                    closedir(dp);
                }
            }
            operator bool() const
            {
                return dp != nullptr;
            }
            DIR* operator&()
            {
                return dp;
            }
        };

        inline posix_dir_stack_holder __posix_opendir(const char* path)
        {
            return posix_dir_stack_holder(opendir(path));
        }
#endif

        int32_t get_file_size(const char* file_path, size_t& size_ref)
        {
            struct stat buf;
            size_ref = 0;
            int32_t result = stat(file_path, &buf);
            if (result == 0) {
                if (!S_ISREG(buf.st_mode)) {
                    return EISDIR;
                }
                size_ref = static_cast<size_t>(buf.st_size);
                return 0;
            }
            return errno;
        }

        int32_t get_file_size(const platform_file_handle& file_handle, size_t& size_ref)
        {
            struct stat buf;
            size_ref = 0;
            int32_t result = fstat(file_handle, &buf);
            if (result == 0) {
                if (!S_ISREG(buf.st_mode)) {
                    return EISDIR;
                }
                size_ref = static_cast<size_t>(buf.st_size);
                return 0;
            }
            return errno;
        }

        bool is_dir(const char* path)
        {
            struct stat buf;
            int32_t result = stat(path, &buf);
            if (result == 0) {
                if (S_ISDIR(buf.st_mode)) {
                    return true;
                }
            }
            return false;
        }

        bool is_regular_file(const char* path)
        {
            struct stat buf;
            int32_t result = stat(path, &buf);
            if (result == 0) {
                if (S_ISREG(buf.st_mode)) {
                    return true;
                }
            }
            return false;
        }

        static int32_t make_dir_recursive(char* path)
        {
            if (strlen(path) == 0) {
                return 0;
            }
            if (is_dir(path)) {
                return 0;
            }
            char* ptr = strrchr(path, '/');
            if (ptr != NULL) {
                *ptr = '\0';
                int32_t make_parent_result = make_dir_recursive(path);
                *ptr = '/';
                if (make_parent_result != 0) {
                    return make_parent_result;
                }
            }
            if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
                return 0;
            }
            return errno;
        }

        int32_t make_dir(const char* path)
        {
            if (is_dir(path))
                return 0;
            char* path_cpy = strdup(path);
            int32_t result = make_dir_recursive(path_cpy);
            free(path_cpy);
            return result;
        }

        bool lock_file(const platform_file_handle& file_handle)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return false;
            }
            struct flock fl;
            fl.l_type = F_WRLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = 0;
            fl.l_len = 0;
            fl.l_pid = getpid();

            if (fcntl(file_handle, F_SETLK, &fl) == -1) {
                return false;
            }
            return true;
        }

        bool unlock_file(const platform_file_handle& file_handle)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return false;
            }
            struct flock fl;
            fl.l_type = F_UNLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = 0;
            fl.l_len = 0;
            fl.l_pid = getpid();

            if (fcntl(file_handle, F_SETLK, &fl) == -1) {
                return false;
            }
            return true;
        }

        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset)
        {
            auto result = ftruncate(file_handle, (off_t)offset);
            if (result == 0) {
                return result;
            }
            return errno;
        }

        constexpr int32_t BQ_MAX_PATH = 255;

        int32_t remove_dir_or_file_inner(char* path, size_t cursor)
        {
#ifdef BQ_PS
            // TODO
            return 0;
#else
            if (!is_dir(path)) {
                if (remove(path) == 0) {
                    return 0;
                }
                return errno;
            } else {
                auto dp = __posix_opendir(path);
                if (!dp) {
                    return errno;
                }
                dirent* dirp;
                while ((dirp = readdir(&dp)) != NULL) {
                    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
                        continue;
                    }
                    size_t name_len = strlen(dirp->d_name);
                    size_t next_cursor = cursor + 1 + name_len;
                    if (next_cursor > BQ_MAX_PATH) {
                        return 0;
                    }
                    path[cursor] = '/';
                    memcpy(path + cursor + 1, dirp->d_name, name_len);
                    path[next_cursor] = '\0';
                    int32_t result = remove_dir_or_file_inner(path, next_cursor);
                    if (result != 0) {
                        return result;
                    }
                }
                path[cursor] = '\0';
                if (remove(path) == 0) {
                    return 0;
                }
                return errno;
            }
#endif
        }

        int32_t remove_dir_or_file(const char* path)
        {
            char temp_path[BQ_MAX_PATH + 1] = { '\0' };
            auto str_len = strlen(path);
            if (str_len == 0 || str_len > BQ_MAX_PATH) {
                return 0;
            }
            memcpy(temp_path, path, str_len);
            if (temp_path[str_len - 1] == '\\' || temp_path[str_len - 1] == '/') {
                temp_path[str_len - 1] = '\0';
                --str_len;
            }
            return remove_dir_or_file_inner(temp_path, str_len);
        }

        // File exclusive works well across different processes,
        // but mutual exclusion within the same process is not explicitly documented to function reliably across different system platforms.
        // To eliminate platform compatibility risks, we decided to implement it ourselves.
        BQ_STRUCT_PACK(struct posix_file_node_info {
            decltype(bq::declval<struct stat>().st_ino) ino;
            uint64_t hash_code() const
            {
                return bq::util::get_hash_64(this, sizeof(posix_file_node_info));
            }
            bool operator==(const posix_file_node_info& rhs) const
            {
                return ino == rhs.ino;
            }
        });

        static bq::hash_map<posix_file_node_info, file_open_mode_enum>& get_file_exclusive_cache()
        {
            static bq::hash_map<posix_file_node_info, file_open_mode_enum> file_exclusive_cache;
            return file_exclusive_cache;
        }

        static bq::platform::mutex& get_file_exclusive_mutex()
        {
            static bq::platform::mutex file_exclusive_mutex;
            return file_exclusive_mutex;
        }

        static bool add_file_execlusive_check(const platform_file_handle& file_handle, file_open_mode_enum mode)
        {
            if (!(int32_t)(mode & (file_open_mode_enum::write | file_open_mode_enum::exclusive))) {
                return true;
            }
            if ((int32_t)(mode & file_open_mode_enum::exclusive)) {
                struct flock lock;
                memset(&lock, 0, sizeof(lock));
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = 0;
                lock.l_len = 0;
                if (fcntl(file_handle, F_SETLK, &lock) == -1) {
                    return false;
                }
            }
            struct stat file_info;
            if (fstat(file_handle, &file_info) < 0) {
                bq::util::log_device_console(log_level::error, "add_file_execlusive_check fstat failed, fd:%d, error code:%d", file_handle, errno);
                return false;
            }
            bq::platform::mutex& file_exclusive_mutex = get_file_exclusive_mutex();
            bq::hash_map<posix_file_node_info, file_open_mode_enum>& file_exclusive_cache = get_file_exclusive_cache();
            bq::platform::scoped_mutex lock(file_exclusive_mutex);
            posix_file_node_info node_info;
            node_info.ino = file_info.st_ino;
            auto iter = file_exclusive_cache.find(node_info);
            if (iter == file_exclusive_cache.end()) {
                file_exclusive_cache.add(node_info, mode);
                return true;
            } else {
                if ((int32_t)((iter->value() | mode) & file_open_mode_enum::exclusive)) {
                    return false;
                }
            }
            return true;
        }
        static void remove_file_execlusive_check(const platform_file_handle& file_handle)
        {
            struct stat file_info;
            if (fstat(file_handle, &file_info) < 0) {
                bq::util::log_device_console(log_level::error, "remove_file_execlusive_check fstat failed, fd:%d, error code:%d", file_handle, errno);
                return;
            }
            bq::platform::mutex& file_exclusive_mutex = get_file_exclusive_mutex();
            bq::hash_map<posix_file_node_info, file_open_mode_enum>& file_exclusive_cache = get_file_exclusive_cache();
            bq::platform::scoped_mutex lock(file_exclusive_mutex);
            posix_file_node_info node_info;
            node_info.ino = file_info.st_ino;
            file_exclusive_cache.erase(node_info);
        }

        int32_t open_file(const char* path, file_open_mode_enum mode, platform_file_handle& out_file_handle)
        {
            int32_t flags = 0;
            if ((mode & file_open_mode_enum::read_write) == file_open_mode_enum::read_write) {
                flags |= O_RDWR;
            } else if ((int32_t)(mode & file_open_mode_enum::read)) {
                flags |= O_RDONLY;
            } else if ((int32_t)(mode & file_open_mode_enum::write)) {
                flags |= O_WRONLY;
            }
            if ((int32_t)(mode & file_open_mode_enum::auto_create)) {
                flags |= O_CREAT;
            }

            out_file_handle = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP);

            if (!is_platform_handle_valid(out_file_handle)) {
                return errno;
            }
            if (!add_file_execlusive_check(out_file_handle, mode)) {
                close(out_file_handle);
                out_file_handle = invalid_platform_file_handle;
                return EACCES;
            }
            return 0;
        }

        int32_t close_file(platform_file_handle& in_out_file_handle)
        {
            remove_file_execlusive_check(in_out_file_handle);
            auto result = close(in_out_file_handle);
            in_out_file_handle = invalid_platform_file_handle;
            if (result == 0) {
                return 0;
            }
            return errno;
        }

        int32_t read_file(const platform_file_handle& file_handle, void* target_addr, size_t read_size, size_t& out_real_read_size)
        {
            out_real_read_size = 0;
            size_t max_size_pertime = static_cast<size_t>(SSIZE_MAX);
            size_t max_read_size_current = 0;
            while (out_real_read_size < read_size) {
                size_t need_read_size_this_time = bq::min_value(max_size_pertime, read_size - max_read_size_current);
                ssize_t out_size = read(file_handle, target_addr, need_read_size_this_time);
                if (out_size < 0) {
                    return errno;
                }
                out_real_read_size += static_cast<size_t>(out_size);
                if (out_size < static_cast<ssize_t>(need_read_size_this_time)) {
                    return 0;
                }
            }
            return 0;
        }

        int32_t write_file(const platform_file_handle& file_handle, const void* src_addr, size_t write_size, size_t& out_real_write_size)
        {

            out_real_write_size = 0;
            size_t max_size_pertime = static_cast<size_t>(SSIZE_MAX);
            size_t max_write_size_current = 0;
            while (out_real_write_size < write_size) {
                size_t need_write_size_this_time = bq::min_value(max_size_pertime, write_size - max_write_size_current);
                ssize_t out_size = write(file_handle, src_addr, need_write_size_this_time);
                if (out_size < 0) {
                    return errno;
                }
                out_real_write_size += static_cast<size_t>(out_size);
                if (out_size < static_cast<ssize_t>(need_write_size_this_time)) {
                    return 0;
                }
            }
            return 0;
        }

        int32_t seek_file(const platform_file_handle& file_handle, file_seek_option opt, int64_t offset)
        {
            int32_t opt_platform = SEEK_CUR;
            switch (opt) {
            case bq::platform::file_seek_option::current:
                opt_platform = SEEK_CUR;
                break;
            case bq::platform::file_seek_option::begin:
                opt_platform = SEEK_SET;
                break;
            case bq::platform::file_seek_option::end:
                opt_platform = SEEK_END;
                break;
            default:
                break;
            }
            if (lseek(file_handle, static_cast<off_t>(offset), opt_platform) < 0) {
                auto error_code = errno;
                return error_code;
            }
            return 0;
        }

        int32_t flush_file(const platform_file_handle& file_handle)
        {
#if defined(BQ_IOS) || defined(BQ_MAC)
            if (fsync(file_handle) == 0) {
#else
            if (fdatasync(file_handle) == 0) {
#endif
                return 0;
            }
            return errno;
        }

        bq::array<bq::string> get_all_sub_names(const char* path)
        {
#ifdef BQ_PS
            bq::array<bq::string> result;
            return result;
#else
            bq::array<bq::string> result;
            if (!path) {
                path = "./";
            }
            auto dp = __posix_opendir(path);
            if (!dp) {
                return result;
            }
            dirent* dirp;
            while ((dirp = readdir(&dp)) != NULL) {
                if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
                    continue;
                }
                const char* dp_name = dirp->d_name;
                result.push_back(dp_name);
            }
            return result;
#endif
        }

#if !defined(BQ_ANDROID) && !defined(BQ_IOS)
        static thread_local bq::string stack_trace_current_str_;
        static thread_local bq::u16string stack_trace_current_str_u16_;
        static constexpr size_t max_stack_size_ = 128;
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            stack_trace_current_str_.clear();
            int32_t stack_count;
            void* buffer[max_stack_size_];
            char** stacks;

            stack_count = backtrace(buffer, max_stack_size_);
            stacks = backtrace_symbols(buffer, stack_count);
            uint32_t valid_frame_count = 0;
            if (stacks) {
                for (int32_t i = 0; i < stack_count; i++) {
                    if (valid_frame_count == 0) {
                        if (strstr(stacks[i], "get_stack_trace")) {
                            continue;
                        }
                        valid_frame_count = 1;
                    }
                    if (valid_frame_count++ <= skip_frame_count) {
                        continue;
                    }
                    auto str_len = strlen(stacks[i]);
                    stack_trace_current_str_.push_back('\n');
                    stack_trace_current_str_.insert_batch(stack_trace_current_str_.end(), stacks[i], (size_t)str_len);
                }
            }
            free(stacks);
            out_str_ptr = stack_trace_current_str_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_.size();
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_current_str_u16_.clear();
            stack_trace_current_str_u16_.fill_uninitialized((u8_char_count << 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_current_str_u16_.begin(), (uint32_t)stack_trace_current_str_u16_.size());
            assert(encoded_size < stack_trace_current_str_u16_.size());
            stack_trace_current_str_u16_.erase(stack_trace_current_str_u16_.begin() + encoded_size, stack_trace_current_str_u16_.size() - encoded_size);
            out_str_ptr = stack_trace_current_str_u16_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_u16_.size();
        }
#endif

        void init_for_file_manager()
        {
            get_file_exclusive_mutex();
        }
    }
}
#endif
