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
#include "native_src_bq_common_platform_java_misc.h"

#if defined(BQ_JAVA)
#include <sys/types.h>
#include "native_src_bq_common_bq_common.h"

namespace bq {
    namespace platform {
        static JavaVM* java_vm = NULL;
        // 0: default(Java thread or not attached yet), 1: attached, 2: detached (Thread is exiting), 3: attached again(attached again when Thread is exiting)
        static BQ_TLS int32_t tls_attach_phase = 0;

        static jclass cls_byte_buffer_ = nullptr;
        static jmethodID method_byte_buffer_byte_order_ = nullptr;
        static jobject jobj_little_endian_value_ = nullptr;

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

        // find_byte_to_modify() returns the index of the first byte utf8_to_modified_utf8() has to change, or len
        // if there is none: a '\0' and, except on Android, anything from 0xF0 up, the lead byte of a 4-byte
        // sequence. It runs over every string on its way to Java, so it reads 16 bytes at a time where it can.
#if defined(BQ_ANDROID)
        static size_t find_byte_to_modify(const uint8_t* src, size_t len)
        {
            const void* nul = len > 0 ? memchr(src, 0, len) : nullptr;
            return nul ? static_cast<size_t>(static_cast<const uint8_t*>(nul) - src) : len;
        }
#else
        static size_t find_byte_to_modify_sw(const uint8_t* src, size_t i, size_t len)
        {
            while (i < len && src[i] != 0 && src[i] < 0xF0U) {
                ++i;
            }
            return i;
        }

#if defined(BQ_X86)
        // Adding 0x10 wraps 0xF0..0xFF round to 0x00..0x0F and takes 0x00 to 0x10, while every other byte ends up
        // above 0x10, so one unsigned compare against 0x10 finds both. SSE2 only, the x86 baseline, so no CPU check.
        static BQ_HW_SIMD_SSE2_TARGET size_t find_byte_to_modify(const uint8_t* src, size_t len)
        {
            const __m128i bias = _mm_set1_epi8(0x10);
            size_t i = 0;
            for (; i + 64 <= len; i += 64) {
                const __m128i a = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i)), bias);
                const __m128i b = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i + 16)), bias);
                const __m128i c = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i + 32)), bias);
                const __m128i d = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i + 48)), bias);
                const __m128i m = _mm_min_epu8(_mm_min_epu8(a, b), _mm_min_epu8(c, d));
                if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_min_epu8(m, bias), m)) != 0) {
                    break;
                }
            }
            for (; i + 16 <= len; i += 16) {
                const __m128i a = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i)), bias);
                const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_min_epu8(a, bias), a)));
                if (mask != 0) {
                    return i + bq_ctz64(mask);
                }
            }
            if (i < len && len >= 16) {
                // The last 16 bytes, overlapping what is already known to be clean, so a hit is at i or later.
                const __m128i a = _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + len - 16)), bias);
                const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_min_epu8(a, bias), a)));
                return mask != 0 ? len - 16 + bq_ctz64(mask) : len;
            }
            return find_byte_to_modify_sw(src, i, len);
        }
#elif defined(BQ_ARM_NEON)
        // Same idea as the SSE2 version above.
        static size_t find_byte_to_modify(const uint8_t* src, size_t len)
        {
            const uint8x16_t bias = vdupq_n_u8(0x10);
            size_t i = 0;
            for (; i + 64 <= len; i += 64) {
                const uint8x16_t a = vaddq_u8(vld1q_u8(src + i), bias);
                const uint8x16_t b = vaddq_u8(vld1q_u8(src + i + 16), bias);
                const uint8x16_t c = vaddq_u8(vld1q_u8(src + i + 32), bias);
                const uint8x16_t d = vaddq_u8(vld1q_u8(src + i + 48), bias);
                if (bq_vmaxvq_u8(vcleq_u8(vminq_u8(vminq_u8(a, b), vminq_u8(c, d)), bias)) != 0) {
                    break;
                }
            }
            for (; i + 16 <= len; i += 16) {
                const uint64_t mask = bq_neon_byte_mask(vcleq_u8(vaddq_u8(vld1q_u8(src + i), bias), bias));
                if (mask != 0) {
                    return i + (bq_ctz64(mask) >> 2);
                }
            }
            if (i < len && len >= 16) {
                // The last 16 bytes, overlapping what is already known to be clean, so a hit is at i or later.
                const uint64_t mask = bq_neon_byte_mask(vcleq_u8(vaddq_u8(vld1q_u8(src + len - 16), bias), bias));
                return mask != 0 ? len - 16 + (bq_ctz64(mask) >> 2) : len;
            }
            return find_byte_to_modify_sw(src, i, len);
        }
