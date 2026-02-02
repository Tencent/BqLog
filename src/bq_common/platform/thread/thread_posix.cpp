/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/platform/thread/thread_posix.h"
#ifdef BQ_POSIX
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#if defined(__has_include)
#if __has_include(<sys/prctl.h>)
#include <sys/prctl.h>
#define BQ_HAVE_DLFCN
#endif
#ifndef BQ_HAVE_DLFCN
#if __has_include(<pthread_np.h>)
#include <pthread_np.h>
#define BQ_HAVE_PTHREAD_NP_UNIX
#else
#define BQ_HAVE_PTHREAD_NP
#endif
#endif

#endif

#if defined(BQ_JAVA)
#include <jni.h>
#endif
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        constexpr size_t thread_name_max_len()
        {
#if defined(BQ_LINUX) || defined(BQ_ANDROID)
            return 16;
#elif defined(BQ_APPLE)
            return 64;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
            return 64;
#else
            return 32;
#endif
        }

        // probes
        template <typename>
        int32_t accept_getname_param_ver1(int32_t (*)(pthread_t, char*, size_t));
        template <typename>
        char accept_getname_param_ver1(...);

        template <typename>
        int32_t accept_getname_param_ver2(void (*)(pthread_t, char*, size_t));
        template <typename>
        char accept_getname_param_ver2(...);

        template <typename>
        int32_t accept_prctl_param(int32_t (*)(int32_t, ...));
        template <typename>
        char accept_prctl_param(...);

        template <typename>
        int32_t accept_setname_param_ver1(int32_t (*)(pthread_t, const char*, void*));
        template <typename>
        char accept_setname_param_ver1(...);

        template <typename>
        int32_t accept_setname_param_ver2(int32_t (*)(pthread_t, const char*));
        template <typename>
        char accept_setname_param_ver2(...);

        template <typename>
        int32_t accept_setname_param_ver3(void (*)(pthread_t, const char*));
        template <typename>
        char accept_setname_param_ver3(...);

        template <typename>
        int32_t accept_setname_param_ver4(int32_t (*)(const char*));
        template <typename>
        char accept_setname_param_ver4(...);

        // 1) pthread_getname_np
        template <typename U, typename = void>
        struct get_thread_name_func_sfinae1 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        struct get_thread_name_func_sfinae1<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_getname_param_ver1<U>(&::pthread_getname_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 2) pthread_get_name_np
        template <typename U, typename = void>
        struct get_thread_name_func_sfinae2 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP_UNIX)
        template <typename U>
        struct get_thread_name_func_sfinae2<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_getname_param_ver2<U>(&::pthread_get_name_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 3) prctl(PR_GET_NAME)
        template <typename U, typename = void>
        struct get_thread_name_func_sfinae3 : bq::false_type { };
#if defined(BQ_HAVE_DLFCN)
        template <typename U>
        struct get_thread_name_func_sfinae3<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_prctl_param<U>(&::prctl)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif

        // 1) pthread_setname_np(pthread_t, const char*, void*)
        template <typename U, typename = void>
        struct set_thread_name_func_sfinae1 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        struct set_thread_name_func_sfinae1<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_setname_param_ver1<U>(&::pthread_setname_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 2) pthread_setname_np(pthread_t, const char*)
        template <typename U, typename = void>
        struct set_thread_name_func_sfinae2 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        struct set_thread_name_func_sfinae2<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_setname_param_ver2<U>(&::pthread_setname_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 3) pthread_set_name_np(pthread_t, const char*)
        template <typename U, typename = void>
        struct set_thread_name_func_sfinae3 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP_UNIX)
        template <typename U>
        struct set_thread_name_func_sfinae3<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_setname_param_ver3<U>(&::pthread_set_name_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 4) pthread_setname_np(const char*)
        template <typename U, typename = void>
        struct set_thread_name_func_sfinae4 : bq::false_type { };
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        struct set_thread_name_func_sfinae4<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_setname_param_ver4<U>(&::pthread_setname_np)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif
        // 5) prctl(PR_SET_NAME)
        template <typename U, typename = void>
        struct set_thread_name_func_sfinae5 : bq::false_type { };
#if defined(BQ_HAVE_DLFCN)
        template <typename U>
        struct set_thread_name_func_sfinae5<U, bq::void_t<bq::enable_if_t<bq::is_same<decltype(accept_prctl_param<U>(&::prctl)), int32_t>::value, int32_t>>> : bq::true_type { };
