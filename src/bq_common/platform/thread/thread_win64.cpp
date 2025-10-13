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
#include "bq_common/platform/thread/thread_win64.h"
#ifdef BQ_WIN
#include "bq_common/bq_common.h"
#include <processthreadsapi.h>
#include <stringapiset.h>
#ifdef BQ_ARM
#include <intrin.h>
#endif

namespace bq {
    namespace platform {
        struct thread_platform_def {
            bq::platform::atomic<HANDLE> thread_handle = INVALID_HANDLE_VALUE;
#if defined(BQ_JAVA)
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

        thread::thread(thread_attr attr)
            : attr_(attr)
            , status_(enum_thread_status::init)
        {
            thread_id_ = 0;
            platform_data_ = (thread_platform_def*)malloc(sizeof(thread_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) thread_platform_def();
        }

#if defined(BQ_JAVA)
        void thread::attach_to_jvm()
        {
            assert(status_.load() == enum_thread_status::init && "attach_to_jvm can only be called before thread is running!");
            platform_data_->jvm = get_jvm();
        }

        JNIEnv* thread::get_jni_env()
        {
            auto current_thread_id = get_current_thread_id();
            auto thread_id_this = get_thread_id();
            assert(current_thread_id == thread_id_this && "bq_thread::get_jni_env can only be called in the thread which is created by bq_thread instance!");
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
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:%" PRId32, thread_name_.c_str(), static_cast<int32_t>(GetLastError()));
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
                bq::util::log_device_console(log_level::error, "join thread \"%s\" failed, thread_id:%" PRIu64 ", error code : %" PRId32 ", return value : %" PRId32, thread_name_.c_str(), static_cast<uint64_t>(thread_id_), static_cast<int32_t>(GetLastError()), static_cast<int32_t>(wait_result));
            }
        }

        void thread::yield()
        {
            SwitchToThread();
        }

        void thread::cpu_relax()
        {
#ifdef BQ_MSVC
#if defined(BQ_X86)
            _mm_pause();
#elif defined(BQ_ARM)
            __yield();
#else
            _ReadWriteBarrier();
#endif
#else
#ifdef BQ_ARM
            __asm__ __volatile__("yield");
#else
            __asm__ __volatile__("pause");
#endif
#endif
        }

        void thread::sleep(uint64_t millsec)
        {
            SleepEx((DWORD)millsec, true);
        }

        static bool is_thread_name_supported_tested_; // false by zero initialization
        static HRESULT(WINAPI* get_thread_desc_func_)(HANDLE hThread, PWSTR* ppszThreadDescription);
        static HRESULT(WINAPI* set_thread_desc_func_)(HANDLE hThread, PCWSTR ppszThreadDescription);

        static void init_thread_apis()
        {
            const auto& os_ver_info = bq::platform::get_windows_version_info();
            if (os_ver_info.dwMajorVersion > 10
                || (os_ver_info.dwMajorVersion == 10 && os_ver_info.dwBuildNumber >= 14393)) {
                get_thread_desc_func_ = bq::platform::get_sys_api<decltype(get_thread_desc_func_)>("kernel32.dll", "GetThreadDescription");
                set_thread_desc_func_ = bq::platform::get_sys_api<decltype(set_thread_desc_func_)>("kernel32.dll", "SetThreadDescription");
            }
            is_thread_name_supported_tested_ = true;
        }

        bq::string thread::get_current_thread_name()
        {
            if (!is_thread_name_supported_tested_) {
                init_thread_apis();
            }
            if (get_thread_desc_func_) {
                HANDLE current_thread_handle = GetCurrentThread();
                PWSTR raw_thread_name;

                HRESULT result = get_thread_desc_func_(current_thread_handle, &raw_thread_name); // GetThreadDescription(current_thread_handle, &raw_thread_name);//
                if (SUCCEEDED(result)) {
                    int32_t utf8_len = WideCharToMultiByte(CP_UTF8, 0, raw_thread_name, -1, nullptr, 0, nullptr, nullptr);
                    assert(utf8_len >= 1);
                    --utf8_len;
                    if (0 == utf8_len) {
                        return "";
                    }
                    bq::string thread_name_utf8;
                    thread_name_utf8.fill_uninitialized((size_t)utf8_len);
                    if (!WideCharToMultiByte(CP_UTF8, 0, raw_thread_name, static_cast<int32_t>(wcslen(raw_thread_name)), &thread_name_utf8[0], utf8_len, nullptr, nullptr)) {
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

        bool thread::is_thread_alive(thread_id id)
        {
            if (id == 0) {
                return false;
            }
            HANDLE thread_handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, static_cast<DWORD>(id));
            if (thread_handle == NULL) {
                return false;
            }
            DWORD exit_code;
            if (GetExitCodeThread(thread_handle, &exit_code)) {
                CloseHandle(thread_handle);
                return exit_code == STILL_ACTIVE; // STILL_ACTIVE (259) indicates the thread is still running
            }
            CloseHandle(thread_handle);
            return false; // Failed to get exit code, assume the thread has terminated
        }

        thread::~thread()
        {
            if (platform_data_->thread_handle.load() != INVALID_HANDLE_VALUE) {
                if (!CloseHandle(platform_data_->thread_handle.load())) {
                    bq::util::log_device_console(bq::log_level::error, "Win64 Thread CloseHandle failed, GetLastError=%" PRId32 ", thread name:%s, thread_id:%" PRIu64, static_cast<int32_t>(GetLastError()), thread_name_.c_str(), thread_id_);
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
#if defined(BQ_MSVC)
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
            if (!is_thread_name_supported_tested_) {
                init_thread_apis();
            }
            if (set_thread_desc_func_) {
                HANDLE current_thread_handle = GetCurrentThread();
                int32_t required_size = MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), static_cast<int32_t>(thread_name_.size()), nullptr, 0);
                bq::array<wchar_t> wide_str;
                wide_str.fill_uninitialized(static_cast<size_t>(required_size + 1));
                wide_str[required_size] = 0;
                int32_t converted_size = MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), static_cast<int32_t>(thread_name_.size()), &wide_str[0], required_size);
                if (converted_size == required_size) {
                    set_thread_desc_func_(current_thread_handle, &wide_str[0]);
                }
            } else {
                apply_name_raise();
            }
        }

        void thread::internal_run()
        {
#if defined(BQ_JAVA)
            if (platform_data_->jvm) {
                using attach_param_type = function_argument_type_t<decltype(&JavaVM::AttachCurrentThread), 0>;
                jint result = platform_data_->jvm->AttachCurrentThread(reinterpret_cast<attach_param_type>(&platform_data_->jenv), NULL);
                if (result != JNI_OK) {
                    bq::util::log_device_console(bq::log_level::error, "failed to JavaVM AttachCurrentThread, result code %" PRId32, static_cast<int32_t>(result));
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
#if defined(BQ_JAVA)
            if (platform_data_->jvm) {
                platform_data_->jvm->DetachCurrentThread();
            }
#endif
            status_.store_seq_cst(enum_thread_status::released);
        }
    }
}
#endif
