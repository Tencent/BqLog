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
 /*!
  * \file rwlock_windows.cpp
  *
  * \author pippocao
  * \date 2025/08/26
  */

#include "bq_common/platform/thread/rwlock.h"

#ifdef _WIN32
#include <windows.h>

namespace bq {
    namespace platform {

        struct rwlock_platform_def {
            SRWLOCK lock;
        };

        rwlock::rwlock()
            : platform_data_(0)
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)malloc(sizeof(rwlock_platform_def));
            if (impl) {
               InitializeSRWLock(&impl->lock);
            }
            platform_data_ = impl;
        }

        rwlock::~rwlock()
        {
            if (platform_data_) {
                // SRWLOCK does not need explicit destruction.
                free(platform_data_);
                platform_data_ = 0;
            }
        }

        void rwlock::read_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            AcquireSRWLockShared(&impl->lock);
        }

        bool rwlock::try_read_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            auto ok =TryAcquireSRWLockShared(&impl->lock);
            return ok ? true : false;
        }

        bool rwlock::read_unlock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            ReleaseSRWLockShared(&impl->lock);
            return true;
        }

        void rwlock::write_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            AcquireSRWLockExclusive(&impl->lock);
        }

        bool rwlock::try_write_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            auto ok =TryAcquireSRWLockExclusive(&impl->lock);
            return ok ? true : false;
        }

        bool rwlock::write_unlock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            ReleaseSRWLockExclusive(&impl->lock);
            return true;
        }

        void* rwlock::get_platform_handle()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            return (void*)&impl->lock;
        }

    }
}

#endif // _WIN32