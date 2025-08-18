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
#include <Windows.h>
#include <errno.h>
#include <stdlib.h>

namespace bq {
    namespace platform {
        // Static API function pointers, zero initialization.
        static VOID(WINAPI* initialize_cv_func_)(PCONDITION_VARIABLE);
        static VOID(WINAPI* wake_cv_func_)(PCONDITION_VARIABLE);
        static VOID(WINAPI* wake_all_cv_func_)(PCONDITION_VARIABLE);
        static BOOL(WINAPI* sleep_cv_cs_func_)(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD);

        static bool native_cv_support_tested_;

        // Initialize condition variable APIs - called once during startup
        static void init_apis()
        {
            initialize_cv_func_ = bq::platform::get_sys_api<decltype(initialize_cv_func_)>("kernel32.dll", "InitializeConditionVariable");
            wake_cv_func_ = bq::platform::get_sys_api<decltype(wake_cv_func_)>("kernel32.dll", "WakeConditionVariable");
            wake_all_cv_func_ = bq::platform::get_sys_api<decltype(wake_all_cv_func_)>("kernel32.dll", "WakeAllConditionVariable");
            sleep_cv_cs_func_ = bq::platform::get_sys_api<decltype(sleep_cv_cs_func_)>("kernel32.dll", "SleepConditionVariableCS");

            native_cv_support_tested_ = true;
        }

        // Platform definition that supports both implementations
        struct condition_variable_platform_def {
            union handle_union {
                CONDITION_VARIABLE cv_;
                HANDLE condition_variable_handle_;
            };
            handle_union handle_;
        };

        condition_variable::condition_variable()
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            platform_data_ = new condition_variable_platform_def();
            if (platform_data_) {
                if (initialize_cv_func_) {
                    // Use native condition variable API
                    InitializeConditionVariable(&platform_data_->handle_.cv_);
                    // initialize_cv_func_(&platform_data_->handle_.cv_);
                } else {
                    // Fall back to event-based implementation
                    platform_data_->handle_.condition_variable_handle_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                    // Check if creation was successful
                    if (!platform_data_->handle_.condition_variable_handle_) {
                        bq::util::log_device_console(log_level::fatal, "%s : %d : create condition_variable failed, error code:%d", __FILE__, __LINE__, static_cast<int32_t>(GetLastError()));
                        assert(false && "create condition_variable failed, see the device log output for more information");
                        return;
                    }
                }
            } else {
                assert(false && "not enough memory for condition_variable");
            }
        }

        condition_variable::~condition_variable()
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            if (platform_data_) {
                if (initialize_cv_func_) {
                } else {
                    CloseHandle(platform_data_->handle_.condition_variable_handle_);
                    platform_data_->handle_.condition_variable_handle_ = nullptr;
                }
                delete platform_data_;
            }
            platform_data_ = nullptr;
        }

        void condition_variable::wait(bq::platform::mutex& lock)
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            if (initialize_cv_func_) {
                // Use native condition variable API
                sleep_cv_cs_func_(&platform_data_->handle_.cv_,
                    (PCRITICAL_SECTION)lock.get_platform_handle(),
                    INFINITE);
            } else {
                // Fall back to event-based implementation
                lock.unlock();
                WaitForSingleObjectEx(platform_data_->handle_.condition_variable_handle_, INFINITE, TRUE);
                lock.lock();
            }
        }

        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms)
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            if (initialize_cv_func_) {
                // Use native condition variable API with timeout
                BOOL result = sleep_cv_cs_func_(&platform_data_->handle_.cv_,
                    (PCRITICAL_SECTION)lock.get_platform_handle(),
                    (DWORD)wait_time_ms);
                return result;
            } else {
                // Fall back to event-based implementation with timeout
                lock.unlock();
                auto result = WaitForSingleObjectEx(platform_data_->handle_.condition_variable_handle_, (DWORD)wait_time_ms, TRUE);
                lock.lock();
                if (result == WAIT_TIMEOUT) {
                    return false;
                }
                return true;
            }
        }

        void condition_variable::notify_one() noexcept
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            if (initialize_cv_func_) {
                // Wake a single waiting thread using native API
                wake_cv_func_(&platform_data_->handle_.cv_);
            } else {
                // Wake a single waiting thread using event
                SetEvent(platform_data_->handle_.condition_variable_handle_);
            }
        }

        void condition_variable::notify_all() noexcept
        {
            if (!native_cv_support_tested_) {
                init_apis();
            }
            if (initialize_cv_func_) {
                // Wake all waiting threads using native API
                wake_all_cv_func_(&platform_data_->handle_.cv_);
            } else {
                // In event-based mode, this only wakes one waiting thread
                // Note: This is a limitation of the legacy implementation
                SetEvent(platform_data_->handle_.condition_variable_handle_);

                // NOTE: This implementation has a limitation in legacy mode.
                // It cannot truly wake all waiting threads with a single event.
            }
        }
    }
}
#endif
