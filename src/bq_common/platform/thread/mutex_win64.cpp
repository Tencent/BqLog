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
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
namespace bq {
    namespace platform {
        struct mutex_platform_def {
            HANDLE mutex_handle = NULL;
            bq::platform::atomic<thread::thread_id> owner_thread_id; // windows platform mutex is always Reentrant lock, we have to simulate the non-recursive mutex by ourself.
                                                                     // it's working because no thread id is 0 on windows.
                                                                     // refer to:https://docs.microsoft.com/en-us/windows/win32/procthread/thread-handles-and-identifiers
        };

        mutex::mutex()
            : reentrant_(true)
        {
#pragma warning(push)
#pragma warning(disable : 6011)
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            platform_data_->owner_thread_id.store(0);
            platform_data_->mutex_handle = CreateMutex(NULL, false, NULL);
            if (!platform_data_->mutex_handle) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : create mutex failed, error code:%d", __FILE__, __LINE__, GetLastError());
                assert(false && "create mutex failed, see the device log output for more information");
                return;
            }
#pragma warning(pop)
        }

        mutex::mutex(bool reentrant /* = true */)
            : reentrant_(reentrant)
        {
#pragma warning(push)
#pragma warning(disable : 6011)
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            platform_data_->owner_thread_id.store(0);
            platform_data_->mutex_handle = CreateMutex(NULL, false, NULL);
            if (!platform_data_->mutex_handle) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : create mutex failed, error code:%d", __FILE__, __LINE__, GetLastError());
                assert(false && "create mutex failed, see the device log output for more information");
                return;
            }
#pragma warning(pop)
        }

        mutex::~mutex()
        {
            if (platform_data_->mutex_handle) {
                CloseHandle(platform_data_->mutex_handle);
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void mutex::lock()
        {
            WaitForSingleObjectEx(platform_data_->mutex_handle, INFINITE, true);
            if (!reentrant_) {
                thread::thread_id current_thread_id = thread::get_current_thread_id();
                if (platform_data_->owner_thread_id.load(memory_order::acquire) == current_thread_id) {
                    bq::util::log_device_console(log_level::error, "%s : %d : you're try to reenter a non-recursive mutex", __FILE__, __LINE__);
                    assert(false && "mutex recursive lock");
                }
                platform_data_->owner_thread_id.store(current_thread_id, memory_order::release);
            }
        }

        bool mutex::try_lock()
        {
            auto result = WaitForSingleObjectEx(platform_data_->mutex_handle, 0, true);
            if (result == WAIT_OBJECT_0) {
                if (!reentrant_) {
                    thread::thread_id current_thread_id = thread::get_current_thread_id();
                    assert(platform_data_->owner_thread_id.load(memory_order::acquire) != current_thread_id && "mutex recursive lock");
                    platform_data_->owner_thread_id.store(current_thread_id, memory_order::release);
                }
                return true;
            } else {
                return false;
            }
        }

        bool mutex::unlock()
        {
            if (ReleaseMutex(platform_data_->mutex_handle)) {
                platform_data_->owner_thread_id.store(0, memory_order::release);
                return true;
            }
            return false;
        }

        void* mutex::get_platform_handle()
        {
            return (void*)(&platform_data_->mutex_handle);
        }
    }
}
#endif
