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
#include "bq_common/platform/thread/mutex_nx.h"
// Nintendo Switch, official Nintendo SDK (nn::os) implementation.
// All nn:: calls below follow the public open-ead/nnheaders declarations.
#if defined(BQ_SWITCH_NN)
#include "bq_common/bq_common.h"
#include <nn/os.h>
namespace bq {
    namespace platform {
        struct mutex_platform_def {
            nn::os::MutexType mutex_handle;
        };

        mutex::mutex()
            : reentrant_(true)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            nn::os::InitializeMutex(&platform_data_->mutex_handle, true, 0);
        }

        mutex::mutex(bool reentrant /* = true */)
            : reentrant_(reentrant)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            nn::os::InitializeMutex(&platform_data_->mutex_handle, reentrant, 0);
        }

        mutex::~mutex()
        {
            nn::os::FinalizeMutex(&platform_data_->mutex_handle);
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void mutex::lock()
        {
            nn::os::LockMutex(&platform_data_->mutex_handle);
        }

        bool mutex::try_lock()
        {
            return nn::os::TryLockMutex(&platform_data_->mutex_handle);
        }

        bool mutex::unlock()
        {
            nn::os::UnlockMutex(&platform_data_->mutex_handle);
            return true;
        }

        void* mutex::get_platform_handle()
        {
            return (void*)(&platform_data_->mutex_handle);
        }
    }
}
#endif
