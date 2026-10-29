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
// Defined before any include: glibc and musl expose F_OFD_SETLK only with it, newlib exposes POSIX with it under a strict -std.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "native_src_bq_common_platform_posix_misc.h"
#if defined(BQ_POSIX)
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#if BQ_HAS_CROSS_PROCESS_FILE_LOCK
#include <sys/file.h>
#endif
#if BQ_HAS_DIRENT
#include <dirent.h>
#endif
#if BQ_HAS_EXECINFO
#include <cxxabi.h>
#include <execinfo.h>
namespace bq {
    BQ_TLS_NON_POD(bq::string, stack_trace_current_str_)
    BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_)
}
#endif
#include "native_src_bq_common_bq_common.h"

namespace bq {
    namespace platform {
#if BQ_HAS_DIRENT
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
            posix_dir_stack_holder(const posix_dir_stack_holder&) = delete;
            posix_dir_stack_holder& operator=(const posix_dir_stack_holder&) = delete;
            posix_dir_stack_holder(posix_dir_stack_holder&& rhs) noexcept
                : dp(rhs.dp)
            {
                rhs.dp = nullptr;
            }
            posix_dir_stack_holder& operator=(posix_dir_stack_holder&& rhs) noexcept
            {
                if (this != &rhs) {
                    if (dp) {
                        closedir(dp);
                    }
                    dp = rhs.dp;
                    rhs.dp = nullptr;
                }
                return *this;
            }
            operator bool() const
            {
                return dp != nullptr;
            }
            DIR* get_dp()
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
            if (errno == EEXIST) {
                // Directory already exists
                return 0;
            }
            return errno;
        }

        int32_t make_dir(const char* path)
        {
            if (is_dir(path))
                return 0;
            bq::string path_cpy = path;
            if (path_cpy.is_empty()) {
                return 0;
            }
            return make_dir_recursive(const_cast<char*>(path_cpy.c_str()));
        }

        bool is_cross_process_file_lock_supported()
        {
#if !BQ_HAS_CROSS_PROCESS_FILE_LOCK || defined(BQ_TEST_DISABLE_FILE_LOCK)
            return false;
#elif defined(F_OFD_SETLK) || (defined(LOCK_EX) && defined(LOCK_NB))
            return true;
#else
            return false;
#endif
        }

        // Non-blocking lock owned by this open description: it conflicts with every other open of the
        // file, in this process or another, and only closing this description releases it.
        static file_lock_result try_lock_file(const platform_file_handle& file_handle)
        {
#if !BQ_HAS_CROSS_PROCESS_FILE_LOCK
            (void)file_handle;
            return file_lock_result::error;
#else
            if (!is_cross_process_file_lock_supported()) {
                return file_lock_result::error;
            }
#if defined(F_OFD_SETLK)
            struct flock fl;
            memset(&fl, 0, sizeof(fl));
            fl.l_type = F_WRLCK;
            fl.l_whence = SEEK_SET;
            if (::fcntl(file_handle, F_OFD_SETLK, &fl) == 0) {
                return file_lock_result::locked;
            }
            // EINVAL means this kernel cannot do OFD locks; fall through to flock
            if (errno != EINVAL) {
                const int err = errno;
                if (err == EAGAIN || err == EACCES || err == EWOULDBLOCK) {
                    return file_lock_result::held_by_other;
                }
                bq::util::log_device_console(log_level::warning, "file lock fcntl failed, fd:%" PRId32 ", errno:%d", file_handle, err);
                return file_lock_result::error;
            }
#endif
#if defined(LOCK_EX) && defined(LOCK_NB)
            if (::flock(file_handle, LOCK_EX | LOCK_NB) == 0) {
                return file_lock_result::locked;
            }
            // a filesystem without locks (NFS returns ENOLCK, some FUSE EOPNOTSUPP) is an error, not contention
            const int err = errno;
            if (err == EAGAIN || err == EACCES || err == EWOULDBLOCK) {
                return file_lock_result::held_by_other;
            }
            bq::util::log_device_console(log_level::warning, "file lock flock failed, fd:%" PRId32 ", errno:%d", file_handle, err);
            return file_lock_result::error;
#else
            bq::util::log_device_console(log_level::warning, "no cross-process file lock on this kernel: OFD locks or native flock required, fd:%" PRId32, file_handle);
            return file_lock_result::error;
#endif
#endif
        }

