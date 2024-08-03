#pragma once
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
 * \file thread.h
 *
 * \author pippocao
 * \date 2022/09/15
 *
 * simple substitute of std::thread.
 * we exclude STL and libc++ to reduce the final executable and library file size
 *
 * inherent thread class and implement the virtual callback functions.
 * call start to begin thread.
 */
#include "bq_common/platform/macros.h"
#include <stdint.h>
#include "bq_common/types/string.h"
#include "bq_common/platform/atomic/atomic.h"
#if BQ_JAVA
#include <jni.h>
#endif

namespace bq {
    namespace platform {
        enum class enum_thread_status : uint8_t {
            init,
            running,
            pendding_cancel, // has received cancel command, and waiting thread exit by self.
            finished,
            released,
            error
        };

        struct thread_attr {
            // 0.5M default stack size;  Only work on posix systems, such as mac, linux and android,
            // windows always expand it's thread stack size dynamically.
            size_t max_stack_size = 512 * 1024;
        };

        class thread {
        public:
            typedef uint64_t thread_id;

        public:
            thread(thread_attr attr = thread_attr());

            thread(const thread& rhs) = delete;

            thread(thread&& rhs) = delete;

#if BQ_JAVA
            void attach_to_jvm();
            JNIEnv* get_jni_env();
#endif

            thread& operator=(const thread& rhs) = delete;

            thread& operator=(thread&& rhs) = delete;

            void set_thread_name(const bq::string& thread_name);

            // call start to begin thread
            void start();

            void join();

            void cancel();

            bool is_cancelled();

            static void yield();

            static void cpu_relax();

            static void sleep(uint64_t millsec);

            // get thread name of current thread which calls this function
            static bq::string get_current_thread_name();

            // get thread id of current thread which calls this function
            static thread_id get_current_thread_id();

            // get thread id of this bq::platform::thread instance.
            thread_id get_thread_id() const
            {
                return thread_id_;
            }

            const bq::string& get_thread_name() const
            {
                return thread_name_;
            }

            enum_thread_status get_status() const
            {
                return status_.load(memory_order::seq_cst);
            }

            virtual ~thread();

        protected:
            // main function you should implement in your own class.
            // it will be executed in new created thread.
            // when this function is returned, this thread will be exit.
            // and on_finished will be called.
            virtual void run() = 0;

            // called when you thread finished;
            virtual void on_finished() { }

        private:
            void internal_run();

            void apply_thread_name();
            void apply_name_raise();

        private:
            bq::string thread_name_;

            thread_id thread_id_;

            const thread_attr attr_;

            bq::platform::atomic<enum_thread_status> status_;

            struct thread_platform_def* platform_data_;

            friend struct thread_platform_processor;
        };
    }
}
