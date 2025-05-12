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
#include <signal.h>

// Modern BSDs need these to compile properly
#if !defined(BQ_APPLE) && !defined(BQ_PS) && defined(BQ_UNIX)
#include <pthread_np.h>
#ifndef pthread_setname_np
#define pthread_setname_np pthread_set_name_np
#define PR_SET_NAME pthread_self()
#endif
#ifndef pthread_getname_np
#define pthread_getname_np pthread_get_name_np
#define PR_GET_NAME pthread_self()
#endif
#endif

#include <sched.h>
#include <sys/select.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#if !defined(BQ_APPLE) && !defined(BQ_PS) && !defined(BQ_UNIX)
#include <sys/prctl.h>
#endif
#if BQ_JAVA
#include <jni.h>
#endif
namespace bq {
    namespace platform {
        struct thread_platform_def {
            pthread_t thread_handle;
#if BQ_JAVA
            JavaVM* jvm = nullptr;
            JNIEnv* jenv = nullptr;
#endif
        };

        struct thread_platform_processor {
            static void* thread_process(void* data)
            {
                thread* thread_ptr = (thread*)data;
                // double setting 1
                thread_ptr->thread_id_ = thread::get_current_thread_id();
                thread_ptr->internal_run();
                return nullptr;
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

#if BQ_JAVA
        void thread::attach_to_jvm()
        {
            assert(status_.load() == enum_thread_status::init && "attach_to_jvm can only be called before thread is running!");
            platform_data_->jvm = get_jvm();
        }

        JNIEnv* thread::get_jni_env()
        {
            auto current_thread_id = get_current_thread_id();
            auto thread_id = get_thread_id();
            assert(current_thread_id == thread_id && "bq_thread::get_jni_env can only be called in the thread which is created by bq_thread instance!");
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
                bq::util::log_device_console(log_level::warning, "trying to start a thread \"%s\" which is still running, thread id :%" PRIu64 ", thread status:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setstacksize(&attr, attr_.max_stack_size);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            auto create_result = pthread_create(&platform_data_->thread_handle, &attr, &thread_platform_processor::thread_process, (void*)this);
            pthread_attr_destroy(&attr);
            if (create_result == EAGAIN) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:EAGAIN", thread_name_.c_str());
            } else if (create_result == EINVAL) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:EINVAL", thread_name_.c_str());
            } else if (create_result == EPERM) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:EPERM", thread_name_.c_str());
            } else if (create_result != 0) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, error code:%d", thread_name_.c_str(), create_result);
            }
            if (create_result != 0) {
                return;
            }
            thread_id_ = (thread_id)(platform_data_->thread_handle);
            auto expected_status = enum_thread_status::init;
            status_.compare_exchange_strong(expected_status, enum_thread_status::running);
        }

        void thread::join()
        {
            auto join_result = pthread_join(platform_data_->thread_handle, nullptr);
            if (ESRCH == join_result) {
                // maybe thread is not created yet.
                return;
            } else if (0 != join_result) {
                bq::util::log_device_console(log_level::error, "join thread \"%s\" failed, thread_id:%" PRIu64 ", error code:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), join_result);
            }
        }

        void thread::yield()
        {
            sleep(0);
        }

        void thread::cpu_relax()
        {
#if BQ_ARM
#if BQ_CLANG
            __builtin_arm_yield();
#else
            __asm__ __volatile__("yield" : : : "memory");
#endif
#else
            __builtin_ia32_pause();
#endif
        }

        void thread::sleep(uint64_t millsec)
        {
            struct timeval sleep_time;
            sleep_time.tv_sec = (decltype(sleep_time.tv_sec))(millsec / 1000);
            sleep_time.tv_usec = (decltype(sleep_time.tv_usec))(millsec % 1000) * 1000;
            select(0, nullptr, nullptr, nullptr, &sleep_time);
        }

        bq::string thread::get_current_thread_name()
        {
            char name_temp[256];
#ifdef BQ_APPLE
            auto result_code = pthread_getname_np(pthread_self(), name_temp, sizeof(name_temp));
            if (0 != result_code) {
                bq::util::log_device_console(log_level::error, "failed to get current thread name, error code:%d", result_code);
                return "";
            }
#elif BQ_PS
            // TODO
#elif BQ_UNIX
            pthread_getname_np(PR_GET_NAME, name_temp, sizeof(name_temp));
#else
            prctl(PR_GET_NAME, name_temp);
#endif

            return name_temp;
        }

        static BQ_TLS thread::thread_id current_thread_id_;
        thread::thread_id thread::get_current_thread_id()
        {
            if (!current_thread_id_) {
                current_thread_id_ = (thread_id)(pthread_self());
            }
            return current_thread_id_;
        }

        bool thread::is_thread_alive(thread_id id)
        {
            if (id == 0) {
                return false;
            }
#if BQ_LINUX || BQ_ANDROID
            assert(false && "is_thread_alive is reliable implemented on Linux");
#endif
            int32_t result = pthread_kill((pthread_t)id, 0);
            return result == 0;
        }

        thread::~thread()
        {
            // auto current_status = status_.load();
            // This is not robust when program is exited by error
            // assert(current_status != enum_thread_status::running && "thread instance is destructed when thread is still running");
            platform_data_->~thread_platform_def();
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void thread::apply_thread_name()
        {
#ifdef BQ_APPLE
            pthread_setname_np(thread_name_.c_str());
#elif BQ_PS
            // TODO
#elif BQ_UNIX
            pthread_setname_np(PR_SET_NAME, thread_name_.c_str());
#else
            prctl(PR_SET_NAME, thread_name_.c_str());
#endif
        }

        void thread::internal_run()
        {
#if BQ_JAVA
            if (platform_data_->jvm) {
                using attach_param_type = function_argument_type_t<decltype(&JavaVM::AttachCurrentThread), 0>;
                jint result = platform_data_->jvm->AttachCurrentThread(reinterpret_cast<attach_param_type>(&platform_data_->jenv), NULL);
                if (result != JNI_OK) {
                    bq::util::log_device_console(bq::log_level::error, "failed to JavaVM AttachCurrentThread, result code %d", result);
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
#if BQ_JAVA
            if (platform_data_->jvm) {
                platform_data_->jvm->DetachCurrentThread();
            }
#endif
            status_.store_seq_cst(enum_thread_status::released);
        }
    }
}
#endif
