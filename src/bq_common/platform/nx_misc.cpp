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
#include "bq_common/platform/nx_misc.h"
// Nintendo Switch, official Nintendo SDK (nn:: APIs). All nn:: calls below
// follow the public open-ead/nnheaders declarations; they compile against the
// real SDK headers as well.
#if defined(BQ_SWITCH_NN)
#include "bq_common/bq_common.h"
#include <nn/fs.h>
#include <nn/os.h>
#include <stdlib.h>

namespace bq {
    namespace platform {
        // nn::fs IO is positional and keeps no file cursor, so the current
        // offset is tracked per open file here (seek_file/read_file/write_file
        // emulate the POSIX cursor semantics the rest of BqLog expects).
        struct nx_file_control_block {
            nn::fs::FileHandle handle;
            int64_t offset;
            uint64_t path_hash;
        };

        // nn::Result packs module/description bit-fields; keep the raw value
        // so device-console logs stay meaningful. Call only on failure.
        static int32_t nx_result_to_error_code(const nn::Result& result)
        {
            return static_cast<int32_t>(result.GetInnerValueForDebug());
        }

        uint64_t high_performance_epoch_ms()
        {
            // Monotonic system tick since boot (ps_misc CLOCK_MONOTONIC
            // precedent); used for log timestamps and performance intervals.
            uint64_t tick = nn::os::GetSystemTick().value;
            uint64_t freq = nn::os::GetSystemTickFrequency().value;
            return tick * 1000 / freq;
        }

        base_dir_initializer::base_dir_initializer()
        {
            // The only generally writable storage for an official-SDK title is
            // the SD card mount. Games can override it at runtime through
            // __api_reset_base_dir.
            set_base_dir_0("sdmc:/");
            set_base_dir_1("sdmc:/");
        }

        int32_t get_file_size(const char* file_path, size_t& size_ref)
        {
            size_ref = 0;
            nn::fs::FileHandle handle;
            nn::Result open_result = nn::fs::OpenFile(&handle, file_path, nn::fs::OpenMode_Read);
            if (open_result.IsFailure()) {
                return nx_result_to_error_code(open_result);
            }
            long nn_size = 0;
            nn::Result size_result = nn::fs::GetFileSize(&nn_size, handle);
            nn::fs::CloseFile(handle);
            if (size_result.IsFailure()) {
                return nx_result_to_error_code(size_result);
            }
            size_ref = static_cast<size_t>(nn_size);
            return 0;
        }

        int32_t get_file_size(const platform_file_handle& file_handle, size_t& size_ref)
        {
            size_ref = 0;
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            long nn_size = 0;
            nn::Result size_result = nn::fs::GetFileSize(&nn_size, file_handle->handle);
            if (size_result.IsFailure()) {
                return nx_result_to_error_code(size_result);
            }
            size_ref = static_cast<size_t>(nn_size);
            return 0;
        }

        bool is_dir(const char* path)
        {
            nn::fs::DirectoryEntryType entry_type;
            if (nn::fs::GetEntryType(&entry_type, path).IsFailure()) {
                return false;
            }
            return entry_type == nn::fs::DirectoryEntryType_Directory;
        }

        bool is_regular_file(const char* path)
        {
            nn::fs::DirectoryEntryType entry_type;
            if (nn::fs::GetEntryType(&entry_type, path).IsFailure()) {
                return false;
            }
            return entry_type == nn::fs::DirectoryEntryType_File;
        }

        static int32_t make_dir_recursive(char* path)
        {
            size_t path_len = strlen(path);
            if (path_len == 0) {
                return 0;
            }
            if (path[path_len - 1] == ':') {
                // Reached a mount root such as "sdmc:" which always exists.
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
            if (nn::fs::CreateDirectory(path).IsSuccess()) {
                return 0;
            }
            // CreateDirectory fails when the directory already exists.
            if (is_dir(path)) {
                return 0;
            }
            return EIO;
        }

        int32_t make_dir(const char* path)
        {
            if (is_dir(path)) {
                return 0;
            }
            // strdup is avoided: strict-ANSI libc modes hide its declaration.
            size_t path_len = strlen(path);
            char* path_cpy = (char*)malloc(path_len + 1);
            memcpy(path_cpy, path, path_len + 1);
            int32_t result = make_dir_recursive(path_cpy);
            free(path_cpy);
            return result;
        }

        bool lock_file(const platform_file_handle& file_handle)
        {
            // Horizon titles are single-process, so cross-process file locking
            // does not apply; in-process write exclusivity is enforced by
            // file_exclusive_cache_ in open_file().
            return is_platform_handle_valid(file_handle);
        }

        bool unlock_file(const platform_file_handle& file_handle)
        {
            // See lock_file(): file locking is a no-op success on Switch.
            return is_platform_handle_valid(file_handle);
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
            if (end_with_slash) {
                result += "/";
            }
            return result;
        }

        bool is_absolute(const string& path)
        {
            // Official-SDK absolute paths carry a mount-name prefix, e.g.
            // "sdmc:/...".
            return (path.size() > 0 && path[0] == '/') || (path.find(":/") != bq::string::npos);
        }

        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            nn::Result result = nn::fs::SetFileSize(file_handle->handle, static_cast<s64>(offset));
            if (result.IsFailure()) {
                return nx_result_to_error_code(result);
            }
            return 0;
        }