        file_lock_result try_lock_instance_file(const char* abs_path, platform_file_handle& out_handle)
        {
            out_handle = invalid_platform_file_handle;
#if !BQ_HAS_MULTI_PROCESS
            (void)abs_path;
            return file_lock_result::locked;
#elif !BQ_HAS_CROSS_PROCESS_FILE_LOCK
            (void)abs_path;
            return file_lock_result::error;
#else
            if (!is_cross_process_file_lock_supported()) {
                bq::util::log_device_console(log_level::warning, "cross-process file lock unsupported on this platform: %s", abs_path);
                return file_lock_result::error;
            }
            int32_t fd = ::open(abs_path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
            if (fd < 0) {
                bq::util::log_device_console(log_level::warning, "file lock open failed: %s, errno:%d", abs_path, errno);
                return file_lock_result::error;
            }
            const file_lock_result result = try_lock_file(fd);
            if (result != file_lock_result::locked) {
                ::close(fd);
                return result;
            }
            out_handle = fd;
            return result;
#endif
        }

        void unlock_instance_file(platform_file_handle& handle)
        {
            // closing the fd releases both OFD and flock locks
            close_descriptor(handle);
        }

        void close_descriptor(platform_file_handle& in_out_file_handle)
        {
            if (is_platform_handle_valid(in_out_file_handle)) {
                ::close(in_out_file_handle);
            }
            in_out_file_handle = invalid_platform_file_handle;
        }

        bool register_after_fork_child_handler(void (*handler)())
        {
#if BQ_HAS_FORK
            return ::pthread_atfork(nullptr, nullptr, handler) == 0;
#else
            (void)handler;
            return false;
#endif
        }

        bq::string get_lexically_path(const bq::string& original_path)
        {
            bq::string result;
            result.set_capacity(original_path.size());
            bool end_with_slash = (original_path.is_empty() ? false : (original_path[original_path.size() - 1] == '/'));
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
            if (end_with_slash) {
                result += "/";
            }
            return result;
        }

        bool is_absolute(const string& path)
        {
            if (path.size() > 0 && (path[0] == '/' || path[0] == '~')) {
                return true;
            }
            // Console mount points: sdmc:/, save:/, cache:/.
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                if (path[i] == '/') {
                    break;
                }
                if (path[i] == ':' && path[i + 1] == '/') {
                    return true;
                }
            }
            return false;
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
#if !BQ_HAS_DIRENT
            if (!is_dir(path.c_str())) {
                if (remove(path.c_str()) == 0) {
                    return 0;
                }
                return errno;
            }
            if (rmdir(path.c_str()) == 0) {
                return 0;
            }
            return ENOTSUP;
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
                while ((dirp = readdir(dp.get_dp())) != NULL) {
                    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
                        continue;
                    }

                    path.push_back('/');
                    path += dirp->d_name;
                    int32_t result = remove_dir_or_file_inner(path);
                    path.erase(path.begin() + static_cast<bq::string::difference_type>(path_init_size), path.size() - path_init_size);
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
            uint8_t buf[sizeof(dev) + sizeof(ino)];
            memcpy(buf, &dev, sizeof(dev));
            memcpy(buf + sizeof(dev), &ino, sizeof(ino));
            return bq::util::get_hash_64(buf, sizeof(buf));
        }

