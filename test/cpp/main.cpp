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
#include <iostream>
#include "bq_common_test/test_basic_types.h"
#include "bq_common_test/test_utils.h"
#include "bq_common_test/test_array.h"
#include "bq_common_test/test_hash_map.h"
#include "test_bounded_hash_cache.h"
#include "test_compressed_cache.h"
#include "test_string_serialize.h"
#include "bq_common_test/test_string.h"
#include "bq_common_test/test_file_manager.h"
#include "bq_common_test/test_base_dir_strict.h"
#include "bq_common_test/test_mutex_recursive.h"
#include "bq_common_test/test_platform_capability.h"
#include "bq_common_test/test_property.h"
#include "bq_common_test/test_thread_atomic.h"
#include "bq_common_test/test_encryption.h"
#include "bq_common_test/test_fast_clock.h"
#include "test_miso_ring_buffer.h"
#include "test_siso_ring_buffer.h"
#include "test_instance_storage.h"
#include "test_meta_registry.h"
#include "test_log_buffer.h"
#include "test_fast_log.h"
#if defined(BQ_CPP_20)
#include "test_fast_info.h"
#endif
#include "test_log_appender.h"
#include "test_log.h"
#include "test_layout.h"
#include "test_disk_full.h"
#include <locale.h>
#include <stdlib.h>
#if defined(BQ_WIN)
#include <windows.h>
#elif defined(BQ_POSIX)
#include <signal.h>
#include <unistd.h>
#endif

#if BQ_HAS_CRASH_HANDLER
pthread_t main_thread_id = 0;
#endif

// CI kills the job at its limit, so this fires first: the slowest runner (arm64 gcc asan) needs
// about 230 minutes for the Debug binary. A tighter budget passes --timeout-minutes.
static uint64_t test_time_out_minutes = 300;

static void parse_time_out_argument(int32_t argc, char** argv)
{
    constexpr const char* prefix = "--timeout-minutes=";
    constexpr size_t prefix_len = sizeof("--timeout-minutes=") - 1;
    for (int32_t i = 1; i < argc; ++i) {
        if (strncmp(argv[i], prefix, prefix_len) != 0) {
            continue;
        }
        const int32_t minutes = atoi(argv[i] + prefix_len);
        if (minutes > 0) {
            test_time_out_minutes = static_cast<uint64_t>(minutes);
        }
    }
}

class time_out_monitor_thread : public bq::platform::thread {
protected:
    void run() override
    {
        const uint64_t time_out_minutes = test_time_out_minutes;
        const auto start_epoch = bq::platform::high_performance_epoch_ms();
        while (!is_cancelled()) {
            if (start_epoch + time_out_minutes * 60 * 1000 < bq::platform::high_performance_epoch_ms()) {
                test_output_dynamic(bq::log_level::error, "test time out, please check your test code!\n");
                printf("test time out after %u minutes in %s\n",
                    static_cast<uint32_t>(time_out_minutes), bq::test::current_test_case());
                fflush(stdout);
#if BQ_HAS_CRASH_HANDLER
                pthread_kill(main_thread_id, SIGUSR2); // the handler prints the stack trace of the running thread
#endif
                bq::platform::thread::sleep(1000);
                // A core dump names every thread; a stuck join would otherwise run to the job limit
                abort();
            }
            bq::platform::thread::sleep(500);
        }
    }
};

#if BQ_HAS_CRASH_HANDLER
void sig_handler(int)
{
    bq::_api_string_def stack_trace_str;
    bq::api::__api_get_stack_trace(&stack_trace_str, 0);
    printf("timeout:stack_trace:%s", stack_trace_str.str);
    fflush(stdout);
    // the monitor thread dumps the core a second later, so the handler only has to report
}
#endif

int32_t main_logic()
{
#if BQ_HAS_CRASH_HANDLER
    struct sigaction sa = {};
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR2, &sa, NULL);
    main_thread_id = pthread_self();
    bq::log::enable_auto_crash_handle();
#endif
#if defined(WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, "zh_CN.utf8");
    time_out_monitor_thread monitor_thread;
    monitor_thread.set_thread_name("TestMonitor");
    monitor_thread.start();

    TEST_GROUP_BEGIN(Bq_Common_Test);
    TEST_GROUP(Bq_Common_Test, bq::test, test_property);
    TEST_GROUP(Bq_Common_Test, bq::test, test_basic_type);
    TEST_GROUP(Bq_Common_Test, bq::test, test_array);
    TEST_GROUP(Bq_Common_Test, bq::test, test_string);
    TEST_GROUP(Bq_Common_Test, bq::test, test_hash_map);
    TEST_GROUP(Bq_Common_Test, bq::test, test_utils);
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Common_Test, bq::test, test_file_manager);
    TEST_GROUP(Bq_Common_Test, bq::test, test_base_dir_strict);
    TEST_GROUP(Bq_Common_Test, bq::test, test_thread_atomic);
    TEST_GROUP(Bq_Common_Test, bq::test, test_mutex_recursive);
    TEST_GROUP(Bq_Common_Test, bq::test, test_platform_capability);
    TEST_GROUP(Bq_Common_Test, bq::test, test_encryption);
    TEST_GROUP(Bq_Common_Test, bq::test, test_fast_clock);
    TEST_GROUP_END(Bq_Common_Test);

    TEST_GROUP_BEGIN(Bq_Log_Test);
    TEST_GROUP(Bq_Log_Test, bq::test, test_bounded_hash_cache);
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_compressed_cache);
    TEST_GROUP(Bq_Log_Test, bq::test, test_string_serialize);
    bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("bqlog_mmap", 0));
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_instance_storage);
    TEST_GROUP(Bq_Log_Test, bq::test, test_meta_registry);
    TEST_GROUP_IF(BQ_HAS_MMAP, Bq_Log_Test, bq::test, test_log_buffer);
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_fast_log);
#if defined(BQ_CPP_20)
    TEST_GROUP(Bq_Log_Test, bq::test, test_fast_info);
