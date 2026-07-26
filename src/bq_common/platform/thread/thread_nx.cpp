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
#include "bq_common/platform/thread/thread_nx.h"
// Nintendo Switch, official Nintendo SDK (nn::os) implementation.
// All nn:: calls below follow the public open-ead/nnheaders declarations.
#if defined(BQ_SWITCH_NN)
#include "bq_common/bq_common.h"
#include <nn/os.h>
#include <stdlib.h>

namespace bq {
    namespace platform {
        // Default thread priority for SDK threads (Horizon priorities run
        // 0(highest)..63(lowest); 16 is the usual default for app threads).
        static constexpr int32_t nx_default_thread_priority = 16;

        struct thread_platform_def {
            nn::os::ThreadType thread_obj;
            void* stack_mem;
            bool thread_created; // CreateThread succeeded and DestroyThread not yet called

            thread_platform_def()
                : stack_mem(nullptr)
                , thread_created(false)
            {
            }
        };

        struct thread_platform_processor {
            static void thread_process(void* data)
            {
                thread* thread_ptr = (thread*)data;
                // double setting 1
                thread_ptr->thread_id_ = thread::get_current_thread_id();
                thread_ptr->internal_run();
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

        void thread::set_thread_name(const bq::string& thread_name)
        {
            thread_name_ = thread_name;
            if (thread_name_.size() >= 15) {
                bq::string truncated_name = thread_name_.substr(0, 15);
                bq::util::log_device_console(bq::log_level::warning, "thread name \"%s\" exceed max length limit of android, the length of thread name can not be larger than 15. so it will be truncated to \"%s\"", thread_name_.c_str(), truncated_name.c_str());
                thread_name_ = truncated_name;
            }
            auto current_status = status_.load();
            if (current_status == enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to set thread name \"%s\" when thread have already been running, thread id :%" PRIu64 ", thread status:%" PRId32, thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
            }
        }

        void thread::start()
        {
            auto current_status = status_.load();
            if (current_status == enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to start a thread \"%s\" which is still running, thread id :%" PRIu64 ", thread status:%" PRId32, thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            if (current_status == enum_thread_status::detached) {
                bq::util::log_device_console(log_level::fatal, "trying to start a thread \"%s\" which is detached, thread id :%" PRIu64 ", thread status:%" PRId32, thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                assert(false && "trying to start a detached thread");
                return;
            }
            // nn::os requires the caller to provide the thread stack.
            const size_t stack_alignment = 4096;
            size_t stack_size = attr_.max_stack_size;
            stack_size = (stack_size + stack_alignment - 1) / stack_alignment * stack_alignment;
            platform_data_->stack_mem = ::aligned_alloc(stack_alignment, stack_size);
            if (!platform_data_->stack_mem) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, can not allocate thread stack", thread_name_.c_str());
                return;
            }
            nn::Result create_result = nn::os::CreateThread(&platform_data_->thread_obj,
                &thread_platform_processor::thread_process, (void*)this,
                platform_data_->stack_mem, static_cast<u64>(stack_size),
                nx_default_thread_priority);
            if (create_result.IsFailure()) {
                bq::util::log_device_console(log_level::fatal, "create thread \"%s\" failed, nn::os::CreateThread error code:%" PRId32, thread_name_.c_str(), static_cast<int32_t>(create_result.GetInnerValueForDebug()));
                ::free(platform_data_->stack_mem);
                platform_data_->stack_mem = nullptr;
                return;
            }
            platform_data_->thread_created = true;
            thread_id_ = static_cast<thread_id>(nn::os::GetThreadId(&platform_data_->thread_obj));
            nn::os::StartThread(&platform_data_->thread_obj);

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
            if (!platform_data_->thread_created) {
                // maybe thread is not created yet, or already joined.
                return;
            }
            nn::os::WaitThread(&platform_data_->thread_obj);
            nn::os::DestroyThread(&platform_data_->thread_obj);
            platform_data_->thread_created = false;
            ::free(platform_data_->stack_mem);
            platform_data_->stack_mem = nullptr;
        }

        void thread::detach()
        {
            auto current_status = status_.load();
            if (current_status != enum_thread_status::running) {
                bq::util::log_device_console(log_level::warning, "trying to detach a thread \"%s\" which is not running, thread id :%" PRIu64 ", thread status:%" PRId32, thread_name_.c_str(), static_cast<uint64_t>(thread_id_), (int32_t)current_status);
                return;
            }
            // nn::os has no detach concept: ThreadType and its stack must stay
            // alive until the thread exits. This only marks the status; the
            // destructor still WaitThread()s before releasing them, so on
            // Switch a bq::platform::thread instance must outlive its thread.
            status_.store_seq_cst(enum_thread_status::detached);
        }

        void thread::yield()
        {
            nn::os::YieldThread();
        }

        void thread::cpu_relax()
        {
            __asm__ __volatile__("yield");
        }

        void thread::sleep(uint64_t millsec)
        {
            nn::os::SleepThread(nn::TimeSpan::FromMilliSeconds(millsec));
        }

        bq::string thread::get_current_thread_name()
        {
            nn::os::ThreadType* current_thread = nn::os::GetCurrentThread();
            char* thread_name = current_thread ? nn::os::GetThreadNamePointer(current_thread) : nullptr;
            if (thread_name && thread_name[0] != '\0') {
                return bq::string(thread_name);
            }
            char thread_name_buf[64] = { 0 };
            snprintf(thread_name_buf, sizeof(thread_name_buf), "nx_thread_%" PRIu64, static_cast<uint64_t>(get_current_thread_id()));
            return thread_name_buf;
        }

        static BQ_TLS thread::thread_id current_thread_id_;
        thread::thread_id thread::get_current_thread_id()
        {
            if (!current_thread_id_) {
                current_thread_id_ = static_cast<thread_id>(nn::os::GetThreadId(nn::os::GetCurrentThread()));
            }
            return current_thread_id_;
        }

        thread::~thread()
        {
            if (platform_data_->thread_created) {
                if (nn::os::GetCurrentThread() != &platform_data_->thread_obj) {
                    // Unlike pthreads, ThreadType and its stack live in our
                    // allocation, so they can only be released after the thread
                    // has exited. BqLog cancels and joins its worker threads
                    // before destruction, so this wait returns immediately in
                    // practice.
                    nn::os::WaitThread(&platform_data_->thread_obj);
                    nn::os::DestroyThread(&platform_data_->thread_obj);
                    platform_data_->thread_created = false;
                    ::free(platform_data_->stack_mem);
                    platform_data_->stack_mem = nullptr;
                } else {
                    // A thread object must never be destroyed from inside its
                    // own thread on Switch; leak the stack rather than let the
                    // kernel reference freed memory.
                }
            }
            platform_data_->~thread_platform_def();
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void thread::apply_thread_name()
        {
            // SetThreadName copies the string into the thread's own name
            // buffer (32 bytes; set_thread_name() already truncated to 15).
            nn::os::SetThreadName(nn::os::GetCurrentThread(), thread_name_.c_str());
        }

        void thread::internal_run()
        {
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
            status_.store_seq_cst(enum_thread_status::released);
        }
    }
}
#endif
