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
#include "bq_common_test/test_string.h"
#include "bq_common_test/test_file_manager.h"
#include "bq_common_test/test_property.h"
#include "bq_common_test/test_thread_atomic.h"
#include "bq_common_test/test_encryption.h"
#include "test_miso_ring_buffer.h"
#include "test_siso_ring_buffer.h"
#include "test_log_buffer.h"
#include "test_log_appender.h"
#include "test_log.h"
#include "test_layout.h"
#include <locale.h>
#if defined(BQ_WIN)
#include <windows.h>
#elif defined(BQ_POSIX)
#include <signal.h>
#endif

#ifdef BQ_POSIX
pthread_t main_thread_id = 0;
#endif

class time_out_monitor_thread : public bq::platform::thread {
protected:
    void run() override
    {
        auto start_epoch = bq::platform::high_performance_epoch_ms();
        constexpr uint64_t time_out = 720 * 60 * 1000;
        while (true) {
            if (is_cancelled()) {
                break;
            }
            if (start_epoch + time_out < bq::platform::high_performance_epoch_ms()) {
                test_output_dynamic(bq::log_level::error, "test time out, please check your test code!\n");
#ifdef BQ_POSIX
                pthread_kill(main_thread_id, SIGUSR2);
#else
                assert(false && "auto test time out!");
#endif
            }
            bq::platform::thread::sleep(500);
        }
    }
};

#ifdef BQ_POSIX
void sig_handler(int)
{
    bq::_api_string_def stack_trace_str;
    bq::api::__api_get_stack_trace(&stack_trace_str, 0);
    printf("timeout:stack_trace:%s", stack_trace_str.str);
    fflush(stdout);
    bq::platform::thread::sleep(1000);
    assert(false && "auto test time out!");
}
#endif

int32_t main_logic()
{
#ifdef BQ_POSIX
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
    TEST_GROUP(Bq_Common_Test, bq::test, test_file_manager);
    TEST_GROUP(Bq_Common_Test, bq::test, test_thread_atomic);
    TEST_GROUP(Bq_Common_Test, bq::test, test_encryption);
    TEST_GROUP_END(Bq_Common_Test);

    TEST_GROUP_BEGIN(Bq_Log_Test);
    bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("bqlog_mmap", 0));
    TEST_GROUP(Bq_Log_Test, bq::test, test_log_buffer);
    TEST_GROUP(Bq_Log_Test, bq::test, test_log_appender);
    TEST_GROUP(Bq_Log_Test, bq::test, test_siso_ring_buffer);
    TEST_GROUP(Bq_Log_Test, bq::test, test_miso_ring_buffer);
    TEST_GROUP(Bq_Log_Test, bq::test, test_log);
    TEST_GROUP(Bq_Log_Test, bq::test, test_layout);
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

int32_t main(int32_t argc, char** argv)
{
    (void)argc;
    (void)argv;
    return main_logic();
}

#if defined(BQ_ANDROID)
int32_t test_main()
{
    return main_logic();
}
#endif
