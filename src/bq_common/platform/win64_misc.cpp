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
#if BQ_WIN
#include <Windows.h>
#include <sys/stat.h>
#include <errno.h>
#include <direct.h>
#include <stdio.h>
#include <fileapi.h>
#include <corecrt_io.h>
#include <stdint.h>
#include <string.h>
#include <DbgHelp.h>
#include <wchar.h>
#include <inttypes.h>

#pragma comment(lib, "dbghelp.lib")

namespace bq {
    namespace platform {
        // TODO optimize use TSC
        uint64_t high_performance_epoch_ms()
        {
            FILETIME ft;
            LARGE_INTEGER li;

            /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (ANSI UTC) and copy it
             * to a LARGE_INTEGER structure. */
            GetSystemTimeAsFileTime(&ft);
            li.LowPart = ft.dwLowDateTime;
            li.HighPart = ft.dwHighDateTime;

            uint64_t ret = li.QuadPart;
            const uint64_t UNIX_TIME_START = 0x019DB1DED53E8000; // January 1, 1970 (start of Unix epoch) in "ticks", difference from ANSI UTC to Unix Epoch.
            const uint64_t TICKS_PER_SECOND = 10000; // a tick is 100ns

            ret -= UNIX_TIME_START; /* Convert from file time to UNIX epoch time. */
            ret /= TICKS_PER_SECOND; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */
            return ret;
        }

        struct ___base_dir_initializer {
            bq::string base_dir;
            ___base_dir_initializer()
            {
                bq::array<char> tmp;
                tmp.fill_uninitialized(1024);
                while (_getcwd(&tmp[0], (int32_t)tmp.size()) == NULL) {
                    tmp.fill_uninitialized(1024);
                }
                base_dir = &tmp[0];
            }
        };
        const bq::string& get_base_dir(bool is_sandbox)
        {
            (void)is_sandbox;
            static ___base_dir_initializer base_dir_init_inst;
            return base_dir_init_inst.base_dir;
        }