#endif
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_log_appender);
    TEST_GROUP(Bq_Log_Test, bq::test, test_siso_ring_buffer);
    TEST_GROUP(Bq_Log_Test, bq::test, test_miso_ring_buffer);
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_log);
    TEST_GROUP(Bq_Log_Test, bq::test, test_layout);
    TEST_GROUP_IF(BQ_HAS_DIRENT, Bq_Log_Test, bq::test, test_disk_full);
    TEST_GROUP_END(Bq_Log_Test);

    bool test_result = TEST_GROUP_RESULT(Bq_Common_Test) && TEST_GROUP_RESULT(Bq_Log_Test);

    monitor_thread.cancel();
    monitor_thread.join();
    if (test_result) {
        bq::util::set_log_device_console_min_level(bq::log_level::verbose);
        bq::test::add_to_test_output_str(false, bq::log_level::info, "\n--------------------------------\nCONGRATULATION!!! ALL TEST CASES IS PASSED!\n--------------------------------\n");
        bq::util::log_device_console(bq::log_level::info, "\n--------------------------------\nCONGRATULATION!!! ALL TEST CASES IS PASSED!\n--------------------------------\n");
        bq::util::set_log_device_console_min_level(bq::log_level::warning);
        return 0;
    } else {
        bq::test::add_to_test_output_str(false, bq::log_level::fatal, "\n--------------------------------\nSORRY!!! TEST CASES FAILED!\n--------------------------------\n");
        bq::util::log_device_console(bq::log_level::fatal, "\n--------------------------------\nSORRY!!! TEST CASES FAILED!\n--------------------------------\n");
        return -1;
    }
}

// the suite registers more formats than the mobile cap; the refusal path is covered by test_set_max_entries tests
static void prepare_test_meta_registry()
{
    bq::meta_registry::instance().test_set_max_entries(1u << 20);
}

int32_t main(int32_t argc, char** argv)
{
    // a pipe (CI) would buffer the output in blocks, which hides the running case for minutes
    setvbuf(stdout, nullptr, _IOLBF, 0);
#if !BQ_HAS_DEFAULT_BASE_DIR
    {
        char cwd_buf[4096];
        if (getcwd(cwd_buf, sizeof(cwd_buf)) != nullptr) {
            bq::log::reset_base_dir(0, cwd_buf);
            bq::log::reset_base_dir(1, cwd_buf);
        }
    }
#endif
    parse_time_out_argument(argc, argv);
    prepare_test_meta_registry();
    if (argc > 1 && strcmp(argv[1], "--fast-clock-only") == 0) {
        bq::test::test_fast_clock clock_test;
        auto result = clock_test.test();
        result.output("test_fast_clock");
        return result.is_all_pass() ? 0 : -1;
    }
    if (argc > 1 && strcmp(argv[1], "--fast-only") == 0) {
        bq::test::test_fast_log fast_test;
        auto result = fast_test.test();
        result.output("test_fast_log");
        return result.is_all_pass() ? 0 : -1;
    }
    if (argc > 1 && strcmp(argv[1], "--appender-only") == 0) {
        bq::test::test_log_appender appender_test;
        auto result = appender_test.test();
        result.output("test_log_appender");
        return result.is_all_pass() ? 0 : -1;
    }
    if (argc > 1 && strcmp(argv[1], "--log-only") == 0) {
        bq::test::test_log log_test;
        auto result = log_test.test();
        result.output("test_log");
        return result.is_all_pass() ? 0 : -1;
    }
    if (argc > 1 && strcmp(argv[1], "--instance-only") == 0) {
        bq::test::test_instance_storage instance_test;
        auto result = instance_test.test();
        result.output("test_instance_storage");
        return result.is_all_pass() ? 0 : -1;
    }
    if (argc > 1 && strcmp(argv[1], "--meta-only") == 0) {
        bq::test::test_meta_registry meta_test;
        auto result = meta_test.test();
        result.output("test_meta_registry");
        return result.is_all_pass() ? 0 : -1;
    }
#if defined(BQ_CPP_20)
    if (argc > 1 && strcmp(argv[1], "--fast-info-only") == 0) {
        bq::test::test_fast_info fast_info_test;
        auto result = fast_info_test.test();
        result.output("test_fast_info");
        return result.is_all_pass() ? 0 : -1;
    }
#endif
    return main_logic();
}

#if defined(BQ_ANDROID)
int32_t test_main()
{
    prepare_test_meta_registry();
    return main_logic();
}
#endif