#endif

#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        bq::enable_if_t<get_thread_name_func_sfinae1<U>::value, bq::string> get_thread_name_impl(U thread_handle)
        {
            char thread_name_buf[thread_name_max_len()];
            int32_t get_name_result = pthread_getname_np(thread_handle, thread_name_buf, sizeof(thread_name_buf));
            if (get_name_result != 0) {
                return "";
            }
            return bq::string(thread_name_buf);
        }
#endif
#if defined(BQ_HAVE_PTHREAD_NP_UNIX)
        template <typename U>
        bq::enable_if_t<!get_thread_name_func_sfinae1<U>::value
                && get_thread_name_func_sfinae2<U>::value,
            bq::string> get_thread_name_impl(U thread_handle)
        {
            char thread_name_buf[thread_name_max_len()] = { 0 };
            pthread_get_name_np(thread_handle, thread_name_buf, sizeof(thread_name_buf));
            return bq::string(thread_name_buf);
        }
#endif
#if defined(BQ_HAVE_DLFCN)
        template <typename U>
        bq::enable_if_t<!get_thread_name_func_sfinae1<U>::value
                && !get_thread_name_func_sfinae2<U>::value
                && get_thread_name_func_sfinae3<U>::value,
            bq::string> get_thread_name_impl(U thread_handle)
        {
            (void)thread_handle;
            assert(thread_handle == pthread_self() && "prctl(PR_GET_NAME) can only get current thread name!");
            char thread_name_buf[thread_name_max_len()] = { 0 };
            int32_t get_name_result = prctl(PR_GET_NAME, thread_name_buf);
            if (get_name_result < 0) {
                return "";
            }
            return bq::string(thread_name_buf);
        }
#endif
        template <typename U>
        bq::enable_if_t<!get_thread_name_func_sfinae1<U>::value
                && !get_thread_name_func_sfinae2<U>::value
                && !get_thread_name_func_sfinae3<U>::value,
            bq::string> get_thread_name_impl(U thread_handle)
        {
            char thread_name_buf[64] = { 0 };
            uint64_t tid = static_cast<uint64_t>(thread_handle);
            snprintf(thread_name_buf, sizeof(thread_name_buf), "pthread_%" PRIu64, tid);
            return thread_name_buf;
        }

#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        bq::enable_if_t<set_thread_name_func_sfinae1<U>::value, bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            using func_type = int32_t (*)(pthread_t, const char*, void*);
            auto func_ptr = reinterpret_cast<func_type>(reinterpret_cast<void*>(&::pthread_setname_np));
            int32_t set_name_result = func_ptr(thread_handle, "%s", const_cast<char*>(thread_name.c_str()));
            return set_name_result == 0;
        }
#endif
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        bq::enable_if_t<!set_thread_name_func_sfinae1<U>::value
                && set_thread_name_func_sfinae2<U>::value,
            bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            using func_type = int32_t (*)(pthread_t, const char*);
            auto func_ptr = reinterpret_cast<func_type>(reinterpret_cast<void*>(&::pthread_setname_np));
            int32_t set_name_result = func_ptr(thread_handle, thread_name.c_str());
            return set_name_result == 0;
        }
#endif
#if defined(BQ_HAVE_PTHREAD_NP_UNIX)
        template <typename U>
        bq::enable_if_t<!set_thread_name_func_sfinae1<U>::value
                && !set_thread_name_func_sfinae2<U>::value
                && set_thread_name_func_sfinae3<U>::value,
            bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            pthread_set_name_np(thread_handle, thread_name.c_str());
            return true;
        }
#endif
#if defined(BQ_HAVE_PTHREAD_NP)
        template <typename U>
        bq::enable_if_t<!set_thread_name_func_sfinae1<U>::value
                && !set_thread_name_func_sfinae2<U>::value
                && !set_thread_name_func_sfinae3<U>::value
                && set_thread_name_func_sfinae4<U>::value,
            bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            (void)thread_handle;
            assert(thread_handle == pthread_self() && "pthread_setname_np can only set current thread name!");
            using func_type = int32_t (*)(const char*);
            func_type func_ptr = reinterpret_cast<func_type>(reinterpret_cast<void*>(&::pthread_setname_np));
            int32_t set_name_result = func_ptr(thread_name.c_str());
            return set_name_result >= 0;
        }
