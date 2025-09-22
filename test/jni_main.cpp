/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
/*!
 * \class jni_main.cpp
 *
 * \brief entry file of unit test in JNI environment
 *
 * \author pippocao
 * \date 2022/09/23
 */
#include "bq_common/bq_common.h"

#if defined(BQ_ANDROID)
#include <jni.h>
#include "com_tencent_bq_unittest_JNIEntry.h"
#include "test_base.h"

extern int32_t test_main();

extern "C" {
JNIEXPORT void JNICALL Java_com_tencent_bq_unittest_JNIEntry_begin_1test(JNIEnv*, jclass)
{
    test_main();
}
JNIEXPORT jstring JNICALL Java_com_tencent_bq_unittest_JNIEntry_get_1current_1test_1result(JNIEnv* env, jclass)
{
    std::string display_str = bq::test::get_test_output();
    return env->NewStringUTF(display_str.c_str());
}
}
#endif