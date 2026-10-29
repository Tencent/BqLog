/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "native_src_bq_common_platform_console_misc.h"
#if defined(BQ_PS) || defined(BQ_SWITCH)
#include <time.h>
#include "native_src_bq_common_bq_common.h"
namespace bq {
    namespace platform {
        uint64_t system_epoch_ms()
        {
            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
                return 0;
            }
            return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
        }
    }
}
#endif
