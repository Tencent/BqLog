#pragma once
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

#include "bq_common/bq_common_public_include.h"
#if defined(BQ_JAVA)
#include <jni.h>

namespace bq {
    namespace platform {
        // Whenever you need to use a JNIEnv* object, declare this type of object on the stack,
        // and you will get a JNIEnv* that is already attached to the Java Virtual Machine.
        // Moreover, you don't need to worry about when to detach,
        // because it will handle this for you when thread ends.
        struct jni_env {
        public:
            JNIEnv* env = NULL;

            jni_env();
            ~jni_env();
        };

        struct jni_onload_register {
            jni_onload_register(void (*onload_callback)());
        };

        JavaVM* get_jvm();

        jobject create_new_direct_byte_buffer(JNIEnv* env, const void* address, size_t capacity, bool is_big_endian);

        jint jni_init(JavaVM* vm, void* reserved);
    }
}
#endif
