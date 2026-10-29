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
#include "native_src_bq_common_platform_thread_mutex_posix.h"
#ifdef BQ_POSIX
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_src_bq_common_bq_common.h"
namespace bq {
    namespace platform {
        struct mutex_platform_def {
            pthread_mutex_t mutex_handle;
        };

        // stderr, not log_device_console: the mutex being built may be console_mutex_ itself.
        static void fatal_mutex_error(const char* message, int32_t error_code)
        {
            fprintf(stderr, "BqLog: %s, error code:%" PRId32 "\n", message, error_code);
            fflush(stderr);
            abort();
        }

        static void verify_recursive_mutex_support()
        {
            static bq::platform::atomic<int32_t> verified = { 0 };
            if (0 != verified.load_acquire()) {
                return;
            }
            pthread_mutexattr_t probe_attr;
            pthread_mutexattr_init(&probe_attr);
            int32_t set_type_result = pthread_mutexattr_settype(&probe_attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_t probe_handle;
            int32_t init_result = (0 == set_type_result) ? pthread_mutex_init(&probe_handle, &probe_attr) : set_type_result;
            pthread_mutexattr_destroy(&probe_attr);
            bool supported = (0 == init_result);
            if (supported) {
                // try_lock is mandatory here: lock() would hang on a non-recursive mutex.
                supported = (0 == pthread_mutex_lock(&probe_handle)) && (0 == pthread_mutex_trylock(&probe_handle));
                pthread_mutex_unlock(&probe_handle);
                pthread_mutex_unlock(&probe_handle);
                pthread_mutex_destroy(&probe_handle);
            }
            if (!supported) {
                fatal_mutex_error("recursive pthread mutexes are required but unsupported", init_result);
            }
            verified.store(1, bq::platform::memory_order::release);
        }

        mutex::mutex()
            : reentrant_(true)
        {
            verify_recursive_mutex_support();
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            auto set_type_result = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
            if (0 != set_type_result) {
                fatal_mutex_error("set recursive mutex type failed", set_type_result);
            }
            auto mutex_init_result = pthread_mutex_init(&platform_data_->mutex_handle, &mutex_attr);
            if (0 != mutex_init_result) {
                fatal_mutex_error("create recursive mutex failed", mutex_init_result);
            }
        }

        mutex::mutex(bool reentrant /* = true */)
            : reentrant_(reentrant)
        {
            if (reentrant) {
                verify_recursive_mutex_support();
            }
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            auto set_type_result = pthread_mutexattr_settype(&mutex_attr, reentrant ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_ERRORCHECK);
            if (0 != set_type_result) {
                fatal_mutex_error("set mutex type failed", set_type_result);
            }
            auto mutex_init_result = pthread_mutex_init(&platform_data_->mutex_handle, &mutex_attr);
            if (0 != mutex_init_result) {
                fatal_mutex_error("create mutex failed", mutex_init_result);
            }
        }

        mutex::~mutex()
        {
            auto mutex_destroy_result = pthread_mutex_destroy(&platform_data_->mutex_handle);
            if (0 != mutex_destroy_result) {
                bq::util::log_device_console(log_level::fatal, "%s : %" PRId32 " : destroy mutex failed, error code:%" PRId32, __FILE__, __LINE__, mutex_destroy_result);
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void mutex::lock()
        {
            auto lock_result = pthread_mutex_lock(&platform_data_->mutex_handle);
            if (0 != lock_result) {
                if (EDEADLK == lock_result) {
                    // Running on without the lock lets the inner unlock release the outer owner.
                    bq::util::log_device_console(log_level::fatal, "%s : %" PRId32 " : reentered a non-recursive mutex, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    abort();
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %" PRId32 " : pthread mutex lock error, error code:%" PRId32 ", called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, lock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
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
                    bq::util::log_device_console(log_level::error, "%s : %" PRId32 " : you're try to reenter a non-recursive mutex, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    assert(false && "mutex recursive lock");
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %" PRId32 " : pthread mutex lock error, error code:%" PRId32 ", called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, lock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                }
            }
            return 0 == lock_result;
        }

        bool mutex::unlock()
        {
            auto unlock_result = pthread_mutex_unlock(&platform_data_->mutex_handle);
            if (0 != unlock_result) {
                if (EPERM == unlock_result) {
                    bq::util::log_device_console(log_level::error, "%s : %" PRId32 " : you're try to unlock mutex does not belongs to caller thread, called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
                    assert(false && "mutex recursive lock");
                } else {
                    bq::util::log_device_console(log_level::error, "%s : %" PRId32 " : pthread mutex unlock error, error code:%" PRId32 ", called from thread id:%" PRIu64 ", name:\"%s\"", __FILE__, __LINE__, unlock_result, bq::platform::thread::get_current_thread_id(), bq::platform::thread::get_current_thread_name().c_str());
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
