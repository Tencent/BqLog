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
#ifdef BQ_POSIX
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
namespace bq {
    namespace platform {
        struct mutex_platform_def {
            pthread_mutex_t mutex_handle;
        };

        mutex::mutex()
            : reentrant_(true)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
            auto mutex_init_result = pthread_mutex_init(&platform_data_->mutex_handle, &mutex_attr);
            if (0 != mutex_init_result) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : create mutex failed, error code:%d", __FILE__, __LINE__, mutex_init_result);
                assert(false && "create mutex failed, see the device log output for more information");
                return;
            }
        }

        mutex::mutex(bool reentrant /* = true */)
            : reentrant_(reentrant)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            if (reentrant) {
                pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
            } else {
                pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
            }
            auto mutex_init_result = pthread_mutex_init(&platform_data_->mutex_handle, &mutex_attr);
            if (0 != mutex_init_result) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : create mutex failed, error code:%d", __FILE__, __LINE__, mutex_init_result);
                assert(false && "create mutex failed, see the device log output for more information");
                return;
            }
        }

        mutex::~mutex()
        {
            auto mutex_destroy_result = pthread_mutex_destroy(&platform_data_->mutex_handle);
            if (0 != mutex_destroy_result) {
                bq::util::log_device_console(log_level::fatal, "%s : %d : destroy mutex failed, error code:%d", __FILE__, __LINE__, mutex_destroy_result);
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void mutex::lock()
        {
            auto lock_result = pthread_mutex_lock(&platform_data_->mutex_handle);
            if (0 != lock_result) {
                if (EDEADLK == lock_result) {
                    bq::util::log_device_console(log_level::error, "%s : %d : you're try to reenter a non-recursive mutex, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    assert(false && "mutex recursive lock");
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %d : pthread mutex lock error, error code:%d, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, lock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                }
            }
        }

        bool mutex::try_lock()
        {
            auto lock_result = pthread_mutex_trylock(&platform_data_->mutex_handle);
            if (0 != lock_result) {
                if (EBUSY == lock_result) {
                    // already locked, failed to lock
                } else if (EDEADLK == lock_result) {
                    bq::util::log_device_console(log_level::error, "%s : %d : you're try to reenter a non-recursive mutex, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    assert(false && "mutex recursive lock");
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %d : pthread mutex lock error, error code:%d, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, lock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                }
            }
            return 0 == lock_result;
        }

        bool mutex::unlock()
        {
            auto unlock_result = pthread_mutex_unlock(&platform_data_->mutex_handle);
            if (0 != unlock_result) {
                if (EPERM == unlock_result) {
                    bq::util::log_device_console(log_level::error, "%s : %d : you're try to unlock mutex does not belongs to caller thread, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    assert(false && "mutex recursive lock");
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %d : pthread mutex unlock error, error code:%d, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, unlock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                }
            }
            return 0 == unlock_result;
        }

        void* mutex::get_platform_handle()
        {
            return (void*)(&platform_data_->mutex_handle);
        }
    }
}
#endif
