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
#include "bq_common/bq_common.h"
#include <stdio.h>
#if defined(BQ_POSIX)
#include <sys/wait.h>
#include <unistd.h>
#endif

class instance_lock_racer : public bq::platform::thread {
    const char* path_;
    bq::platform::atomic<int32_t>& active_;
    bq::platform::atomic<bool>& valid_;

    void run() override
    {
        using namespace bq::platform;
        for (uint32_t i = 0; i < 500; ++i) {
            platform_file_handle handle = invalid_platform_file_handle;
            const auto result = try_lock_instance_file(path_, handle);
            if (result == instance_lock_result::locked) {
                if (active_.fetch_add(1, memory_order::acq_rel) != 0) {
                    valid_.store_release(false);
                }
                thread::yield();
                active_.fetch_sub(1, memory_order::acq_rel);
                unlock_instance_file(handle);
            } else if (result != instance_lock_result::held_by_other) {
                valid_.store_release(false);
            }
        }
    }

public:
    instance_lock_racer(const char* path, bq::platform::atomic<int32_t>& active, bq::platform::atomic<bool>& valid)
        : path_(path)
        , active_(active)
        , valid_(valid)
    {
    }
};

int main()
{
    using namespace bq::platform;
    if (!is_cross_process_file_lock_supported()) {
        printf("SKIP instance lock probe: this platform has no cross-process file lock\n");
        return 77;
    }
    const char* path = "instance_lock_probe.lock";
    platform_file_handle first = invalid_platform_file_handle;
    platform_file_handle second = invalid_platform_file_handle;
    const auto first_result = try_lock_instance_file(path, first);
    const auto second_result = try_lock_instance_file(path, second);
    const bool held = first_result == instance_lock_result::locked
        && second_result == instance_lock_result::held_by_other
        && !is_platform_handle_valid(second);
    unlock_instance_file(second);
    const auto third_result = try_lock_instance_file(path, second);
    const bool still_held = third_result == instance_lock_result::held_by_other;
    unlock_instance_file(second);
    bool child_blocked = true;
#if defined(BQ_POSIX)
    const auto child = fork();
    if (child == 0) {
        unlock_instance_file(first);
        const auto child_result = try_lock_instance_file(path, second);
        unlock_instance_file(second);
        _exit(child_result == instance_lock_result::held_by_other ? 0 : 1);
    }
    int status = 0;
    child_blocked = child > 0 && waitpid(child, &status, 0) == child
        && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
    unlock_instance_file(first);
    const auto final_result = try_lock_instance_file(path, first);
    const bool released = final_result == instance_lock_result::locked;
    unlock_instance_file(first);
    bool crash_released = true;
#if defined(BQ_POSIX)
    const auto crash_child = fork();
    if (crash_child == 0) {
        const auto crash_result = try_lock_instance_file(path, first);
        _exit(crash_result == instance_lock_result::locked ? 0 : 1);
    }
    int crash_status = 0;
    crash_released = crash_child > 0 && waitpid(crash_child, &crash_status, 0) == crash_child
        && WIFEXITED(crash_status) && WEXITSTATUS(crash_status) == 0;
    crash_released = crash_released && try_lock_instance_file(path, first) == instance_lock_result::locked;
    unlock_instance_file(first);
#endif
    bq::platform::atomic<int32_t> active(0);
    bq::platform::atomic<bool> races_valid(true);
    instance_lock_racer racer_a(path, active, races_valid);
    instance_lock_racer racer_b(path, active, races_valid);
    instance_lock_racer racer_c(path, active, races_valid);
    instance_lock_racer racer_d(path, active, races_valid);
    instance_lock_racer* racers[] = { &racer_a, &racer_b, &racer_c, &racer_d };
    for (auto* racer : racers) {
        racer->start();
    }
    for (auto* racer : racers) {
        racer->join();
    }
    printf("instance locks: first=%d second=%d third=%d final=%d\n",
        static_cast<int>(first_result), static_cast<int>(second_result),
        static_cast<int>(third_result), static_cast<int>(final_result));
    bq::file_manager::remove_file_or_dir(path);
    return held && still_held && child_blocked && released && crash_released && races_valid.load_acquire() ? 0 : 1;
}