        int32_t remove_dir_or_file(const char* path)
        {
            bq::string path_str = get_lexically_path(path);
            if (path_str.is_empty()) {
                return 0;
            }
            nn::Result result;
            if (is_dir(path_str.c_str())) {
                result = nn::fs::DeleteDirectoryRecursively(path_str.c_str());
            } else {
                result = nn::fs::DeleteFile(path_str.c_str());
            }
            if (result.IsFailure()) {
                return nx_result_to_error_code(result);
            }
            return 0;
        }

        uint64_t file_node_info::hash_code() const
        {
            return path_hash;
        }

        static bool add_file_execlusive_check(const bq::string& lexical_path, file_open_mode_enum mode, uint64_t& out_path_hash)
        {
            out_path_hash = bq::util::get_hash_64(lexical_path.c_str(), lexical_path.size());
            if (!(int32_t)(mode & (file_open_mode_enum::write | file_open_mode_enum::exclusive))) {
                return true;
            }
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
            node_info.path_hash = out_path_hash;
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

        static void remove_file_execlusive_check(uint64_t path_hash)
        {
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
            node_info.path_hash = path_hash;
            file_exclusive_cache.erase(node_info);
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
            out_file_handle = invalid_platform_file_handle;
            int32_t nn_mode = 0;
            if ((mode & file_open_mode_enum::read_write) == file_open_mode_enum::read_write) {
                nn_mode = nn::fs::OpenMode_ReadWrite;
            } else if ((int32_t)(mode & file_open_mode_enum::read)) {
                nn_mode = nn::fs::OpenMode_Read;
            } else if ((int32_t)(mode & file_open_mode_enum::write)) {
                nn_mode = nn::fs::OpenMode_Write;
            }
            bq::string lexical_path = get_lexically_path(path);

            nn::fs::FileHandle nn_handle;
            nn::Result open_result = nn::fs::OpenFile(&nn_handle, lexical_path.c_str(), nn_mode);
            if (open_result.IsFailure() && (int32_t)(mode & file_open_mode_enum::auto_create)) {
                // CreateFile fails when the file already exists, so it is only
                // attempted after OpenFile failed.
                if (nn::fs::CreateFile(lexical_path.c_str(), 0).IsSuccess()) {
                    open_result = nn::fs::OpenFile(&nn_handle, lexical_path.c_str(), nn_mode);
                }
            }
            if (open_result.IsFailure()) {
                return nx_result_to_error_code(open_result);
            }
            uint64_t path_hash = 0;
            if (!add_file_execlusive_check(lexical_path, mode, path_hash)) {
                nn::fs::CloseFile(nn_handle);
                return EACCES;
            }
            platform_file_handle block = (platform_file_handle)malloc(sizeof(nx_file_control_block));
            new (block, bq::enum_new_dummy::dummy) nx_file_control_block();
            block->handle = nn_handle;
            block->offset = 0;
            block->path_hash = path_hash;
            out_file_handle = block;
            return 0;
        }

        int32_t close_file(platform_file_handle& in_out_file_handle)
        {
            if (!is_platform_handle_valid(in_out_file_handle)) {
                return EBADF;
            }
            remove_file_execlusive_check(in_out_file_handle->path_hash);
            nn::fs::CloseFile(in_out_file_handle->handle);
            in_out_file_handle->~nx_file_control_block();
            free(in_out_file_handle);
            in_out_file_handle = invalid_platform_file_handle;
            return 0;
        }

        int32_t read_file(const platform_file_handle& file_handle, void* target_addr, size_t read_size, size_t& out_real_read_size)
        {
            out_real_read_size = 0;
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            u64 bytes_read = 0;
            nn::Result result = nn::fs::ReadFile(&bytes_read, file_handle->handle,
                static_cast<s64>(file_handle->offset), target_addr, static_cast<u64>(read_size));
            if (result.IsFailure()) {
                return nx_result_to_error_code(result);
            }
            out_real_read_size = static_cast<size_t>(bytes_read);
            file_handle->offset += static_cast<int64_t>(bytes_read);
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
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            // nn::fs::WriteFile writes the whole range or fails; there is no
            // partial-write count. It also extends the file as needed.
            nn::Result result = nn::fs::WriteFile(file_handle->handle,
                static_cast<s64>(file_handle->offset), src_addr, static_cast<u64>(write_size),
                nn::fs::WriteOption::CreateOption(0));
            if (result.IsFailure()) {
                return nx_result_to_error_code(result);
            }
            out_real_write_size = write_size;
            file_handle->offset += static_cast<int64_t>(write_size);
            return 0;
        }

        int32_t seek_file(const platform_file_handle& file_handle, file_seek_option opt, int64_t offset)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            int64_t new_offset = 0;
            switch (opt) {
            case bq::platform::file_seek_option::current:
                new_offset = file_handle->offset + offset;
                break;
            case bq::platform::file_seek_option::begin:
                new_offset = offset;
                break;
            case bq::platform::file_seek_option::end: {
                long nn_size = 0;
                nn::Result size_result = nn::fs::GetFileSize(&nn_size, file_handle->handle);
                if (size_result.IsFailure()) {
                    return nx_result_to_error_code(size_result);
                }
                new_offset = static_cast<int64_t>(nn_size) + offset;
                break;
            }
            default:
                break;
            }
            if (new_offset < 0) {
                return EINVAL;
            }
            file_handle->offset = new_offset;
            return 0;
        }

