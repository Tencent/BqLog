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
#include "bq_common/platform/ps_misc.h"
#if defined(BQ_PS)
#include "bq_common/bq_common.h"
#include <time.h>
namespace bq {
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

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            const char* empty_str = "TODO";
            out_str_ptr = empty_str;
            out_char_count = 4;
        }
        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            const char16_t* empty_str = u"TODO";
            out_str_ptr = empty_str;
            out_char_count = 4;
        }
    }
}
#endif
