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
        struct condition_variable_platform_def {
            HANDLE condition_variable_handle = CreateEvent(nullptr, false, 0, nullptr);
        };

        condition_variable::condition_variable()
        {
#pragma warning(push)
#pragma warning(disable : 6011)
            platform_data_ = (condition_variable_platform_def*)malloc(sizeof(condition_variable_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) condition_variable_platform_def();
            if (!platform_data_->condition_variable_handle) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : create condition_variable failed, error code:%d", __FILE__, __LINE__, GetLastError());
                assert(false && "create condition_variable failed, see the device log output for more information");
                return;
            }
#pragma warning(pop)
        }

        condition_variable::~condition_variable()
        {
            if (platform_data_->condition_variable_handle) {
                CloseHandle(platform_data_->condition_variable_handle);
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void condition_variable::wait(bq::platform::mutex& lock)
        {
            lock.unlock();
            WaitForSingleObjectEx(platform_data_->condition_variable_handle, INFINITE, true);
            lock.lock();
        }

        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms)
        {
            lock.unlock();
            auto result = WaitForSingleObjectEx(platform_data_->condition_variable_handle, (DWORD)wait_time_ms, true);
            lock.lock();
            if (result == WAIT_TIMEOUT) {
                return false;
            }
            return true;
        }

        void condition_variable::notify_one() noexcept
        {
            SetEvent(platform_data_->condition_variable_handle);
        }

        // TODO: this did not notify all the threads waiting who are waitting for this event.
        void condition_variable::notify_all() noexcept
        {
            SetEvent(platform_data_->condition_variable_handle);
        }
    }
}
#endif
