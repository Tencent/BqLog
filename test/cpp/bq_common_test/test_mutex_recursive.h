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
        class test_mutex_recursive : public test_base {
        private:
            bool recursive_mutex_reenters()
            {
                bq::platform::mutex holder;
                holder.lock();
                const bool try_ok = holder.try_lock();
                holder.unlock();
                if (!try_ok) {
                    holder.unlock();
                    return false;
                }
                holder.unlock();

                bq::platform::mutex nested;
                nested.lock();
                nested.lock();
                nested.unlock();
                nested.unlock();
                return true;
            }

#if BQ_HAS_FORK
            bool non_recursive_reentry_terminates()
            {
                const pid_t child = fork();
                if (child == 0) {
                    // The crash handler would run on abort; keep the child's death signal clean.
                    signal(SIGABRT, SIG_DFL);
                    // ERRORCHECK degraded to NORMAL would self-deadlock and hang the suite.
                    signal(SIGALRM, SIG_DFL);
                    alarm(5);
                    // Another parent thread may have held the console mutex at fork time.
                    bq::util::set_log_device_console_min_level(bq::log_level::log_level_max);

                    bq::platform::mutex non_recursive(false);
                    non_recursive.lock();
                    non_recursive.lock();
                    _exit(0);
                }
                if (child < 0) {
                    return false;
                }
                int32_t status = 0;
                waitpid(child, &status, 0);
                return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
            }
#endif

        public:
            virtual test_result test() override
            {
                test_result result;
                result.add_result(recursive_mutex_reenters(), "recursive mutex reenters");
#if !BQ_HAS_FORK
                result.add_result(true, "non-recursive reentry check needs fork()");
#else
                result.add_result(non_recursive_reentry_terminates(), "non-recursive reentry terminates the process");
#endif
                return result;
            }
        };
    }
}