        int32_t flush_file(const platform_file_handle& file_handle)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return EBADF;
            }
            nn::Result result = nn::fs::FlushFile(file_handle->handle);
            if (result.IsFailure()) {
                return nx_result_to_error_code(result);
            }
            return 0;
        }

        uint64_t get_file_last_modified_epoch_ms(const char* path)
        {
            // nn::fs exposes no modification-time query (GetFileTimeStampForDebug
            // is debug-only), so report "unknown". Callers treat 0 gracefully:
            // expiry cleanup is skipped and capacity sorting degrades to
            // enumeration order.
            (void)path;
            return (uint64_t)0;
        }

        bq::array<bq::string> get_all_sub_names(const char* path)
        {
            bq::array<bq::string> result;
            if (!path) {
                path = "./";
            }
            nn::fs::DirectoryHandle dir_handle;
            if (nn::fs::OpenDirectory(&dir_handle, path, nn::fs::OpenDirectoryMode_All).IsFailure()) {
                return result;
            }
            bq::array<nn::fs::DirectoryEntry> entries;
            entries.fill_uninitialized(16);
            while (true) {
                s64 entry_count = 0;
                nn::Result read_result = nn::fs::ReadDirectory(&entry_count, &entries[0], dir_handle, static_cast<s64>(entries.size()));
                if (read_result.IsFailure() || entry_count <= 0) {
                    break;
                }
                for (s64 i = 0; i < entry_count; ++i) {
                    result.push_back(entries[static_cast<size_t>(i)].mName);
                }
            }
            nn::fs::CloseDirectory(dir_handle);
            return result;
        }

        // The official SDK has no backtrace facility.
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char not_supported_str[] = "stack trace is not supported on this platform";
            out_str_ptr = not_supported_str;
            out_char_count = (uint32_t)(sizeof(not_supported_str) / sizeof(not_supported_str[0]) - 1);
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char16_t not_supported_str[] = u"stack trace is not supported on this platform";
            out_str_ptr = not_supported_str;
            out_char_count = (uint32_t)(sizeof(not_supported_str) / sizeof(not_supported_str[0]) - 1);
        }

        void* aligned_alloc(size_t alignment, size_t size)
        {
            if (alignment < sizeof(void*)) {
                alignment = sizeof(void*);
            }
            // The SDK's musl-derived libc provides C11 aligned_alloc, which
            // requires the size to be an integral multiple of the alignment.
            size = (size + alignment - 1) / alignment * alignment;
            return ::aligned_alloc(alignment, size);
        }

        void aligned_free(void* ptr)
        {
            ::free(ptr);
        }
    }
}
#endif
