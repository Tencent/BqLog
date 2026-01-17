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
#include "bq_common/platform/java_misc.h"

#if defined(BQ_JAVA)
#include <sys/types.h>
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        static JavaVM* java_vm = NULL;
        static BQ_TLS JNIEnv* tls_java_env = NULL;
        static BQ_TLS bool tls_did_attach = false;

        static jclass cls_byte_buffer_ = nullptr;
        static jmethodID method_byte_buffer_byte_order_ = nullptr;
        static jobject jobj_little_endian_value_ = nullptr;
        static jobject jobj_big_endian_value_ = nullptr;

        static bq::array<void (*)()>& get_jni_onload_callbacks()
        {
            return common_global_vars::get().jni_onload_callbacks_inst_;
        }

        jni_env::jni_env()
        {
            env = nullptr;
            if (common_global_vars::get().is_jvm_destroyed()) {
                return;
            }
            assert(java_vm != NULL && "invoke functions on java_vm before it is initialized");
            jint get_env_result = java_vm->GetEnv((void**)&tls_java_env, JNI_VERSION_1_6);
            if (JNI_EDETACHED == get_env_result) {
                using attach_param_type = function_argument_type_t<decltype(&JavaVM::AttachCurrentThreadAsDaemon), 0>;
                jint attach_result = java_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<attach_param_type>(&tls_java_env), nullptr);
                if (JNI_OK != attach_result) {
                    bq::util::log_device_console(log_level::fatal, "jni_env attach error, AttachCurrentThreadAsDaemon error code:%" PRId32, static_cast<int32_t>(attach_result));
                    tls_java_env = nullptr;
                    return;
                }
                tls_did_attach = true;
            }else if (JNI_OK != get_env_result) {
                bq::util::log_device_console(log_level::fatal, "JVM GetEnv error, error code:%" PRIu32, static_cast<int32_t>(get_env_result));
                tls_java_env = nullptr;
                return;
            }
            env = tls_java_env;
        }

        struct tls_java_env_lifecycle_holder
        {
            ~tls_java_env_lifecycle_holder() {
                if (tls_java_env && tls_did_attach) {
                    if (!common_global_vars::get().is_jvm_destroyed()) {
                        jint detach_result = java_vm->DetachCurrentThread();
                        if (JNI_OK != detach_result) {
                            bq::util::log_device_console(log_level::fatal, "jni_env attach error, DetachCurrentThread error code:%" PRId32, static_cast<int32_t>(detach_result));
                        }
                    }
                }
                tls_java_env = nullptr;
            }
        };

        jni_onload_register::jni_onload_register(void (*onload_callback)())
        {
            assert(!java_vm && "jni_onload_register should be called before JNI_Onload");
            get_jni_onload_callbacks().push_back(onload_callback);
        }

        JavaVM* get_jvm()
        {
            return java_vm;
        }

        jobject create_new_direct_byte_buffer(JNIEnv* env, const void* address, size_t capacity, bool is_big_endian)
        {
            jobject result = env->NewDirectByteBuffer(const_cast<void*>(address), static_cast<jlong>(capacity));
            if (!is_big_endian) {
                jobject new_order = env->CallObjectMethod(result, method_byte_buffer_byte_order_, jobj_little_endian_value_);
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    result = NULL;
                }
                if (new_order) {
                    env->DeleteLocalRef(new_order);
                }
            }
            return result;
        }

        static void init_reflection_variables()
        {
            jni_env env_holder;
            auto env = env_holder.env;
            jclass cls_byte_buffer_local = env->FindClass("java/nio/ByteBuffer");
            cls_byte_buffer_ = (jclass)env->NewGlobalRef(cls_byte_buffer_local);
            env->DeleteLocalRef(cls_byte_buffer_local);

            method_byte_buffer_byte_order_ = env->GetMethodID(cls_byte_buffer_, "order", "(Ljava/nio/ByteOrder;)Ljava/nio/ByteBuffer;");
            
            jclass cls_byte_order = env->FindClass("java/nio/ByteOrder");
            jfieldID little_endian_field = env->GetStaticFieldID(cls_byte_order, "LITTLE_ENDIAN", "Ljava/nio/ByteOrder;");
            jfieldID big_endian_field = env->GetStaticFieldID(cls_byte_order, "BIG_ENDIAN", "Ljava/nio/ByteOrder;");
            
            jobject jobj_little_endian_local = env->GetStaticObjectField(cls_byte_order, little_endian_field);
            jobj_little_endian_value_ = env->NewGlobalRef(jobj_little_endian_local);
            env->DeleteLocalRef(jobj_little_endian_local);

            jobject jobj_big_endian_local = env->GetStaticObjectField(cls_byte_order, big_endian_field);
            jobj_big_endian_value_ = env->NewGlobalRef(jobj_big_endian_local);
            env->DeleteLocalRef(jobj_big_endian_local);

            env->DeleteLocalRef(cls_byte_order);
        }

        jint jni_init(JavaVM* vm, void* reserved)
        {
            (void)reserved;
            java_vm = vm;
            bq::util::log_device_console(bq::log_level::info, "Bq JNI_Onload is called");
            init_reflection_variables();
            for (auto callback : get_jni_onload_callbacks()) {
                callback();
            }
            return JNI_VERSION_1_6;
        }

#ifdef BQ_DYNAMIC_LIB
#ifdef __cplusplus
        extern "C" {
#endif
        JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
        {
            return jni_init(vm, reserved);
        }
#ifdef __cplusplus
        }
#endif
#endif
    }

    BQ_TLS_NON_POD(platform::tls_java_env_lifecycle_holder, java_env_lifecycle_holder_)
}
#endif
