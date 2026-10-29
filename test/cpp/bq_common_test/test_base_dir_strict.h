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
#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"

#if BQ_HAS_FORK
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace bq {
    namespace test {
        class test_base_dir_strict : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;
#if !BQ_HAS_FORK
                result.add_result(true, "unset base dir termination needs fork()");
#else
                const pid_t child = fork();
                if (child == 0) {
                    // The crash handler would flush logs first, and flushing needs the base dir.
                    signal(SIGABRT, SIG_DFL);
                    signal(SIGALRM, SIG_DFL);
                    alarm(5);
                    // Another parent thread may have held the console mutex at fork time.
                    bq::util::set_log_device_console_min_level(bq::log_level::log_level_max);
                    bq::platform::test_inject::set_base_dir_unset(true);
                    const bq::string path = TO_ABSOLUTE_PATH("strict_probe.log", 0);
                    (void)path;
                    _exit(0);
                }
                if (child < 0) {
                    result.add_result(false, "fork for the unset base dir check");
                    return result;
                }
                int32_t status = 0;
                waitpid(child, &status, 0);
                result.add_result(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT,
                    "unset base dir terminates the process (wait status:%" PRId32 ")", status);
#endif
                return result;
            }
        };
    }
}
