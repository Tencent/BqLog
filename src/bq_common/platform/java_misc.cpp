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
        // 0: default(Java thread or not attached yet), 1: attached, 2: detached (Thread is exiting), 3: attached again(attached again when Thread is exiting)
        static BQ_TLS int32_t tls_attach_phase = 0;

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
            assert(java_vm != nullptr && "invoke functions on java_vm before it is initialized");
            jint get_env_result = java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (JNI_EDETACHED == get_env_result) {
                using attach_param_type = function_argument_type_t<decltype(&JavaVM::AttachCurrentThreadAsDaemon), 0>;
                jint attach_result = java_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<attach_param_type>(&env), nullptr);
                if (JNI_OK != attach_result) {
                    // JVM is Destroying
                    env = nullptr;
                    return;
                }
                if (tls_attach_phase == 0) {
                    tls_attach_phase = 1;
                } else {
#ifndef NDEBUG
                    assert(tls_attach_phase == 2 && "invalid tls_attach_phase");
#else
                    bq::util::_default_console_output(bq::log_level::error, "invalid tls_attach_phase");
#endif
                    tls_attach_phase = 3;
                }
            } else if (JNI_OK != get_env_result) {
                // JVM is Destroying
                env = nullptr;
                return;
            }
        }

        jni_env::~jni_env()
        {
            if (tls_attach_phase == 3) {
                jint detach_result = java_vm->DetachCurrentThread();
                if (JNI_OK != detach_result) {
                    // JVM is destroying
                }
                tls_attach_phase = 2;
            }
        }

        struct tls_java_env_lifecycle_holder {
            ~tls_java_env_lifecycle_holder()
            {
                try_to_detach_thread();
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

        void try_to_detach_thread()
        {
            if (tls_attach_phase == 1) {
                if (!common_global_vars::get().is_jvm_destroyed()) {
                    jint detach_result = java_vm->DetachCurrentThread();
                    if (JNI_OK != detach_result) {
                        // JVM is destroying
                    }
                }
            }
            tls_attach_phase = 2;
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

        void remove_global_ref_async(jobject recycle_obj)
        {
            bq::platform::scoped_mutex lock(common_global_vars::get().remove_jni_global_ref_list_mutex_);
            common_global_vars::get().remove_jni_global_ref_list_array_.push_back(recycle_obj);
            if (common_global_vars::get().remove_jni_global_ref_list_array_.size() > 16) {
                auto thread_status = common_global_vars::get().remove_jni_global_ref_thread_.get_status();
                if (thread_status == bq::platform::enum_thread_status::init
                    || thread_status == bq::platform::enum_thread_status::released) {
                    common_global_vars::get().remove_jni_global_ref_thread_.start();
                }
            }
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
