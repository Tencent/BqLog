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
#include "bq_common/platform/switch_misc.h"
#if defined(BQ_SWITCH_LIBNX)
#include "bq_common/bq_common.h"
#include <time.h>
namespace bq {
    namespace platform {
        uint64_t high_performance_epoch_ms()
        {
            // Follows the ps_misc precedent (CLOCK_MONOTONIC); this clock feeds
            // log timestamps and performance intervals.
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t epoch_milliseconds = (uint64_t)(ts.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec) / 1000000;
            return epoch_milliseconds;
        }

        base_dir_initializer::base_dir_initializer()
        {
            // On libnx the process working directory is the directory of the
            // .nro on the SD card (devoptab routes relative paths to sdmc:/).
            set_base_dir_0("./");
            set_base_dir_1("./");
        }

        // newlib on devkitA64 ships no <execinfo.h>, so there is no backtrace.
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
    }
}
#endif