#else
        static size_t find_byte_to_modify(const uint8_t* src, size_t len)
        {
            size_t i = 0;
            for (; i + 8 <= len; i += 8) {
                uint64_t v;
                memcpy(&v, src + i, sizeof(v));
                // Leaves the top bit of a byte set if the byte is 0x00, or if its top 4 bits are all set.
                const uint64_t hit = ((v - 0x0101010101010101ULL) & ~v) | (v & (v << 1) & (v << 2) & (v << 3));
                if ((hit & 0x8080808080808080ULL) != 0) {
                    break;
                }
            }
            return find_byte_to_modify_sw(src, i, len);
        }
#endif
#endif

#if !defined(BQ_ANDROID)
        // The code point of the well-formed 4-byte UTF-8 sequence at p, or 0 if there is none.
        static uint32_t read_4_byte_sequence(const uint8_t* p, size_t remaining)
        {
            if (remaining < 4 || (p[0] & 0xF8U) != 0xF0U || (p[1] & 0xC0U) != 0x80U || (p[2] & 0xC0U) != 0x80U || (p[3] & 0xC0U) != 0x80U) {
                return 0;
            }
            const uint32_t code_point = ((p[0] & 0x07U) << 18) | ((p[1] & 0x3FU) << 12) | ((p[2] & 0x3FU) << 6) | (p[3] & 0x3FU);
            return (code_point >= 0x10000U && code_point <= 0x10FFFFU) ? code_point : 0;
        }
#endif

        const char* utf8_to_modified_utf8(const char* utf8, size_t len, bq::array<char>& buffer)
        {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(utf8);
            size_t i = find_byte_to_modify(src, len);
            if (i == len) {
                return utf8;
            }
            // A '\0' grows to 2 bytes and a 4-byte sequence to 6, so twice the length is enough, plus the '\0'.
            buffer.clear();
            buffer.fill_uninitialized(len * 2 + 1);
            uint8_t* out = reinterpret_cast<uint8_t*>(&buffer[0]);
            size_t copied = 0;
            while (true) {
                // Everything between copied and i goes over as it is.
                memcpy(out, src + copied, i - copied);
                out += i - copied;
                if (i == len) {
                    break;
                }
                const uint8_t c = src[i];
                if (c == 0) {
                    *out++ = 0xC0;
                    *out++ = 0x80;
                    i += 1;
#if !defined(BQ_ANDROID)
                } else if (const uint32_t code_point = read_4_byte_sequence(src + i, len - i)) {
                    const uint32_t units[2] = { 0xD800U + ((code_point - 0x10000U) >> 10), 0xDC00U + ((code_point - 0x10000U) & 0x3FFU) };
                    for (uint32_t unit : units) {
                        *out++ = static_cast<uint8_t>(0xE0U | (unit >> 12));
                        *out++ = static_cast<uint8_t>(0x80U | ((unit >> 6) & 0x3FU));
                        *out++ = static_cast<uint8_t>(0x80U | (unit & 0x3FU));
                    }
                    i += 4;
#endif
                } else {
                    // A malformed byte from 0xF0 up is copied as it is, as when nothing needs changing.
                    *out++ = c;
                    i += 1;
                }
                copied = i;
                i += find_byte_to_modify(src + i, len - i);
            }
            *out = 0;
            return &buffer[0];
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

            jobject jobj_little_endian_local = env->GetStaticObjectField(cls_byte_order, little_endian_field);
            jobj_little_endian_value_ = env->NewGlobalRef(jobj_little_endian_local);
            env->DeleteLocalRef(jobj_little_endian_local);

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