#endif
#if defined(BQ_HAVE_DLFCN)
        template <typename U>
        bq::enable_if_t<!set_thread_name_func_sfinae1<U>::value
                && !set_thread_name_func_sfinae2<U>::value
                && !set_thread_name_func_sfinae3<U>::value
                && !set_thread_name_func_sfinae4<U>::value
                && set_thread_name_func_sfinae5<U>::value,
            bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            (void)thread_handle;
            assert(thread_handle == pthread_self() && "prctl(PR_SET_NAME) can only set current thread name!");
            int32_t set_name_result = prctl(PR_SET_NAME, thread_name.c_str());
            return set_name_result >= 0;
        }
#endif
        template <typename U>
        bq::enable_if_t<!set_thread_name_func_sfinae1<U>::value
                && !set_thread_name_func_sfinae2<U>::value
                && !set_thread_name_func_sfinae3<U>::value
                && !set_thread_name_func_sfinae4<U>::value
                && !set_thread_name_func_sfinae5<U>::value,
            bool> set_thread_name_impl(U thread_handle, const bq::string& thread_name)
        {
            (void)thread_handle;
            (void)thread_name;
        }

        struct thread_platform_def {
            pthread_t thread_handle;
#if defined(BQ_JAVA)
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

#if defined(BQ_JAVA)
        void thread::attach_to_jvm()
        {
            assert(status_.load() == enum_thread_status::init && "attach_to_jvm can only be called before thread is running!");
            platform_data_->jvm = get_jvm();
        }

        JNIEnv* thread::get_jni_env()
        {
            auto current_thread_id = get_current_thread_id();
            auto jni_init_thread_id = get_thread_id();
            assert(current_thread_id == jni_init_thread_id && "bq_thread::get_jni_env can only be called in the thread which is created by bq_thread instance!");
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
            if (current_status == enum_thread_status::detached) {
                bq::util::log_device_console(log_level::fatal, "trying to start a thread \"%s\" which is detached, thread id :%" PRIu64 ", thread status:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                assert(false && "trying to start a detached thread");
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
            
            // If the previous status is init or released, we can set it to running.
            // internal_run() is spinning and waiting for this status change.
            auto expected_status = enum_thread_status::init;
            if (!status_.compare_exchange_strong(expected_status, enum_thread_status::running)) {
                expected_status = enum_thread_status::released;
                status_.compare_exchange_strong(expected_status, enum_thread_status::running);
            }
        }

        void thread::join()
        {
            auto join_result = pthread_join(platform_data_->thread_handle, nullptr);
            if (ESRCH == join_result) {
                // maybe thread is not created yet.
                return;
            } else if (EINVAL == join_result) {
                // maybe thread is detached or already joined.
                return;
            } else if (0 != join_result) {
                bq::util::log_device_console(log_level::error, "join thread \"%s\" failed, thread_id:%" PRIu64 ", error code:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), join_result);
            }
        }

        void thread::detach()
        {
            auto current_status = status_.load();
            if (current_status != enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to detach a thread \"%s\" which is not running, thread id :%" PRIu64 ", thread status:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            auto detach_result = pthread_detach(platform_data_->thread_handle);
            if (detach_result != 0) {
                bq::util::log_device_console(log_level::error, "detach thread \"%s\" failed, thread_id:%" PRIu64 ", error code:%d", thread_name_.c_str(), static_cast<uint64_t>(thread_id_), detach_result);
            }
            status_.store_seq_cst(enum_thread_status::detached);
        }

        void thread::yield()
        {
            sleep(0);
        }

        void thread::cpu_relax()
        {
#if defined(BQ_ARM)
            __asm__ __volatile__("yield");
#else
            __asm__ __volatile__("pause");
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
            return get_thread_name_impl<pthread_t>(pthread_self());
        }

        static BQ_TLS thread::thread_id current_thread_id_;
        thread::thread_id thread::get_current_thread_id()
        {
            if (!current_thread_id_) {
                current_thread_id_ = (thread_id)(pthread_self());
            }
            return current_thread_id_;
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
            set_thread_name_impl<pthread_t>(pthread_self(), thread_name_);
        }

        void thread::internal_run()
        {
#if defined(BQ_JAVA)
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
                && status_.load() != enum_thread_status::pendding_cancel
                && status_.load() != enum_thread_status::detached) {
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
#if defined(BQ_JAVA)
            if (platform_data_->jvm) {
                platform_data_->jvm->DetachCurrentThread();
            }
#endif
            status_.store_seq_cst(enum_thread_status::released);
        }
    }
}
#endif
