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
#include "bq_common/platform/thread/condition_variable_nx.h"
// Nintendo Switch, official Nintendo SDK (nn::os) implementation.
// All nn:: calls below follow the public open-ead/nnheaders declarations.
#if defined(BQ_SWITCH_NN)
#include "bq_common/bq_common.h"
#include <nn/os.h>

namespace bq {
    namespace platform {
        struct condition_variable_platform_def {
            nn::os::ConditionVariableType cond_handle;
        };

        condition_variable::condition_variable()
        {
            platform_data_ = (condition_variable_platform_def*)malloc(sizeof(condition_variable_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) condition_variable_platform_def();
            nn::os::InitializeConditionVariable(&platform_data_->cond_handle);
        }

        condition_variable::~condition_variable()
        {
            if (platform_data_) {
                nn::os::FinalizeConditionVariable(&platform_data_->cond_handle);
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void condition_variable::wait(bq::platform::mutex& lock)
        {
            // Per the SDK, WaitConditionVariable releases the nn::os mutex the
            // calling thread currently holds (Horizon ties the wait to the
            // thread's critical section) and re-acquires it before returning;
            // the caller must therefore hold lock, a bq::platform::mutex
            // wrapping nn::os::MutexType.
            (void)lock;
            nn::os::WaitConditionVariable(&platform_data_->cond_handle);
        }

        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms)
        {
            nn::os::MutexType* native_mutex = (nn::os::MutexType*)(lock.get_platform_handle());
            u8 wait_result = nn::os::TimedWaitConditionVariable(&platform_data_->cond_handle,
                native_mutex, nn::TimeSpan::FromMilliSeconds(wait_time_ms));
            return wait_result != nn::os::ConditionVariableStatus_Timeout;
        }

        void condition_variable::notify_one() noexcept
        {
            nn::os::SignalConditionVariable(&platform_data_->cond_handle);
        }

        void condition_variable::notify_all() noexcept
        {
            nn::os::BroadcastConditionVariable(&platform_data_->cond_handle);
        }

    } // namespace platform
} // namespace bq
#endif
