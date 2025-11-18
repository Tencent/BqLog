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
#include "bq_common/platform/win64_misc.h"
#if defined(BQ_WIN)
#include "bq_common/platform/win64_includes_begin.h"
#include <winternl.h>
#include <sys/stat.h>
#include <direct.h>
#include <fileapi.h>
#include <DbgHelp.h>
#include <wchar.h>
#include "bq_common/bq_common.h"

#ifdef BQ_VISUAL_STUDIO
#pragma comment(lib, "dbghelp.lib")
#else
#pragma message("Warning: DbgHelp.a is not linked without MSVC automatically. Please link it manually by -ldbghelp. ")
#endif

namespace bq {
    BQ_TLS_NON_POD(bq::string, stack_trace_current_str_)
    BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_)
    namespace platform {
        static bq::u16string trans_to_windows_wide_string(const bq::string utf8_str)
        {
            bq::u16string result;
            if (utf8_str.is_empty()) {
                return result;
            }
            result.fill_uninitialized(utf8_str.size() + 1);
            uint32_t trans_size = bq::util::utf8_to_utf16(utf8_str.c_str(), (uint32_t)utf8_str.size(), &result[0], (uint32_t)result.size());
            assert((trans_size < result.size()) && "trans_to_windows_wide_string error");
            result.erase(result.begin() + static_cast<ptrdiff_t>(trans_size), result.size() - ((size_t)trans_size));
            result = result.replace(u"/", u"\\");
            return result;
        }

        static bq::string force_to_abs_path(const bq::string path)
        {
            if (is_absolute(path)) {
                return path;
            }
            const bq::string& base_dir = get_base_dir(0);
            if (path.begin_with("\\") || path.begin_with("/")) {
                return base_dir + path.substr(1, path.size() - 1);
            } else {
                return base_dir + path;
            }
        }

        static bool get_stat_by_path(const bq::string& path, WIN32_FILE_ATTRIBUTE_DATA& buf)
        {
            bq::u16string file_path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            if (!GetFileAttributesExW((LPCWSTR)file_path_w.c_str(), GetFileExInfoStandard, &buf)) {
                return false;
            }
            return true;
        }

        // TODO optimize use TSC
        uint64_t high_performance_epoch_ms()
        {
            FILETIME ft;
            LARGE_INTEGER li;

            /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (ANSI UTC) and copy it
             * to a LARGE_INTEGER structure. */
            GetSystemTimeAsFileTime(&ft);
            li.LowPart = static_cast<decltype(li.LowPart)>(ft.dwLowDateTime);
            li.HighPart = static_cast<decltype(li.HighPart)>(ft.dwHighDateTime);

            uint64_t ret = static_cast<uint64_t>(li.QuadPart);
            const uint64_t UNIX_TIME_START = 0x019DB1DED53E8000; // January 1, 1970 (start of Unix epoch) in "ticks", difference from ANSI UTC to Unix Epoch.
            const uint64_t TICKS_PER_SECOND = 10000; // a tick is 100ns

            ret -= UNIX_TIME_START; /* Convert from file time to UNIX epoch time. */
            ret /= TICKS_PER_SECOND; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */
            return ret;
        }

        base_dir_initializer::base_dir_initializer()
        {
            static_assert(sizeof(char16_t) == sizeof(WCHAR), "WCHAR must be 16bits on WIndows Platform!");
            const char16_t* wpath = (const char16_t*)_wgetcwd(nullptr, 0);
            uint32_t wpath_len = (uint32_t)wcslen((LPCWSTR)wpath);
            bq::string base_dir;
            base_dir.fill_uninitialized((size_t)wpath_len * 3 + 2);
            size_t utf8_len = (size_t)bq::util::utf16_to_utf8(wpath, wpath_len, base_dir.begin(), (uint32_t)base_dir.size());
            assert(utf8_len < base_dir.size() && "base_dir utf16_to_utf8 size error!");
            base_dir.erase(base_dir.begin() + static_cast<ptrdiff_t>(utf8_len), base_dir.size() - utf8_len);
            set_base_dir_0(base_dir);
            set_base_dir_1(base_dir);
        }

        int32_t get_file_size(const char* file_path, size_t& size_ref)
        {
            bq::u16string file_path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(file_path)));
            HANDLE file = CreateFileW(
                (LPCWSTR)file_path_w.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
            if (file == INVALID_HANDLE_VALUE) {
                return ERROR_FILE_NOT_FOUND;
            }
            int32_t result = get_file_size(file, size_ref);
            CloseHandle(file);
            return result;
        }

        int32_t get_file_size(const platform_file_handle& file_handle, size_t& size_ref)
        {
            size_ref = 0;
            LARGE_INTEGER file_size;
            if (!GetFileSizeEx(file_handle, &file_size)) {
                return static_cast<int32_t>(GetLastError());
            }
            size_ref = static_cast<size_t>(file_size.QuadPart);
            return 0;
        }

        bool is_dir(const char* path)
        {
            WIN32_FILE_ATTRIBUTE_DATA buf;
            if (get_stat_by_path(path, buf)) {
                if (buf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    return true;
                }
            }
            return false;
        }

        bool is_regular_file(const char* path)
        {
            WIN32_FILE_ATTRIBUTE_DATA buf;
            if (get_stat_by_path(path, buf)) {
                if (!(buf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(buf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                    return true;
                }
            }
            return false;
        }

        static int32_t make_dir_recursive(char16_t* path)
        {
            constexpr size_t prefix_size = 2; // u"\\\\?\\"
            if ((wcslen((LPCWSTR)path) == prefix_size + 2) && path[prefix_size + 1] == ':') {
                return 0;
            }
            WIN32_FILE_ATTRIBUTE_DATA buf;
            if (GetFileAttributesExW((LPCWSTR)path, GetFileExInfoStandard, &buf)) {
                if (buf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    return 0;
                }
            }

            LPWSTR ptr = wcsrchr((LPWSTR)path, '\\');
            if (ptr != NULL) {
                *ptr = u'\0';
                int32_t make_parent_result = make_dir_recursive(path);
                *ptr = u'\\';
                if (make_parent_result != 0) {
                    return make_parent_result;
                }
            }
            if (_wmkdir((LPCWSTR)path) == 0) {
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
            bq::u16string path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            int32_t result = make_dir_recursive(path_w.begin());
            return result;
        }

        bool lock_file(const platform_file_handle& file_handle)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return false;
            }
            return LockFile(file_handle, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
        }

        bool unlock_file(const platform_file_handle& file_handle)
        {
            if (!is_platform_handle_valid(file_handle)) {
                return false;
            }
            return UnlockFile(file_handle, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
        }

        bq::string get_lexically_path(const bq::string& original_path)
        {
            bq::string result;
            result.set_capacity(original_path.size());
            bq::string trans_path = original_path.replace("\\", "/");
            bool end_with_slash = (trans_path.is_empty() ? false : (trans_path[trans_path.size() - 1] == '/'));
            bq::array<bq::string> split = trans_path.split("/");
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
            char first_char = path.is_empty() ? '\0' : path[0];
            if (first_char == '/' || first_char == '\\') {
                return true;
            }
            if (path.size() >= 2 && path[1] == ':' && ((first_char >= 'A' && first_char <= 'Z') || (first_char >= 'a' && first_char <= 'z'))) {
                return true;
            }
            return false;
        }

        int32_t remove_dir_or_file_inner(bq::u16string& path)
        {
            WIN32_FILE_ATTRIBUTE_DATA buf;
            if (!GetFileAttributesExW((LPCWSTR)path.c_str(), GetFileExInfoStandard, &buf)) {
                DWORD err = GetLastError();
                return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? ENOENT : EACCES;
            }

            if (!(buf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                DWORD attr = GetFileAttributesW((LPCWSTR)path.c_str());
                attr &= static_cast<DWORD>(~FILE_ATTRIBUTE_READONLY);
                SetFileAttributesW((LPCWSTR)path.c_str(), attr);
                int32_t result = _wremove((LPCWSTR)path.c_str());
                if (result != 0) {
                    return errno;
                }
                return 0;
            } else {
                size_t path_init_size = path.size();
                path.push_back(u'\\');
                path.push_back(u'*');
                WIN32_FIND_DATAW find_data;
                HANDLE h_file;
                h_file = FindFirstFileW((LPCWSTR)path.c_str(), &find_data);
                path.erase(path.end() - 2, 2); // u"\\*"
                if (h_file == INVALID_HANDLE_VALUE) {
                    // it's empty
                } else {
                    if (wcscmp(find_data.cFileName, (LPCWSTR)u".") == 0 || wcscmp(find_data.cFileName, (LPCWSTR)u"..") == 0) {
                        // ignore
                    } else {
                        path.push_back(u'\\');
                        path += (const char16_t*)find_data.cFileName;
                        int32_t result = remove_dir_or_file_inner(path);
                        path.erase(path.begin() + static_cast<ptrdiff_t>(path_init_size), path.size() - path_init_size);
                        if (result != 0) {
                            return result;
                        }
                    }
                    while (FindNextFileW(h_file, &find_data)) {
                        if (wcscmp(find_data.cFileName, (LPCWSTR)u".") == 0 || wcscmp(find_data.cFileName, (LPCWSTR)u"..") == 0) {
                            // ignore
                            continue;
                        }
                        path.push_back(u'\\');
                        path += (const char16_t*)find_data.cFileName;
                        int32_t result = remove_dir_or_file_inner(path);
                        path.erase(path.begin() + static_cast<ptrdiff_t>(path_init_size), path.size() - path_init_size);
                        if (result != 0) {
                            return result;
                        }
                    }
                }
                DWORD attr = GetFileAttributesW((LPCWSTR)path.c_str());
                attr &= static_cast<DWORD>(~FILE_ATTRIBUTE_READONLY);
                SetFileAttributesW((LPCWSTR)path.c_str(), attr);
                if (!RemoveDirectoryW((LPCWSTR)path.c_str())) {
                    return static_cast<int32_t>(GetLastError());
                }
                return 0;
            }
        }

        int32_t remove_dir_or_file(const char* path)
        {
            bq::u16string path_w = trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            if (path_w.is_empty()) {
                return 0;
            }
            path_w = u"\\\\?\\" + path_w;
            return remove_dir_or_file_inner(path_w);
        }

        static bool add_file_execlusive_check(const platform_file_handle& file_handle, file_open_mode_enum mode)
        {
            if (!(int32_t)(mode & (file_open_mode_enum::write | file_open_mode_enum::exclusive))) {
                return true;
            }
            BY_HANDLE_FILE_INFORMATION file_info;
            if (!GetFileInformationByHandle(file_handle, &file_info)) {
                bq::util::log_device_console(log_level::error, "add_file_execlusive_check GetFileInformationByHandle failed");
                return false;
            }
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
            node_info.volumn = static_cast<uint32_t>(file_info.dwVolumeSerialNumber);
            node_info.idx_high = static_cast<uint32_t>(file_info.nFileIndexHigh);
            node_info.idx_low = static_cast<uint32_t>(file_info.nFileIndexLow);
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
            BY_HANDLE_FILE_INFORMATION file_info;
            if (!GetFileInformationByHandle(file_handle, &file_info)) {
                bq::util::log_device_console(log_level::error, "remove_file_execlusive_check GetFileInformationByHandle failed");
                return;
            }
            auto& file_exclusive_cache = common_global_vars::get().file_exclusive_cache_;
            bq::platform::scoped_mutex lock(common_global_vars::get().file_exclusive_mutex_);
            file_node_info node_info;
            node_info.volumn = static_cast<uint32_t>(file_info.dwVolumeSerialNumber);
            node_info.idx_high = static_cast<uint32_t>(file_info.nFileIndexHigh);
            node_info.idx_low = static_cast<uint32_t>(file_info.nFileIndexLow);
            file_exclusive_cache.erase(node_info);
        }

        int32_t open_file(const char* path, file_open_mode_enum mode, platform_file_handle& out_file_handle)
        {
            bq::u16string file_path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            out_file_handle = CreateFileW((LPCWSTR)file_path_w.c_str(), ((int32_t)(mode & file_open_mode_enum::read) ? GENERIC_READ : 0) | ((int32_t)(mode & file_open_mode_enum::write) ? GENERIC_WRITE : 0), ((int32_t)(mode & file_open_mode_enum::exclusive) ? FILE_SHARE_READ : (FILE_SHARE_READ | FILE_SHARE_WRITE)), NULL, ((int32_t)(mode & file_open_mode_enum::auto_create) ? OPEN_ALWAYS : OPEN_EXISTING), FILE_ATTRIBUTE_NORMAL, NULL);
            if (!is_platform_handle_valid(out_file_handle)) {
                return static_cast<int32_t>(GetLastError());
            }
            if (!add_file_execlusive_check(out_file_handle, mode)) {
                CloseHandle(out_file_handle);
                out_file_handle = invalid_platform_file_handle;
                return ERROR_SHARING_VIOLATION;
            }
            return 0;
        }

        int32_t close_file(platform_file_handle& in_out_file_handle)
        {
            remove_file_execlusive_check(in_out_file_handle);
            bool result = CloseHandle(in_out_file_handle);
            in_out_file_handle = invalid_platform_file_handle;
            if (result) {
                return 0;
            }
            return static_cast<int32_t>(GetLastError());
        }

        int32_t read_file(const platform_file_handle& file_handle, void* target_addr, size_t read_size, size_t& out_real_read_size)
        {
            out_real_read_size = 0;
            size_t max_size_pertime = static_cast<size_t>(UINT32_MAX);
            size_t max_read_size_current = 0;
            while (out_real_read_size < read_size) {
                size_t need_read_size_this_time = bq::min_value(max_size_pertime, read_size - max_read_size_current);
                DWORD out_size = 0;
                bool result = ReadFile(file_handle, target_addr, static_cast<DWORD>(need_read_size_this_time), &out_size, NULL);
                if (!result) {
                    return static_cast<int32_t>(GetLastError());
                }
                out_real_read_size += static_cast<size_t>(out_size);
                if (out_size != need_read_size_this_time) {
                    return 0;
                }
            }
            return 0;
        }

        int32_t write_file(const platform_file_handle& file_handle, const void* src_addr, size_t write_size, size_t& out_real_write_size)
        {
            out_real_write_size = 0;
            const char* current_src = static_cast<const char*>(src_addr);
            size_t remaining = write_size;

            while (remaining > 0) {
                DWORD chunk_size = static_cast<DWORD>(bq::min_value(remaining, static_cast<size_t>(UINT32_MAX)));
                DWORD bytes_written = 0;
                BOOL result = WriteFile(file_handle, current_src, chunk_size, &bytes_written, NULL);
                if (!result) {
                    return static_cast<int32_t>(GetLastError());
                }
                out_real_write_size += bytes_written;
                current_src += bytes_written;
                remaining -= bytes_written;

                if (bytes_written == 0) {
                    // Maybe EOF
                    break;
                }
            }
            return 0;
        }

        int32_t seek_file(const platform_file_handle& file_handle, file_seek_option opt, int64_t offset)
        {
            LONG offset_low = (LONG)(offset & 0xFFFFFFFF);
            LONG offset_high = (LONG)(offset >> 32);
            DWORD opt_platform = FILE_CURRENT;
            switch (opt) {
            case bq::platform::file_seek_option::current:
                opt_platform = FILE_CURRENT;
                break;
            case bq::platform::file_seek_option::begin:
                opt_platform = FILE_BEGIN;
                break;
            case bq::platform::file_seek_option::end:
                opt_platform = FILE_END;
                break;
            default:
                break;
            }
            if (INVALID_SET_FILE_POINTER == SetFilePointer(file_handle, offset_low, &offset_high, opt_platform)) {
                auto error_code = static_cast<int32_t>(GetLastError());
                return error_code;
            }
            return 0;
        }

        int32_t flush_file(const platform_file_handle& file_handle)
        {
            if (FlushFileBuffers(file_handle)) {
                return 0;
            }
            return static_cast<int32_t>(GetLastError());
        }

        uint64_t get_file_last_modified_epoch_ms(const char* path)
        {
            bq::u16string file_path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            WIN32_FILE_ATTRIBUTE_DATA attr_data;
            if (!GetFileAttributesExW((LPCWSTR)file_path_w.c_str(), GetFileExInfoStandard, &attr_data)) {
                return 0; // Failed to get attributes
            }

            // Check if file exists and is accessible
            if (attr_data.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
                return 0; // File doesn't exist or is inaccessible
            }

            // Convert FILETIME to epoch milliseconds
            ULARGE_INTEGER uli;
            uli.LowPart = attr_data.ftLastWriteTime.dwLowDateTime;
            uli.HighPart = attr_data.ftLastWriteTime.dwHighDateTime;

            // FILETIME is in 100-nanosecond intervals since Jan 1, 1601 (UTC)
            // Convert to milliseconds since Unix epoch (Jan 1, 1970)
            uint64_t epoch_ms = uli.QuadPart / 10000; // Convert to milliseconds
            epoch_ms -= 11644473600000ULL; // Subtract milliseconds from 1601 to 1970
            return epoch_ms;
        }

        int32_t truncate_file(const platform_file_handle& file_handle, size_t offset)
        {
            LONG offset_low = (LONG)(static_cast<int64_t>(offset) & 0xFFFFFFFF);
            LONG offset_high = (LONG)(static_cast<int64_t>(offset) >> 32);
            if (INVALID_SET_FILE_POINTER == SetFilePointer(file_handle, offset_low, &offset_high, FILE_BEGIN)) {
                auto error_code = static_cast<int32_t>(GetLastError());
                return error_code;
            }
            if (!SetEndOfFile(file_handle)) {
                auto error_code = static_cast<int32_t>(GetLastError());
                return error_code;
            }
            return 0;
        }

        bq::array<bq::string> get_all_sub_names(const char* path)
        {
            bq::u16string file_path_w = u"\\\\?\\" + trans_to_windows_wide_string(force_to_abs_path(get_lexically_path(path)));
            bq::array<bq::string> list;
            WIN32_FIND_DATAW find_data;
            HANDLE h_file;
            h_file = FindFirstFileW((LPCWSTR)((file_path_w + u"\\*").c_str()), &find_data);
            if (h_file == INVALID_HANDLE_VALUE) {
                // it's empty
            } else {
                if (wcscmp(find_data.cFileName, (LPCWSTR)u".") == 0 || wcscmp(find_data.cFileName, (LPCWSTR)u"..") == 0) {
                    // ignore
                } else {
                }
                while (FindNextFileW(h_file, &find_data)) {
                    if (wcscmp(find_data.cFileName, (LPCWSTR)u".") == 0 || wcscmp(find_data.cFileName, (LPCWSTR)u"..") == 0) {
                        // ignore
                        continue;
                    }
                    char name_utf8_tmp[MAX_PATH * 3 + 2];
                    uint32_t trans_len = bq::util::utf16_to_utf8((const char16_t*)find_data.cFileName, (uint32_t)wcslen(find_data.cFileName), name_utf8_tmp, sizeof(name_utf8_tmp));
                    assert(trans_len < (uint32_t)sizeof(name_utf8_tmp) && "get_all_sub_names bq::util::utf16_to_utf8 size error");
                    name_utf8_tmp[(size_t)trans_len] = u'\0';
                    list.push_back(name_utf8_tmp);
                }
            }
            return list;
        }

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            const char16_t* u16_str;
            uint32_t u16_str_len;
            get_stack_trace_utf16(skip_frame_count, u16_str, u16_str_len);
            stack_trace_str_ref.clear();
            stack_trace_str_ref.fill_uninitialized(((u16_str_len * 3) >> 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf16_to_utf8(u16_str, u16_str_len, stack_trace_str_ref.begin(), (uint32_t)stack_trace_current_str_.get().size());
            assert(encoded_size < stack_trace_str_ref.size());
            stack_trace_str_ref.erase(stack_trace_current_str_.get().begin() + static_cast<ptrdiff_t>(encoded_size), stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_current_str_.get().size();
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_u16_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::u16string& stack_trace_str_ref = bq::stack_trace_current_str_u16_.get();
            HANDLE thread = GetCurrentThread();
            CONTEXT context;

            if (!common_global_vars::get().stack_trace_sym_initialized_.exchange(true, bq::platform::memory_order::relaxed)) {
                SymInitialize(common_global_vars::get().stack_trace_process_, NULL, TRUE);
            }
            stack_trace_str_ref.clear();
            bq::platform::scoped_mutex lock(common_global_vars::get().stack_trace_mutex_);
            RtlCaptureContext(&context);

            STACKFRAME64 stack;
            memset(&stack, 0, sizeof(STACKFRAME64));
            DWORD machine_type;

#ifdef BQ_ARM_64
            machine_type = IMAGE_FILE_MACHINE_ARM64;
            stack.AddrPC.Offset = context.Pc;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Sp;
            stack.AddrStack.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Fp;
            stack.AddrFrame.Mode = AddrModeFlat;
#elif defined(BQ_ARM_32)
            machine_type = IMAGE_FILE_MACHINE_ARM;
            stack.AddrPC.Offset = context.Pc;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Sp;
            stack.AddrStack.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.R11;
            stack.AddrFrame.Mode = AddrModeFlat;
#elif defined(BQ_X86_64)
            machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack.AddrPC.Offset = context.Rip;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Rsp;
            stack.AddrStack.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Rbp;
            stack.AddrFrame.Mode = AddrModeFlat;
#else
            static_assert(false, "Unsupported architecture on Windows");
#endif

            bool sym_refreshed = false;
            bool effective_stack_started = false;
            uint32_t current_frame_idx = 0;
            while (StackWalk64(
                machine_type,
                common_global_vars::get().stack_trace_process_,
                thread,
                &stack,
                &context,
                NULL,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                NULL)) {
                static_assert(sizeof(void*) == 8, "Only 64bit windows is supported");

                DWORD64 address = stack.AddrPC.Offset;
                char buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(WCHAR)];
                PSYMBOL_INFOW symbol = (PSYMBOL_INFOW)buffer;
                symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
                symbol->MaxNameLen = MAX_SYM_NAME;
                DWORD64 displacement64 = 0;
                bool symbo_found = SymFromAddrW(common_global_vars::get().stack_trace_process_, address, &displacement64, symbol);
                if (!symbo_found && !sym_refreshed) {
                    int32_t error_code = static_cast<int32_t>(GetLastError());
                    (void)error_code;
                    sym_refreshed = true;
                    SymRefreshModuleList(common_global_vars::get().stack_trace_process_);
                    symbo_found = SymFromAddrW(common_global_vars::get().stack_trace_process_, address, &displacement64, symbol);
                }
                if (symbo_found) {
                    if (!effective_stack_started) {
                        if (wcsstr(symbol->Name, L"bq::platform::get_stack_trace") == symbol->Name
                            || wcsstr(symbol->Name, L"bq::api::__api_get_stack_trace") == symbol->Name) {
                            continue;
                        }
                        effective_stack_started = true;
                    }
                }
                if (effective_stack_started && (++current_frame_idx <= skip_frame_count)) {
                    continue;
                }

                stack_trace_str_ref.push_back(u'\n');
                stack_trace_str_ref.fill_uninitialized(16);
                static_assert(sizeof(wchar_t) == sizeof(WCHAR) && sizeof(WCHAR) == sizeof(char16_t), "windows wchar_t should be 2 bytes");
                swprintf((wchar_t*)(char16_t*)stack_trace_str_ref.end() - 16, 16 + 1, L"%016" PRIx64 "", (uintptr_t)address);
                stack_trace_str_ref.push_back(L'\t');

                DWORD64 module_base = SymGetModuleBase64(common_global_vars::get().stack_trace_process_, address);
                if (!module_base && !sym_refreshed) {
                    sym_refreshed = true;
                    SymRefreshModuleList(common_global_vars::get().stack_trace_process_);
                    module_base = SymGetModuleBase64(common_global_vars::get().stack_trace_process_, address);
                }

                if (module_base) {
                    HMODULE h_module = (HMODULE)module_base;
                    constexpr DWORD default_max_module_name_len = 1024;
                    wchar_t module_file_name[default_max_module_name_len];
                    const wchar_t* module_file_name_ptr = module_file_name;
                    DWORD get_module_length = GetModuleFileNameW(h_module, module_file_name, default_max_module_name_len);
                    if (!get_module_length && !sym_refreshed) {
                        sym_refreshed = true;
                        SymRefreshModuleList(common_global_vars::get().stack_trace_process_);
                        get_module_length = GetModuleFileNameW(h_module, module_file_name, default_max_module_name_len);
                    }
                    module_file_name[default_max_module_name_len - 1] = (wchar_t)0;
                    if (get_module_length) {
                        const wchar_t* last_slash = wcsrchr(module_file_name, L'\\');
                        if (last_slash != nullptr) {
                            module_file_name_ptr = last_slash + 1;
                        }
                        stack_trace_str_ref += (const char16_t*)module_file_name_ptr;
                        stack_trace_str_ref.push_back(u'\t');
                    }
                } else {
                    stack_trace_str_ref += u"unknown module\t";
                }

                if (symbo_found) {
                    stack_trace_str_ref += (const char16_t*)symbol->Name;
                    stack_trace_str_ref.push_back(u' ');
                    DWORD displacement = 0;
                    IMAGEHLP_LINEW64 line;
                    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                    if (SymGetLineFromAddrW64(common_global_vars::get().stack_trace_process_, address, &displacement, &line)) {
                        stack_trace_str_ref.push_back(u'(');
                        stack_trace_str_ref += (const char16_t*)line.FileName;
                        stack_trace_str_ref.push_back(u':');
                        char tmp[32];
                        auto num_len = snprintf(tmp, sizeof(tmp), "%" PRIu32, static_cast<uint32_t>(line.LineNumber));
                        for (decltype(num_len) i = 0; i < num_len; ++i) {
                            stack_trace_str_ref.push_back(static_cast<char16_t>(tmp[i]));
                        }
                        stack_trace_str_ref.push_back(u')');
                    }
                } else {
                    stack_trace_str_ref += u"(unknown function and file)";
                }
            }
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }

        void* aligned_alloc(size_t alignment, size_t size)
        {
            return _aligned_malloc(size, alignment);
        }
        void aligned_free(void* ptr)
        {
            _aligned_free(ptr);
        }

        uint64_t file_node_info::hash_code() const
        {
            return bq::util::get_hash_64(this, sizeof(file_node_info));
        }

        static windows_version_info win_version_info_;
        const windows_version_info& get_windows_version_info()
        {
            bq::platform::scoped_mutex lock(common_global_vars::get().win_api_mutex_);
            if (win_version_info_.major_version == 0) {
                NTSTATUS(WINAPI * get_rtl_get_version)(PRTL_OSVERSIONINFOW) = nullptr;
                get_rtl_get_version = get_sys_api<decltype(get_rtl_get_version)>("ntdll.dll", "RtlGetVersion");
                if (get_rtl_get_version) {
                    RTL_OSVERSIONINFOW tmp;
                    NTSTATUS status = get_rtl_get_version(&tmp);
                    if (0 == status) {
                        win_version_info_.major_version = tmp.dwMajorVersion;
                        win_version_info_.minor_version = tmp.dwMinorVersion;
                        win_version_info_.build_number = tmp.dwBuildNumber;
                        return win_version_info_;
                    }
                }
                win_version_info_.major_version = 0;
            }
            return win_version_info_;
        }

        void* get_process_adress(const char* module_name, const char* api_name)
        {
            HMODULE module = GetModuleHandleA(module_name);
            if (module) {
                return reinterpret_cast<void*>(GetProcAddress(module, api_name));
            }
            return nullptr;
        }

    }
}
#include "bq_common/platform/win64_includes_end.h"
#endif
