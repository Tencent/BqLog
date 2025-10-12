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
#include "bq_common/platform/ohos_misc.h"

#if defined(BQ_OHOS)
#include <bundle/native_interface_bundle.h>
#include <pthread.h>
#include <time.h>
#include <unwind.h>
#include <dlfcn.h>
#include <string.h>
#include "bq_common/bq_common.h"


namespace bq {
    BQ_TLS_NON_POD(bq::string, stack_trace_current_str_);
    BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_);
    namespace platform {
        // According to test result, benefit from VDSO.
        //"CLOCK_REALTIME_COARSE clock_gettime"  has higher performance
        //  than "gettimeofday" and event "TSC" on Android and Linux.
        uint64_t high_performance_epoch_ms()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            uint64_t epoch_milliseconds = (uint64_t)(ts.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec) / 1000000;
            return epoch_milliseconds;
        }

        base_dir_initializer::base_dir_initializer()
        {
            set_base_dir_0("/data/storage/el2/base/files");
            set_base_dir_1("/data/storage/el2/base/cache");
        }
        const bq::string get_bundle_name()
        {
            auto bundle = OH_NativeBundle_GetCurrentApplicationInfo();
            return bundle.bundleName;
        }


        static constexpr size_t max_stack_size_ = 128;

        struct android_backtrace_state {
            void** current;
            void** end;
        };

        static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
        {
            android_backtrace_state* state = static_cast<android_backtrace_state*>(arg);
            uintptr_t pc = _Unwind_GetIP(context);
            if (pc) {
                if (state->current == state->end) {
                    return _URC_END_OF_STACK;
                } else {
                    *state->current++ = reinterpret_cast<void*>(pc);
                }
            }
            return _URC_NO_REASON;
        }

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            stack_trace_str_ref.clear();
            void* buffer[max_stack_size_];
            android_backtrace_state state = { buffer, buffer + max_stack_size_ };
            _Unwind_Backtrace(unwindCallback, &state);
            size_t stack_count = static_cast<size_t>(state.current - buffer);
            uint32_t valid_frame_count = 0;
            for (int32_t idx = static_cast<int32_t>(skip_frame_count); idx < static_cast<int32_t>(stack_count); ++idx) {
                const void* addr = buffer[idx];
                const char* symbol = "(unknown symbol)";
                Dl_info info;
                if (dladdr(addr, &info) && info.dli_sname) {
                    symbol = info.dli_sname;
                }
                if (valid_frame_count == 0) {
                    if (strstr(symbol, "get_stack_trace")) {
                        continue;
                    }
                    valid_frame_count = 1;
                }
                if (valid_frame_count++ <= skip_frame_count) {
                    continue;
                }
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "\n#%d %p ", idx, addr);
                stack_trace_str_ref += tmp;
                stack_trace_str_ref += symbol;
            }
            out_str_ptr = stack_trace_str_ref.c_str();
            out_char_count = static_cast<uint32_t>(stack_trace_str_ref.size());
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_u16_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::u16string& stack_trace_str_ref = bq::stack_trace_current_str_u16_.get();
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_str_ref.clear();
            stack_trace_str_ref.fill_uninitialized((u8_char_count << 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_str_ref.begin(), (uint32_t)stack_trace_str_ref.size());
            assert(encoded_size < stack_trace_str_ref.size());
            stack_trace_str_ref.erase(stack_trace_str_ref.begin() + static_cast<ptrdiff_t>(encoded_size), stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }
    }
}
#endif