        static bool add_file_execlusive_check(const platform_file_handle& file_handle, file_open_mode_enum mode)
        {
            if (!(int32_t)(mode & (file_open_mode_enum::write | file_open_mode_enum::exclusive))) {
                return true;
            }
            struct stat file_info;
            if (fstat(file_handle, &file_info) < 0) {
                bq::util::log_device_console(log_level::error, "add_file_execlusive_check fstat failed, fd:%" PRId32 ", error code:%" PRId32, file_handle, errno);
                return false;
            }
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            file_node_info node_info;
            node_info.dev = file_info.st_dev;
            node_info.ino = file_info.st_ino;
            {
                bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
                auto iter = file_exclusive_cache.find(node_info);
                if (iter != file_exclusive_cache.end()) {
                    return !(int32_t)((iter->value().mode_ | mode) & file_open_mode_enum::exclusive);
                }
                file_exclusive_cache.add(node_info, file_exclusive_entry { mode, file_handle });
            }
            // Only an exclusive open has to keep the other processes out, and the cache above already
            // covers this one; a kernel that cannot lock is left with just the cache.
            if (!(int32_t)(mode & file_open_mode_enum::exclusive) || !is_cross_process_file_lock_supported()) {
                return true;
            }
            // retried because overlayfs (GitHub runners) reports a transient conflict now and then
            constexpr int32_t max_retry = 5;
            int32_t retry = 0;
            file_lock_result lock_result;
            while ((lock_result = try_lock_file(file_handle)) == file_lock_result::held_by_other && ++retry < max_retry) {
                bq::platform::thread::sleep(3);
            }
            if (lock_result == file_lock_result::locked) {
                return true;
            }
            bq::util::log_device_console(log_level::error, "add_file_execlusive_check lock failed, fd:%" PRId32, file_handle);
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_exclusive_cache.erase(node_info);
            return false;
        }
        static void remove_file_execlusive_check(const platform_file_handle& file_handle)
        {
            struct stat file_info;
            if (fstat(file_handle, &file_info) < 0) {
                bq::util::log_device_console(log_level::error, "remove_file_execlusive_check fstat failed, fd:%" PRId32 ", error code:%" PRId32, file_handle, errno);
                return;
            }
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
            node_info.dev = file_info.st_dev;
            node_info.ino = file_info.st_ino;
            auto iter = file_exclusive_cache.find(node_info);
            if (iter != file_exclusive_cache.end() && iter->value().owner_ == file_handle) {
                file_exclusive_cache.erase(iter);
            }
        }

        int32_t open_file(const char* path, file_open_mode_enum mode, platform_file_handle& out_file_handle)
        {
#if defined(BQ_UNIT_TEST)
            if (test_inject::get_fault() == test_inject::fault_kind::enospc_on_open
                && test_inject::path_matches_filter(path)) {
                out_file_handle = invalid_platform_file_handle;
                return ENOSPC;
            }
#endif
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
            // The exclusive lock lives on this open description, so a child that inherits the fd and
            // then execs would hold the file locked after this process is gone.
            flags |= O_CLOEXEC;

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
            size_t max_size_pertime = BQ_SSIZE_MAX;
            char* current_target = static_cast<char*>(target_addr);
            while (out_real_read_size < read_size) {
                size_t need_read_size_this_time = bq::min_value(max_size_pertime, read_size - out_real_read_size);
                ssize_t out_size = read(file_handle, current_target, need_read_size_this_time);
                if (out_size < 0) {
                    return errno;
                }
                out_real_read_size += static_cast<size_t>(out_size);
                current_target += out_size;
                if (out_size < static_cast<ssize_t>(need_read_size_this_time)) {
                    return 0;
                }
            }
            return 0;
        }

