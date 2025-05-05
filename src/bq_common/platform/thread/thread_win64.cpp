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
#ifdef BQ_WIN
#include <processthreadsapi.h>
#include <Windows.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stringapiset.h>
#if BQ_JAVA
#include <jni.h>
#endif

namespace bq {
    namespace platform {
        struct thread_platform_def {
            bq::platform::atomic<HANDLE> thread_handle = INVALID_HANDLE_VALUE;
#if BQ_JAVA
            JavaVM* jvm = nullptr;
            JNIEnv* jenv = nullptr;
#endif
        };

        struct thread_platform_processor {
            static DWORD thread_process(LPVOID data)
            {
                thread* thread_ptr = (thread*)data;
                thread_ptr->internal_run();
                return 0;
            }
        };

        static HMODULE hKernel32 = 0;
        thread::thread(thread_attr attr)
            : attr_(attr)
            , status_(enum_thread_status::init)
        {
            if (!hKernel32) {
                hKernel32 = LoadLibrary(TEXT("kernel32.dll"));
            }
            thread_id_ = 0;
            platform_data_ = (thread_platform_def*)malloc(sizeof(thread_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) thread_platform_def();
        }

#if BQ_JAVA
        void thread::attach_to_jvm()
        {
            assert(status_.load() == enum_thread_status::init && "attach_to_jvm can only be called before thread is running!");
            platform_data_->jvm = get_jvm();
        }

        JNIEnv* thread::get_jni_env()
        {
            auto current_thread_id = get_current_thread_id();
            auto thread_id = get_thread_id();
            assert(current_thread_id == thread_id && "bq_thread::get_jni_env can only be called in the thread which is created by bq_thread instance!");
            return platform_data_->jenv;
        }
#endif

        void thread::set_thread_name(const bq::string& thread_name)
        {
            thread_name_ = thread_name;
            if (thread_name_.size() >= 15) {
                bq::util::log_device_console(bq::log_level::warning, "thread name \"%s\" excceed max length limit of android, the length of thread name can not be larger than 15. so it will be truncated to \"%s\"", thread_name_.c_str(), thread_name_.substr(0, 15).c_str());
                thread_name_ = thread_name_.substr(0, 15);
            }
            auto current_status = status_.load();
            if (current_status == enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to set thread name \"%s\" when thread have already been running, thread id :%" PRIu64 ", thread status:%d", thread_name.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
            }
        }

        void thread::start()
        {
            auto current_status = status_.load();
            if (current_status == enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to start a thread \"%s\" which is still running, thread id :%" PRIu64 ", thread status : %d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            platform_data_->thread_handle = CreateThread(NULL, attr_.max_stack_size, &thread_platform_processor::thread_process, this, STACK_SIZE_PARAM_IS_A_RESERVATION, (DWORD*)&thread_id_);
            if (!platform_data_->thread_handle.load()) {
                status_.store_seq_cst(enum_thread_status::error);
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:%d", thread_name_.c_str(), GetLastError());
                assert(false && "create thread failed, see the device log output for more information");
                return;
            }
            auto expected_status = enum_thread_status::init;
            status_.compare_exchange_strong(expected_status, enum_thread_status::running);
        }

        void thread::join()
        {
            auto current_status = status_.load();
            if (!platform_data_->thread_handle.load()) {
                bq::util::log_device_console(log_level::warning, "trying to join a thread \"%s\" which is not started or is ended, thread id :%" PRIu64 ", thread status : %d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            DWORD wait_result = WaitForSingleObjectEx(platform_data_->thread_handle.load(), INFINITE, true);
            if (wait_result != WAIT_OBJECT_0) {
                bq::util::log_device_console(log_level::error, "join thread \"%s\" failed, thread_id:%" PRIu64 ", error code : %d, return value : %d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), GetLastError(), wait_result);
            }
        }

        void thread::yield()
        {
            SwitchToThread();
        }

        void thread::cpu_relax()
        {
            SleepEx((DWORD)0, true);
        }

        void thread::sleep(uint64_t millsec)
        {
            SleepEx((DWORD)millsec, true);
        }

        NTSTATUS(WINAPI* GetRtlGetVersion)
        (PRTL_OSVERSIONINFOW) = nullptr;
        void InitializeRtlGetVersion()
        {
            HMODULE module = GetModuleHandle("ntdll.dll");
            if (module) {
                GetRtlGetVersion = reinterpret_cast<decltype(GetRtlGetVersion)>((void*)GetProcAddress(module, "RtlGetVersion"));
            }
        }

        bool IsWindows10OrGreaterRtl()
        {
            if (!GetRtlGetVersion) {
                InitializeRtlGetVersion();
            }
            if (!GetRtlGetVersion) {
                return false;
            }
            RTL_OSVERSIONINFOW os_ver_info = {};
            os_ver_info.dwOSVersionInfoSize = sizeof(os_ver_info);
            NTSTATUS status = GetRtlGetVersion(&os_ver_info);
            if (status == 0 /*STATUS_SUCCESS*/) {
                return os_ver_info.dwMajorVersion > 10 || (os_ver_info.dwMajorVersion == 10 && os_ver_info.dwBuildNumber >= 14393);
            }
            return false;
        }

        bq::string thread::get_current_thread_name()
        {
            if (IsWindows10OrGreaterRtl()) {
                if (!hKernel32)
                    return "";
                typedef HRESULT(WINAPI * GetThreadDescriptionFunc)(HANDLE hThread, PWSTR * ppszThreadDescription);
                GetThreadDescriptionFunc pGetThreadDescription = reinterpret_cast<GetThreadDescriptionFunc>((void*)GetProcAddress(hKernel32, "GetThreadDescription"));
                if (!pGetThreadDescription) {
                    return "";
                }

                HANDLE current_thread_handle = GetCurrentThread();
                PWSTR raw_thread_name;

                HRESULT result = pGetThreadDescription(current_thread_handle, &raw_thread_name); // GetThreadDescription(current_thread_handle, &raw_thread_name);//
                if (SUCCEEDED(result)) {
                    int32_t utf8_len = WideCharToMultiByte(CP_UTF8, 0, raw_thread_name, -1, nullptr, 0, nullptr, nullptr);
                    assert(utf8_len >= 1);
                    --utf8_len;
                    if (0 == utf8_len) {
                        return "";
                    }
                    bq::string thread_name_utf8;
                    thread_name_utf8.fill_uninitialized((size_t)utf8_len);
                    if (!WideCharToMultiByte(CP_UTF8, 0, raw_thread_name, static_cast<int>(wcslen(raw_thread_name)), &thread_name_utf8[0], utf8_len, nullptr, nullptr)) {
                        return "";
                    }
                    return thread_name_utf8;
                }
            }
            char tmp[64];
            snprintf(tmp, 64, "THREAD-%" PRIu64, get_current_thread_id());
            return tmp;
        }

        static BQ_TLS thread::thread_id current_thread_id_;
        thread::thread_id thread::get_current_thread_id()
        {
            if (!current_thread_id_) {
                current_thread_id_ = static_cast<thread_id>(GetCurrentThreadId());
            }
            return current_thread_id_;
        }

        thread::~thread()
        {
            DWORD exitCode = 0;

            if (platform_data_->thread_handle.load() != INVALID_HANDLE_VALUE) {
                if (GetExitCodeThread(platform_data_->thread_handle.load(), &exitCode)) {
                    const char* stack_trace_addr = nullptr;
                    uint32_t stack_trace_len = 0;
                    bq::platform::get_stack_trace(0, stack_trace_addr, stack_trace_len);
                    if (exitCode == STILL_ACTIVE) {
                        bq::util::log_device_console(bq::log_level::fatal, "thread instance is destructed but thread is still exist, stack trace:\n%s", stack_trace_addr);
                        assert(false && "thread instance is destructed but thread is still running");
                    }
                } else {
                    bq::util::log_device_console(bq::log_level::fatal, "GetExitCodeThread failed, GetLastError=%d", GetLastError());
                }
                if (!CloseHandle(platform_data_->thread_handle.load())) {
                    bq::util::log_device_console(bq::log_level::error, "Win64 Thread CloseHandle failed, GetLastError=%d, thread name:%s, thread_id:%" PRIu64, GetLastError(), thread_name_.c_str(), thread_id_);
                }
            }
            platform_data_->~thread_platform_def();
            free(platform_data_);
            platform_data_ = nullptr;
        }

#pragma pack(push, 8)
        typedef struct tagTHREADNAME_INFO {
            DWORD dwType; // Must be 0x1000.
            LPCSTR szName; // Pointer to name (in user addr space).
            DWORD dwThreadID; // Thread ID (-1=caller thread).
            DWORD dwFlags; // Reserved for future use, must be zero.
        } THREADNAME_INFO;
#pragma pack(pop)

        void thread::apply_name_raise()
        {
#if BQ_MSVC
            THREADNAME_INFO info;
            info.dwType = 0x1000;
            info.szName = thread_name_.c_str();
            info.dwThreadID = (DWORD)-1;
            info.dwFlags = 0;

#pragma warning(push)
#pragma warning(disable : 6320 6322)
            __try {
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
#pragma warning(pop)
#else
            bq::util::log_device_console(bq::log_level::warning, "Set thread name is only support Windows 10 or later, or compiled with MSVC");
#endif
        }

        void thread::apply_thread_name()
        {
            if (IsWindows10OrGreaterRtl()) {
                if (!hKernel32)
                    return;
                HANDLE current_thread_handle = GetCurrentThread();
                int required_size = MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), static_cast<int>(thread_name_.size()), nullptr, 0);
                bq::array<wchar_t> wide_str;
                wide_str.fill_uninitialized(required_size + 1);
                wide_str[required_size] = 0;
                int converted_size = MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), static_cast<int>(thread_name_.size()), &wide_str[0], required_size);
                if (converted_size == required_size) {
                    typedef HRESULT(WINAPI * SetThreadDescriptionFunc)(HANDLE hThread, PCWSTR ppszThreadDescription);
                    SetThreadDescriptionFunc pGetThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>((void*)GetProcAddress(hKernel32, "SetThreadDescription"));
                    if (pGetThreadDescription) {
                        pGetThreadDescription(current_thread_handle, &wide_str[0]);
                    }
                }
            } else {
                apply_name_raise();
            }
        }

        void thread::internal_run()
        {
#if BQ_JAVA
            if (platform_data_->jvm) {
                using attach_param_type = function_argument_type_t<decltype(&JavaVM::AttachCurrentThread), 0>;
                jint result = platform_data_->jvm->AttachCurrentThread(reinterpret_cast<attach_param_type>(&platform_data_->jenv), NULL);
                if (result != JNI_OK) {
                    bq::util::log_device_console(bq::log_level::error, "failed to JavaVM AttachCurrentThread, result code %d", result);
                    assert(false && "failed to JavaVM AttachCurrentThread");
                    return;
                }
            }
#endif
            while (status_.load() != enum_thread_status::running
                    && status_.load() != enum_thread_status::pendding_cancel) {
                cpu_relax();
            }
            apply_thread_name();
            run();
            auto expected_status = enum_thread_status::running;
            if (!status_.compare_exchange_strong(expected_status, enum_thread_status::finished, bq::platform::memory_order::seq_cst, bq::platform::memory_order::seq_cst)) {
                expected_status = enum_thread_status::pendding_cancel;
                status_.compare_exchange_strong(expected_status, enum_thread_status::finished, bq::platform::memory_order::seq_cst, bq::platform::memory_order::seq_cst);
            }
            on_finished();

            thread_id_ = 0;
            bq::util::log_device_console(log_level::info, "thread cancel success");
#if BQ_JAVA
            if (platform_data_->jvm) {
                platform_data_->jvm->DetachCurrentThread();
            }
#endif
            status_.store_seq_cst(enum_thread_status::released);
        }
    }
}
#endif
