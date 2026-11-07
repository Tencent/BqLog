/* Copyright (C) 2025 Tencent.
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

        // Creates a Java string from len bytes of UTF-8. Use this rather than NewStringUTF for any text
        // that has a length: NewStringUTF stops at the first '\0', and it reads modified UTF-8, where the
        // 4-byte sequences of characters outside the BMP (emoji, for one) are not valid.
        jstring create_jstring_from_utf8(JNIEnv* env, const char* utf8, size_t len);

        jint jni_init(JavaVM* vm, void* reserved);

        void try_to_detach_thread();

        // Sometimes, you need remove global-ref in TLS variable destructor when thread is exiting.
        //  This function can help you do that in an async way to avoid JVM errors.
        void remove_global_ref_async(jobject recycle_obj);
    }
}
#endif