        int32_t write_file(const platform_file_handle& file_handle, const void* src_addr, size_t write_size, size_t& out_real_write_size)
        {
            out_real_write_size = 0;
#if defined(BQ_UNIT_TEST)
            if (test_inject::get_fault() == test_inject::fault_kind::enospc_on_write) {
                // Filter doesn't apply to write_file because we don't have a path here;
                // tests should scope by clearing the fault before/after the targeted op.
                return ENOSPC;
            }
#endif
            const char* current_src = static_cast<const char*>(src_addr);
            size_t remaining = write_size;

            while (remaining > 0) {
                size_t chunk_size = bq::min_value(remaining, BQ_SSIZE_MAX);
                ssize_t written = write(file_handle, current_src, chunk_size);
                if (written < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return errno;
                }
                out_real_write_size += static_cast<size_t>(written);
                current_src += static_cast<size_t>(written);
                remaining -= static_cast<size_t>(written);

                if (written == 0) {
                    // Maybe EOF
                    break;
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
#if !BQ_HAS_FDATASYNC
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
#if !BQ_HAS_DIRENT
            static bq::platform::atomic<int32_t> warn_once = { 0 };
            if (0 == warn_once.fetch_add_relaxed(1)) {
                bq::util::log_device_console(log_level::warning, "directory enumeration is unsupported on this platform: expire_time and capacity_limit are ignored");
            }
            (void)path;
            return bq::array<bq::string>();
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
            while ((dirp = readdir(dp.get_dp())) != NULL) {
                if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
                    continue;
                }
                const char* dp_name = dirp->d_name;
                result.push_back(dp_name);
            }
            return result;
#endif
        }

#if BQ_HAS_EXECINFO
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            stack_trace_str_ref.clear();
            void* buffer[128];
            char** stacks;
            auto stack_count = backtrace(buffer, 128);
            stacks = backtrace_symbols(buffer, stack_count);
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
                    stack_trace_str_ref.push_back('\n');
                    char* demangled_str = nullptr;
                    demangled_str = strstr(stacks[i], "_Z");
                    if (demangled_str) {
                        auto tail_mark = strchr(demangled_str, ' ');
                        if (tail_mark) {
                            tail_mark[0] = '\0';
                        }
                        tail_mark = strchr(demangled_str, '+');
                        if (tail_mark) {
                            tail_mark[0] = '\0';
                        }
                        int32_t status;
                        char* demangled = abi::__cxa_demangle(demangled_str, nullptr, nullptr, &status);
                        if (status == 0 && demangled) {
                            auto str_len = strlen(demangled);
                            stack_trace_str_ref.insert_batch(stack_trace_str_ref.end(), stacks[i], static_cast<size_t>(demangled_str - stacks[i]));
                            stack_trace_str_ref.insert_batch(stack_trace_str_ref.end(), demangled, (size_t)str_len);
                            free(demangled);
                        } else {
                            demangled_str = nullptr;
                        }
                    }
                    if (!demangled_str) {
                        auto str_len = strlen(stacks[i]);
                        stack_trace_str_ref.insert_batch(stack_trace_str_ref.end(), stacks[i], (size_t)str_len);
                    }
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
            stack_trace_str_ref.erase(stack_trace_str_ref.begin() + static_cast<bq::u16string::difference_type>(encoded_size), stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }
#elif !BQ_HAS_BACKTRACE
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char unsupported[] = "<stack trace unsupported on this platform>";
            out_str_ptr = unsupported;
            out_char_count = (uint32_t)(sizeof(unsupported) / sizeof(char) - 1);
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char16_t unsupported[] = u"<stack trace unsupported on this platform>";
            out_str_ptr = unsupported;
            out_char_count = (uint32_t)(sizeof(unsupported) / sizeof(char16_t) - 1);
        }
#endif

        void* aligned_alloc(size_t alignment, size_t size)
        {
            if (alignment < sizeof(void*)) {
                alignment = sizeof(void*);
            }
            void* p = nullptr;
#if BQ_HAS_POSIX_MEMALIGN
            if (posix_memalign(&p, alignment, size) != 0) {
                return nullptr;
            }
#else
            const size_t aligned_size = (size + alignment - 1) / alignment * alignment;
            p = ::aligned_alloc(alignment, aligned_size);
            if (!p) {
                return nullptr;
            }
#endif
            return p;
        }
        void aligned_free(void* ptr)
        {
            free(ptr);
        }
    }
}
#else
// Keep the translation unit non-empty where the platform has no POSIX layer at all.
typedef int bq_posix_misc_empty_tu;
#endif
