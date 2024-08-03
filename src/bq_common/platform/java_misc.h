#pragma once
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
#if BQ_JAVA
#include <jni.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        // Whenever you need to use a JNIEnv* object, declare this type of object on the stack,
        // and you will get a JNIEnv* that is already attached to the Java Virtual Machine.
        // Moreover, you don't need to worry about when to detach,
        // because it will handle this for you when its lifecycle ends.
        // It is smart enough to deal with nested declarations
        // and also knows whether it was called from the Java side (in which case no detachment is necessary).
        struct jni_env {
        private:
            bool attached_in_init = false;

        public:
            JNIEnv* env = NULL;

            jni_env();
            ~jni_env();
        };

        struct jni_onload_register {
            jni_onload_register(void (*onload_callback)());
        };

        JavaVM* get_jvm();
        const bq::string& get_android_id();
        const bq::string& get_package_name();
    }
}
#endif
