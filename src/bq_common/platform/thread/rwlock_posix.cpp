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
  * \file rwlock_posix.cpp
  *
  * \author pippocao
  * \date 2025/08/26
  */

#include "bq_common/platform/thread/rwlock.h"

#ifdef BQ_POSIX
#include <pthread.h>

namespace bq {
    namespace platform {

        struct rwlock_platform_def {
            pthread_rwlock_t lock;
        };

        rwlock::rwlock()
            : platform_data_(0)
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)malloc(sizeof(rwlock_platform_def));
            if (impl) {
                pthread_rwlockattr_t attr;
                // Default attributes
                if (pthread_rwlockattr_init(&attr) == 0) {
                    pthread_rwlock_init(&impl->lock, &attr);
                    pthread_rwlockattr_destroy(&attr);
                }
                else {
                    // Fallback: try default init even if attr init fails
                    pthread_rwlock_init(&impl->lock, 0);
                }
            }
            platform_data_ = impl;
        }

        rwlock::~rwlock()
        {
            if (platform_data_) {
                rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
                pthread_rwlock_destroy(&impl->lock);
                free(platform_data_);
                platform_data_ = 0;
            }
        }

        void rwlock::read_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            pthread_rwlock_rdlock(&impl->lock);
        }

        bool rwlock::try_read_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            int r = pthread_rwlock_tryrdlock(&impl->lock);
            return (r == 0);
        }

        bool rwlock::read_unlock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            int r = pthread_rwlock_unlock(&impl->lock);
            return (r == 0);
        }

        void rwlock::write_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            pthread_rwlock_wrlock(&impl->lock);
        }

        bool rwlock::try_write_lock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            int r = pthread_rwlock_trywrlock(&impl->lock);
            return (r == 0);
        }

        bool rwlock::write_unlock()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            int r = pthread_rwlock_unlock(&impl->lock);
            return (r == 0);
        }

        void* rwlock::get_platform_handle()
        {
            rwlock_platform_def* impl = (rwlock_platform_def*)platform_data_;
            return (void*)&impl->lock;
        }

    }
}

#endif