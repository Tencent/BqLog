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
#include "bq_common/platform/ps_misc.h"
#if defined(BQ_PS)
#include "bq_common/bq_common.h"
#include <time.h>
#if defined(__has_include)
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define BQ_PS_EXECINFO_SUPPORTED 1
#endif
#endif
namespace bq {
    BQ_TLS_NON_POD(bq::string, stack_trace_current_str_)
    BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_)
    namespace platform {
        uint64_t high_performance_epoch_ms()
        {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t epoch_milliseconds = (uint64_t)(ts.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec) / 1000000;
            return epoch_milliseconds;
        }

        base_dir_initializer::base_dir_initializer()
        {
            set_base_dir_0("./");
            set_base_dir_1("./");
        }

#if defined(BQ_PS_EXECINFO_SUPPORTED)
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            stack_trace_str_ref.clear();
            void* buffer[128];
            // Note: backtrace() returns int on glibc-like systems but size_t on
            // FreeBSD-like libcs (such as PS5), keep auto/decltype for both.
            auto stack_count = backtrace(buffer, 128);
            char** stacks = backtrace_symbols(buffer, stack_count);
            uint32_t valid_frame_count = 0;
            if (stacks) {
                for (decltype(stack_count) i = 0; i < stack_count; i++) {
                    if (valid_frame_count == 0) {
                        if (strstr(stacks[i], "get_stack_trace")) {
                            continue;
                        }
                        valid_frame_count = 1;
                    }
                    if (valid_frame_count++ <= skip_frame_count) {
                        continue;
                    }
                    stack_trace_str_ref.push_back('\n');
                    auto str_len = strlen(stacks[i]);
                    stack_trace_str_ref.insert_batch(stack_trace_str_ref.end(), stacks[i], (size_t)str_len);
                }
                free(stacks);
            }
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
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
            stack_trace_str_ref.erase(stack_trace_str_ref.begin() + static_cast<bq::u16string::difference_type>(encoded_size), stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }
#else
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char not_supported_str[] = "stack trace is not supported on this platform";
            out_str_ptr = not_supported_str;
            out_char_count = (uint32_t)(sizeof(not_supported_str) / sizeof(not_supported_str[0]) - 1);
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            (void)skip_frame_count;
            static const char16_t not_supported_str[] = u"stack trace is not supported on this platform";
            out_str_ptr = not_supported_str;
            out_char_count = (uint32_t)(sizeof(not_supported_str) / sizeof(not_supported_str[0]) - 1);
        }
#endif
    }
}
#endif
