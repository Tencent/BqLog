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
#include "bq_common/platform/linux_misc.h"
#if defined(BQ_LINUX)
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include "bq_common/bq_common.h"
namespace bq {
    namespace platform {
        // According to test result, benifit from VDSO.
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
            bq::array<char> tmp;
            tmp.fill_uninitialized(1024);
            while (getcwd(&tmp[0], tmp.size()) == NULL) {
                tmp.fill_uninitialized(1024);
            }
            set_base_dir_0(&tmp[0]);
            set_base_dir_1(&tmp[0]);
        }

        bool share_file(const char* file_path)
        {
            (void)file_path;
            return false;
        }
    }
}
#endif
