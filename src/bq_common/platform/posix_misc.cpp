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
#if defined(BQ_POSIX)
#include <pthread.h>
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
namespace bq {
    BQ_TLS_NON_POD(bq::string, stack_trace_current_str_);
    BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_);
}
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

        bq::string get_lexically_path(const bq::string& original_path)
        {
            bq::string result;
            result.set_capacity(original_path.size());
            bq::array<bq::string> split = original_path.split("/");
            bq::array<bq::string> result_split;
            result_split.set_capacity(split.size());
            for (decltype(split)::size_type i = 0; i < split.size(); ++i) {
                split[i] = split[i].trim();
                if (split[i] == "..") {
                    if (result_split.size() > 0 && result_split[result_split.size() - 1] != "..") {
                        result_split.pop_back();
                    } else {
                        result_split.push_back(split[i]);
                    }
                } else if (split[i] == ".") {

                } else {
                    result_split.push_back(split[i]);
                }
            }
            for (decltype(result_split)::size_type i = 0; i < result_split.size(); ++i) {
                if (i != 0) {
                    result += "/";
                }
                result += result_split[i];
            }
            if (result.is_empty()) {
                result = "./";
            }
            if (original_path.size() > 0 && original_path[0] == '/')
                result = "/" + result;

#if defined(BQ_MAC) || defined(BQ_LINUX) || defined(BQ_UNIX)
            if (result.size() > 0 && result[0] == '~') {
                if (result.size() == 1 || result[1] == '/') {
                    // Case: "~" Or "~/"
                    auto home_dir_c_str = getenv("HOME");
                    if (!home_dir_c_str) {
                        home_dir_c_str = getpwuid(getuid())->pw_dir;
                    }
                    result.erase(result.begin(), 1);
                    bq::string home_dir = home_dir_c_str;
                    if (!home_dir.is_empty() && home_dir[home_dir.size() - 1] == '/') {
                        home_dir.erase(home_dir.end() - 1);
                    }
                    result = home_dir + result;
                } else {
                    auto slash_index = result.find("/");
                    bq::string username = (slash_index == bq::string::npos) ? result.substr(1) : result.substr(1, (slash_index - 1));
                    struct passwd* pw = getpwnam(username.c_str());
                    if (!pw) {
                        bq::util::log_device_console(log_level::error, "unknown user:%s, when parsing path:%s", username.c_str(), original_path.c_str());
                    } else if (slash_index == bq::string::npos) {
                        result = pw->pw_dir;
                    } else {
                        bq::string pw_dir = pw->pw_dir;
                        if (!pw_dir.end_with("/")) {
                            result = pw_dir + result.substr(slash_index + 1);
                        } else {
                            result = pw_dir + result.substr(slash_index);
                        }
                    }
                }
            }
#endif
            return result;
        }

        bool is_absolute(const string& path)
        {
            return path.size() > 0 && (path[0] == '/' || path[0] == '~');
        }

        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset)
        {
            auto result = ftruncate(file_handle, (off_t)offset);
            if (result == 0) {
                return result;
            }
            return errno;
        }


        int32_t remove_dir_or_file_inner(bq::string& path)
        {
#ifdef BQ_PS
            // TODO
            return 0;
#else
            if (!is_dir(path.c_str())) {
                if (remove(path.c_str()) == 0) {
                    return 0;
                }
                return errno;
            } else {
                size_t path_init_size = path.size();
                auto dp = __posix_opendir(path.c_str());
                if (!dp) {
                    return errno;
                }
                dirent* dirp;
                while ((dirp = readdir(&dp)) != NULL) {
                    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
                        continue;
                    }

                    path.push_back('/');
                    path += dirp->d_name;
                    int32_t result = remove_dir_or_file_inner(path);
                    path.erase(path.begin() + path_init_size, path.size() - path_init_size);
                    if (result != 0) {
                        return result;
                    }
                }
                if (remove(path.c_str()) == 0) {
                    return 0;
                }
                return errno;
            }
#endif
        }

        int32_t remove_dir_or_file(const char* path)
        {
            bq::string path_str = get_lexically_path(path);
            if (path_str.is_empty()) {
                return 0;
            }
            return remove_dir_or_file_inner(path_str);
        }

        uint64_t file_node_info::hash_code() const
        {
            return bq::util::get_hash_64(this, sizeof(file_node_info));
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
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
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
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
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
        
        uint64_t get_file_last_modified_epoch_ms(const char* path)
        {
            bq::string abs_path = get_lexically_path(path);
            struct stat buf;
            int32_t result = stat(abs_path.c_str(), &buf);
            if (result == 0) {
                return (uint64_t)buf.st_mtime * 1000;
            }
            return (uint64_t)0;
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
        static constexpr size_t max_stack_size_ = 128;
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            stack_trace_str_ref.clear();
            size_t stack_count;
            void* buffer[max_stack_size_];
            char** stacks;
            using backtrace_stack_size_type = function_argument_type_t<decltype(&backtrace), 1>;
            using backtrace_symbols_stack_count_type = function_argument_type_t<decltype(&backtrace_symbols), 1>;
            auto stack_count = backtrace(buffer, static_cast<backtrace_stack_size_type>(max_stack_size_));
            stacks = backtrace_symbols(buffer, static_cast<backtrace_symbols_stack_count_type>(stack_count));
            uint32_t valid_frame_count = 0;
            if (stacks) {
                for (decltype(stack_count) i = 0; i < stack_count; i++) {
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
                    stack_trace_str_ref.push_back('\n');
                    stack_trace_str_ref.insert_batch(stack_trace_str_ref.end(), stacks[i], (size_t)str_len);
                }
            }
            free(stacks);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_u16_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::u16string& stack_trace_str_ref = bq::stack_trace_current_str_u16_.get();
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_str_ref.clear();
            stack_trace_str_ref.fill_uninitialized((u8_char_count << 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_str_ref.begin(), (uint32_t)stack_trace_str_ref.size());
            assert(encoded_size < stack_trace_str_ref.size());
            stack_trace_str_ref.erase(stack_trace_str_ref.begin() + encoded_size, stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }
#endif

        void* aligned_alloc(size_t alignment, size_t size)
        {
            void* p = nullptr;
            if (posix_memalign(&p, alignment, size) != 0) {
                return nullptr;
            }
            return p;
        }
        void aligned_free(void* ptr)
        {
            free(ptr);
        }
    }
}
#endif