        int32_t get_file_size(const char* file_path, size_t& size_ref)
        {
            HANDLE file = CreateFileA(
                file_path,
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
            struct _stat buf;
            int32_t result = _stat(path, &buf);
            if (result == 0) {
                if ((buf.st_mode & _S_IFMT) == _S_IFDIR) {
                    return true;
                }
            }
            return false;
        }

        bool is_regular_file(const char* path)
        {
            struct _stat buf;
            int32_t result = _stat(path, &buf);
            if (result == 0) {
                if ((buf.st_mode & _S_IFMT) == _S_IFREG) {
                    return true;
                }
            }
            return false;
        }

        static int32_t make_dir_recursive(char* path)
        {
            if (strlen(path) == 2 && path[1] == ':') {
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
            if (_mkdir(path) == 0) {
                return 0;
            }
            return errno;
        }

        int32_t make_dir(const char* path)
        {
            if (is_dir(path))
                return 0;
            char* path_cpy = _strdup(path);
            int32_t result = make_dir_recursive(path_cpy);
            free(path_cpy);
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

        int32_t remove_dir_or_file_inner(char* path, size_t cursor)
        {
            if (!is_dir(path)) {
                DWORD attr = GetFileAttributesA(path);
                attr &= ~FILE_ATTRIBUTE_READONLY;
                SetFileAttributesA(path, attr);
                int32_t result = remove(path);
                if (result != 0) {
                    return errno;
                }
                return 0;
            } else {
                path[cursor] = '/';
                path[cursor + 1] = '*';
                path[cursor + 2] = '\0';
                WIN32_FIND_DATAA find_data;
                HANDLE h_file;
                h_file = FindFirstFileA(path, &find_data);
                if (h_file == INVALID_HANDLE_VALUE) {
                    // it's empty
                } else {
                    auto file_name_len = strlen(find_data.cFileName);
                    memcpy(path + cursor + 1, find_data.cFileName, file_name_len);
                    path[cursor + 1 + file_name_len] = '\0';
                    if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                        // ignore
                    } else {
                        int32_t result = remove_dir_or_file_inner(path, cursor + 1 + file_name_len);
                        if (result != 0) {
                            return result;
                        }
                    }
                    while (FindNextFileA(h_file, &find_data)) {
                        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                            // ignore
                            continue;
                        }
                        file_name_len = strlen(find_data.cFileName);
                        memcpy(path + cursor + 1, find_data.cFileName, file_name_len);
                        path[cursor + 1 + file_name_len] = '\0';
                        int32_t result = remove_dir_or_file_inner(path, cursor + 1 + file_name_len);
                        if (result != 0) {
                            return result;
                        }
                    }
                }
                path[cursor] = '\0';
                DWORD attr = GetFileAttributesA(path);
                attr &= ~FILE_ATTRIBUTE_READONLY;
                SetFileAttributesA(path, attr);
                if (!RemoveDirectoryA(path)) {
                    return static_cast<int32_t>(GetLastError());
                }
                return 0;
            }
        }

        int32_t remove_dir_or_file(const char* path)
        {
            char temp_path[MAX_PATH] = { '\0' };
            auto str_len = strlen(path);
            if (str_len == 0) {
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
        BQ_PACK_BEGIN
        struct alignas(4) windows_file_node_info {
            DWORD volumn;
            DWORD idx_high;
            DWORD idx_low;
            uint64_t hash_code() const
            {
                return bq::util::get_hash_64(this, sizeof(windows_file_node_info));
            }
            bool operator==(const windows_file_node_info& rhs) const
            {
                return volumn == rhs.volumn && idx_high == rhs.idx_high && idx_low == rhs.idx_low;
            }
        }
        BQ_PACK_END

        static bq::hash_map<windows_file_node_info, file_open_mode_enum> &get_file_exclusive_cache()
        {
            static bq::hash_map<windows_file_node_info, file_open_mode_enum> file_exclusive_cache;
            return file_exclusive_cache;
        }

        static bq::platform::mutex &get_file_exclusive_mutex()
        {
            static bq::platform::mutex file_exclusive_mutex;
            return file_exclusive_mutex;
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
            bq::platform::mutex& file_exclusive_mutex = get_file_exclusive_mutex();
            bq::hash_map<windows_file_node_info, file_open_mode_enum> &file_exclusive_cache = get_file_exclusive_cache();
            bq::platform::scoped_mutex lock(file_exclusive_mutex);
            windows_file_node_info node_info;
            node_info.volumn = file_info.dwVolumeSerialNumber;
            node_info.idx_high = file_info.nFileIndexHigh;
            node_info.idx_low = file_info.nFileIndexLow;
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
            bq::platform::mutex& file_exclusive_mutex = get_file_exclusive_mutex();
            bq::hash_map<windows_file_node_info, file_open_mode_enum>& file_exclusive_cache = get_file_exclusive_cache();
            bq::platform::scoped_mutex lock(file_exclusive_mutex);
            windows_file_node_info node_info;
            node_info.volumn = file_info.dwVolumeSerialNumber;
            node_info.idx_high = file_info.nFileIndexHigh;
            node_info.idx_low = file_info.nFileIndexLow;
            file_exclusive_cache.erase(node_info);
        }

        int32_t open_file(const char* path, file_open_mode_enum mode, platform_file_handle& out_file_handle)
        {
            out_file_handle = CreateFileA(path, ((int32_t)(mode & file_open_mode_enum::read) ? GENERIC_READ : 0) | ((int32_t)(mode & file_open_mode_enum::write) ? GENERIC_WRITE : 0), ((int32_t)(mode & file_open_mode_enum::exclusive) ? FILE_SHARE_READ : (FILE_SHARE_READ | FILE_SHARE_WRITE)), NULL, ((int32_t)(mode & file_open_mode_enum::auto_create) ? OPEN_ALWAYS : OPEN_EXISTING), FILE_ATTRIBUTE_NORMAL, NULL);
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
            size_t max_size_pertime = static_cast<size_t>(UINT32_MAX);
            size_t max_write_size_current = 0;
            while (out_real_write_size < write_size) {
                size_t need_write_size_this_time = bq::min_value(max_size_pertime, write_size - max_write_size_current);
                DWORD out_size = 0;
                bool result = WriteFile(file_handle, src_addr, static_cast<DWORD>(need_write_size_this_time), &out_size, NULL);
                if (!result) {
                    return static_cast<int32_t>(GetLastError());
                }
                out_real_write_size += static_cast<size_t>(out_size);
                if (out_size != need_write_size_this_time) {
                    return 0;
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
            char temp_path[MAX_PATH] = { '\0' };
            if (path && strlen(path) != 0) {
                auto str_len = strlen(path);
                memcpy(temp_path, path, str_len);
                if (temp_path[str_len - 1] == '\\' || temp_path[str_len - 1] == '/') {
                    temp_path[str_len - 1] = '\0';
                    --str_len;
                }
                temp_path[str_len] = '/';
                temp_path[str_len + 1] = '*';
                temp_path[str_len + 2] = '\0';
            }
            bq::array<bq::string> list;
            WIN32_FIND_DATAA find_data;
            HANDLE h_file;
            h_file = FindFirstFileA(temp_path, &find_data);
            if (h_file == INVALID_HANDLE_VALUE) {
                // it's empty
            } else {
                if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                    // ignore
                } else {
                }
                while (FindNextFileA(h_file, &find_data)) {
                    if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                        // ignore
                        continue;
                    }
                    const char* name_ptr = find_data.cFileName;
                    list.push_back(name_ptr);
                }
            }
            return list;
        }
        bool share_file(const char* file_path)
        {
            // 使用默认文件浏览器打开文件夹
            HINSTANCE result = ShellExecute(NULL, "open", file_path, NULL, NULL, SW_SHOWNORMAL);
            // 检查操作是否成功
            return reinterpret_cast<int64_t>(result) > 32;
        }

        static thread_local bq::string stack_trace_current_str_;
        static thread_local bq::u16string stack_trace_current_str_u16_;
        static HANDLE stack_trace_process_ = GetCurrentProcess();
        static bq::platform::atomic<bool> stack_trace_sym_initialized_ = false;
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            const char16_t* u16_str;
            uint32_t u16_str_len;
            get_stack_trace_utf16(skip_frame_count, u16_str, u16_str_len);
            stack_trace_current_str_.clear();
            stack_trace_current_str_.fill_uninitialized(((u16_str_len * 3) >> 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf16_to_utf8(u16_str, u16_str_len, stack_trace_current_str_.begin(), (uint32_t)stack_trace_current_str_.size());
            assert(encoded_size < stack_trace_current_str_.size());
            stack_trace_current_str_.erase(stack_trace_current_str_.begin() + encoded_size, stack_trace_current_str_.size() - encoded_size);
            out_str_ptr = stack_trace_current_str_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_.size();
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            HANDLE thread = GetCurrentThread();
            CONTEXT context;

            if (!stack_trace_sym_initialized_.exchange(true, bq::platform::memory_order::relaxed)) {
                SymInitialize(stack_trace_process_, NULL, TRUE);
            }
            stack_trace_current_str_u16_.clear();
            static bq::platform::mutex stack_trace_mutex_;
            bq::platform::scoped_mutex lock(stack_trace_mutex_);
            RtlCaptureContext(&context);

            STACKFRAME64 stack;
            memset(&stack, 0, sizeof(STACKFRAME64));
            stack.AddrPC.Offset = context.Rip;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Rsp;
            stack.AddrStack.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Rbp;
            stack.AddrFrame.Mode = AddrModeFlat;

            bool sym_refreshed = false;
            bool effective_stack_started = false;
            uint32_t current_frame_idx = 0;
            while (StackWalk64(
                IMAGE_FILE_MACHINE_AMD64,
                stack_trace_process_,
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
                bool symbo_found = SymFromAddrW(stack_trace_process_, address, &displacement64, symbol);
                if (!symbo_found && !sym_refreshed) {
                    int32_t error_code = GetLastError();
                    (void)error_code;
                    sym_refreshed = true;
                    SymRefreshModuleList(stack_trace_process_);
                    symbo_found = SymFromAddrW(stack_trace_process_, address, &displacement64, symbol);
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

                stack_trace_current_str_u16_.push_back(u'\n');
                stack_trace_current_str_u16_.fill_uninitialized(16);
                static_assert(sizeof(wchar_t) == sizeof(WCHAR) && sizeof(WCHAR) == sizeof(char16_t), "windows wchar_t should be 2 bytes");
                swprintf((wchar_t*)(char16_t*)stack_trace_current_str_u16_.end() - 16, 16 + 1, L"%016" PRIx64 "", (uintptr_t)address);
                stack_trace_current_str_u16_.push_back(L'\t');

                DWORD64 module_base = SymGetModuleBase64(stack_trace_process_, address);
                if (!module_base && !sym_refreshed) {
                    sym_refreshed = true;
                    SymRefreshModuleList(stack_trace_process_);
                    module_base = SymGetModuleBase64(stack_trace_process_, address);
                }

                if (module_base) {
                    HMODULE h_module = (HMODULE)module_base;
                    constexpr DWORD default_max_module_name_len = 1024;
                    wchar_t module_file_name[default_max_module_name_len];
                    const wchar_t* module_file_name_ptr = module_file_name;
                    DWORD get_module_length = GetModuleFileNameW(h_module, module_file_name, default_max_module_name_len);
                    if (!get_module_length && !sym_refreshed) {
                        sym_refreshed = true;
                        SymRefreshModuleList(stack_trace_process_);
                        get_module_length = GetModuleFileNameW(h_module, module_file_name, default_max_module_name_len);
                    }
                    module_file_name[default_max_module_name_len - 1] = (wchar_t)0;
                    if (get_module_length) {
                        const wchar_t* last_slash = wcsrchr(module_file_name, L'\\');
                        if (last_slash != nullptr) {
                            module_file_name_ptr = last_slash + 1;
                        }
                        stack_trace_current_str_u16_ += (const char16_t*)module_file_name_ptr;
                        stack_trace_current_str_u16_.push_back(u'\t');
                    }
                } else {
                    stack_trace_current_str_u16_ += u"unknown module\t";
                }

                if (symbo_found) {
                    stack_trace_current_str_u16_ += (const char16_t*)symbol->Name;
                    stack_trace_current_str_u16_.push_back(u' ');
                    DWORD displacement = 0;
                    IMAGEHLP_LINEW64 line;
                    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                    if (SymGetLineFromAddrW64(stack_trace_process_, address, &displacement, &line)) {
                        stack_trace_current_str_u16_.push_back(u'(');
                        stack_trace_current_str_u16_ += (const char16_t*)line.FileName;
                        stack_trace_current_str_u16_.push_back(u':');
                        char16_t tmp[32];
                        swprintf((wchar_t*)tmp, 32, L"%d", line.LineNumber);
                        stack_trace_current_str_u16_ += tmp;
                        stack_trace_current_str_u16_.push_back(u')');
                    }
                } else {
                    stack_trace_current_str_u16_ += u"(unknown function and file)";
                }
            }
            out_str_ptr = stack_trace_current_str_u16_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_u16_.size();
        }

        void init_for_file_manager()
        {
            get_file_exclusive_mutex();
        }
    }
}
#endif
