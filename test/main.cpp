/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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
#include "bq_common/bq_common_public_include.h"
#include "bq_common_test/test_basic_types.h"
#include "bq_common_test/test_utils.h"
#include "bq_common_test/test_array.h"
#include "bq_common_test/test_hash_map.h"
#include "bq_common_test/test_string.h"
#include "bq_common_test/test_file_manager.h"
#include "bq_common_test/test_property.h"
#include "bq_common_test/test_thread_atomic.h"
#include "test_ring_buffer.h"
#include "test_log.h"
#include <locale.h>
#if defined(WIN32)
#include <windows.h>
#endif

#if defined(BQ_ANDROID)
int32_t test_main()
#else
int32_t main()
#endif
{
#if defined(WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setlocale(LC_ALL, "zh_CN.utf8");
    TEST_GROUP_BEGIN(Bq_Common_Test);
    TEST_GROUP(Bq_Common_Test, bq::test, test_property);
    TEST_GROUP(Bq_Common_Test, bq::test, test_basic_type);
    TEST_GROUP(Bq_Common_Test, bq::test, test_array);
    TEST_GROUP(Bq_Common_Test, bq::test, test_string);
    TEST_GROUP(Bq_Common_Test, bq::test, test_hash_map);
    TEST_GROUP(Bq_Common_Test, bq::test, test_utils);
    TEST_GROUP(Bq_Common_Test, bq::test, test_file_manager);
    TEST_GROUP(Bq_Common_Test, bq::test, test_thread_atomic);
    TEST_GROUP_END(Bq_Common_Test);

    TEST_GROUP_BEGIN(Bq_Log_Test);
    TEST_GROUP(Bq_Log_Test, bq::test, test_log);
    TEST_GROUP(Bq_Log_Test, bq::test, test_ring_buffer);
    TEST_GROUP_END(Bq_Log_Test);

    if (TEST_GROUP_RESULT(Bq_Common_Test)
        && TEST_GROUP_RESULT(Bq_Log_Test)) {
        return 0;
    } else {
        return -1;
    }
}
