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
#include <stddef.h>
#include "bq_log/bq_log.h"

#if BQ_JAVA
#include "bq_common/bq_common.h"
#include "bq_log/api/bq_impl_log_invoker.h"
#include "bq_log/log/log_manager.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_version
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1version(JNIEnv* env, jclass)
{
    const char* version_code = bq::log::get_version().c_str();
    return env->NewStringUTF(version_code);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_enable_auto_crash_handler
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1enable_1auto_1crash_1handler(JNIEnv*, jclass)
{
    bq::log::enable_auto_crash_handle();
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_create_log
 * Signature: (Ljava/lang/String;Ljava/lang/String;J[Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1create_1log(JNIEnv* env, jclass, jstring log_name, jstring log_config, jlong categories_count, jobjectArray category_names_array)
{
    const char* log_name_c_str = env->GetStringUTFChars(log_name, NULL);
    const char* log_config_c_str = env->GetStringUTFChars(log_config, NULL);
    bq::array<const char*> category_names;
    for (jlong i = 0; i < categories_count; ++i) {
        jobject category_name_object = env->GetObjectArrayElement(category_names_array, (jsize)i);
        jstring category_name_string = static_cast<jstring>(category_name_object);
        const char* category_name_c_str = env->GetStringUTFChars(category_name_string, NULL);
        category_names.push_back(category_name_c_str);
    }
    uint64_t log_id = bq::api::__api_create_log(log_name_c_str, log_config_c_str, (uint32_t)categories_count, (const char**)category_names.begin());

    for (jlong i = 0; i < categories_count; ++i) {
        jobject category_name_object = env->GetObjectArrayElement(category_names_array, (jsize)i);
        jstring category_name_string = static_cast<jstring>(category_name_object);
        env->ReleaseStringUTFChars(category_name_string, category_names[(decltype(category_names)::size_type)i]);
    }

    env->ReleaseStringUTFChars(log_name, log_name_c_str);
    env->ReleaseStringUTFChars(log_config, log_config_c_str);
    return (jlong)log_id;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_reset_config
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1reset_1config(JNIEnv* env, jclass, jstring log_name, jstring log_config)
{
    const char* log_name_c_str = env->GetStringUTFChars(log_name, NULL);
    const char* log_config_c_str = env->GetStringUTFChars(log_config, NULL);

    bq::api::__api_log_reset_config(log_name_c_str, log_config_c_str);

    env->ReleaseStringUTFChars(log_name, log_name_c_str);
    env->ReleaseStringUTFChars(log_config, log_config_c_str);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_ring_buffer
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1ring_1buffer(JNIEnv* env, jclass, jlong log_id)
{
    bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
    bq::ring_buffer& ring_buffer = log->get_ring_buffer();
    uint8_t* log_ring_buffer_base_addr = const_cast<uint8_t*>(ring_buffer.get_buffer_addr());
    jlong log_ring_buffer_size = (jlong)(ring_buffer.get_block_size() * ring_buffer.get_total_blocks_count());
    jobject ring_buffer_java_obj = env->NewDirectByteBuffer(log_ring_buffer_base_addr, log_ring_buffer_size);
    return ring_buffer_java_obj;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_buffer_alloc
 * Signature: (J[J)Z
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1buffer_1alloc(JNIEnv* env, jclass, jlong log_id, jlong length, jshort level, jlong category_index, jstring format_content, jlong utf16_str_bytes_len)
{
    auto handle = bq::api::__api_log_buffer_alloc(log_id, (uint32_t)length);
    if (handle.result != bq::enum_buffer_result_code::success) {
        return -1;
    }

    bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
    head->category_idx = static_cast<uint32_t>(category_index);
    head->level = (uint8_t)level;
    head->log_format_str_type = static_cast<uint16_t>(bq::log_arg_type_enum::string_utf16_type);

    jboolean is_cpy = false;
    bq::tools::size_seq<false, const char16_t*> seq;
    seq.get_element().value = sizeof(uint32_t) + (size_t)utf16_str_bytes_len;
    assert(sizeof(bq::_log_entry_head_def) + seq.get_total() <= UINT16_MAX && "log format string too long");
    head->log_args_offset = static_cast<uint16_t>(sizeof(bq::_log_entry_head_def) + seq.get_total());
    uint8_t* log_format_content_addr = handle.data_addr + sizeof(bq::_log_entry_head_def);
    const char16_t* format_str = (const char16_t*)env->GetStringCritical(format_content, &is_cpy);
    bq::impl::_do_log_args_fill<false>(log_format_content_addr, seq, format_str);
    env->ReleaseStringCritical(format_content, (const jchar*)format_str);

    bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
    bq::ring_buffer& ring_buffer = log->get_ring_buffer();
    return ((jlong)((uint8_t*)handle.data_addr - ring_buffer.get_buffer_addr()) << 32) | (jlong)head->log_args_offset;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_arg_push_utf16_string
 * Signature: (JLjava/lang/String;J)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1arg_1push_1utf16_1string(JNIEnv* env, jclass, jlong log_id, jlong offset, jstring arg_str, jlong arg_utf16_bytes_len)
{
    bq::tools::size_seq<true, const char16_t*> seq;
    seq.get_element().value = sizeof(uint32_t) + sizeof(uint32_t) + (size_t)arg_utf16_bytes_len;
    bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
    bq::ring_buffer& ring_buffer = log->get_ring_buffer();
    uint8_t* log_format_content_addr = const_cast<uint8_t*>(ring_buffer.get_buffer_addr()) + (ptrdiff_t)offset;
    jboolean is_cpy = false;
    const char16_t* str = (const char16_t*)env->GetStringCritical(arg_str, &is_cpy);
    bq::impl::_do_log_args_fill<true>(log_format_content_addr, seq, str);
    env->ReleaseStringCritical(arg_str, (const jchar*)str);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_buffer_commit
 * Signature: ([J)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1buffer_1commit(JNIEnv*, jclass, jlong log_id, jlong offset)
{
    bq::_api_ring_buffer_chunk_write_handle handle = {};
    handle.result = bq::enum_buffer_result_code::success;
    bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
    bq::ring_buffer& ring_buffer = log->get_ring_buffer();
    handle.data_addr = const_cast<uint8_t*>(ring_buffer.get_buffer_addr()) + (ptrdiff_t)(offset >> 32);
    bq::api::__api_log_buffer_commit(log_id, handle);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_set_appenders_enable
 * Signature: (ILjava/lang/String;Z)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1set_1appenders_1enable(JNIEnv* env, jclass, jlong log_id, jstring appender_name, jboolean enable)
{
    const char* name = env->GetStringUTFChars(appender_name, NULL);
    bq::api::__api_set_appenders_enable((uint64_t)log_id, name, (bool)enable);
    env->ReleaseStringUTFChars(appender_name, name);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_logs_count
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1logs_1count(JNIEnv*, jclass)
{
    return (jlong)bq::api::__api_get_logs_count();
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_id_by_index
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1id_1by_1index(JNIEnv*, jclass, jlong index)
{
    return (jlong)bq::api::__api_get_log_id_by_index((uint32_t)index);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_name_by_id
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1name_1by_1id(JNIEnv* env, jclass, jlong log_id)
{
    bq::_api_string_def name_def;
    if (!bq::api::__api_get_log_name_by_id((uint64_t)log_id, &name_def)) {
        return nullptr;
    }
    return env->NewStringUTF(name_def.str);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_categories_count
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1categories_1count(JNIEnv*, jclass, jlong log_id)
{
    uint32_t count = bq::api::__api_get_log_categories_count((uint64_t)log_id);
    return (jlong)count;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_category_name_by_index
 * Signature: (JJ)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1category_1name_1by_1index(JNIEnv* env, jclass, jlong log_id, jlong index)
{
    bq::_api_string_def name_def;
    if (!bq::api::__api_get_log_category_name_by_index((uint64_t)log_id, (uint32_t)index, &name_def)) {
        return nullptr;
    }
    return env->NewStringUTF(name_def.str);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_merged_log_level_bitmap_by_log_id
 * Signature: (J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1merged_1log_1level_1bitmap_1by_1log_1id(JNIEnv* env, jclass, jlong log_id)
{
    const uint32_t* bitmap_ptr = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id((uint64_t)log_id);
    jobject buffer = env->NewDirectByteBuffer(const_cast<uint32_t*>(bitmap_ptr), sizeof(uint32_t));
    return buffer;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_category_masks_array_by_log_id
 * Signature: (J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1category_1masks_1array_1by_1log_1id(JNIEnv* env, jclass, jlong log_id)
{
    uint32_t count = bq::api::__api_get_log_categories_count((uint64_t)log_id);
    const uint8_t* mask_array_ptr = bq::api::__api_get_log_category_masks_array_by_log_id((uint64_t)log_id);
    jobject buffer = env->NewDirectByteBuffer(const_cast<uint8_t*>(mask_array_ptr), count * sizeof(uint8_t));
    return buffer;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_log_print_stack_level_bitmap_by_log_id
 * Signature: (J)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1log_1print_1stack_1level_1bitmap_1by_1log_1id(JNIEnv* env, jclass, jlong log_id)
{
    const uint32_t* bitmap_ptr = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id((uint64_t)log_id);
    jobject buffer = env->NewDirectByteBuffer(const_cast<uint32_t*>(bitmap_ptr), sizeof(uint32_t));
    return buffer;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_device_console
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1device_1console(JNIEnv* env, jclass, jint level, jstring content)
{
    const char* content_c_str = env->GetStringUTFChars(content, NULL);
    bq::api::__api_log_device_console((bq::log_level)level, content_c_str);
    env->ReleaseStringUTFChars(content, content_c_str);
    ;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_force_flush
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1force_1flush(JNIEnv*, jclass, jlong log_id)
{
    bq::api::__api_force_flush((uint64_t)log_id);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_get_file_base_dir
 * Signature: (Z)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bq_impl_log_1invoker__1_1api_1get_1file_1base_1dir(JNIEnv* env, jclass, jboolean in_sand_box)
{
    const char* path = bq::api::__api_get_file_base_dir(in_sand_box);
    return env->NewStringUTF(path);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_decoder_create
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1decoder_1create(JNIEnv* env, jclass, jstring path)
{
    uint32_t handle = 0;
    const char* path_c_str = env->GetStringUTFChars(path, NULL);
    auto result = bq::api::__api_log_decoder_create(path_c_str, &handle);
    env->ReleaseStringUTFChars(path, path_c_str);
    if (result != bq::appender_decode_result::success) {
        return (jlong)(result) * (-1);
    } else {
        return (jlong)handle;
    }
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_decoder_decode
 * Signature: (JLbq/def/string_holder;)I
 */
JNIEXPORT jint JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1decoder_1decode(JNIEnv* env, jclass, jlong handle, jobject out_decoded_text_holder)
{
    bq::_api_string_def text;
    auto result = bq::api::__api_log_decoder_decode((uint32_t)handle, &text);

    jclass holder_cls = env->GetObjectClass(out_decoded_text_holder);
    jfieldID holder_value_filed_id = env->GetFieldID(holder_cls, "value", "Ljava/lang/String;");
    if (result == bq::appender_decode_result::success) {
        jstring log_text = env->NewStringUTF(text.str);
        env->SetObjectField(out_decoded_text_holder, holder_value_filed_id, log_text);
    } else {
        jstring log_text = env->NewStringUTF("");
        env->SetObjectField(out_decoded_text_holder, holder_value_filed_id, log_text);
    }
    return (jint)result;
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_decoder_destroy
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1decoder_1destroy(JNIEnv*, jclass, jlong handle)
{
    bq::api::__api_log_decoder_destroy((uint32_t)handle);
}

/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_log_decode
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT jboolean JNICALL Java_bq_impl_log_1invoker__1_1api_1log_1decode(JNIEnv* env, jclass, jstring in_path, jstring out_path)
{
    jboolean re = JNI_FALSE;
    const char* in_path_ = env->GetStringUTFChars(in_path, NULL);
    const char* out_path_ = env->GetStringUTFChars(out_path, NULL);
    if (bq::api::__api_log_decode(in_path_, out_path_))
        re = JNI_TRUE;
    env->ReleaseStringUTFChars(in_path, in_path_);
    env->ReleaseStringUTFChars(out_path, out_path_);
    return re;
}

JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1enable_1snapshot(JNIEnv*, jclass, jlong log_id, jlong snapshot_buffer_size)
{
    bq::api::__api_enable_snapshot((uint64_t)log_id, (uint32_t)snapshot_buffer_size);
}

JNIEXPORT jstring JNICALL Java_bq_impl_log_1invoker__1_1api_1take_1snapshot_1string(JNIEnv* env, jclass, jlong log_id, jboolean use_gmt_time)
{
    bq::_api_string_def snapshot_str_def;
    bq::api::__api_take_snapshot_string((uint64_t)log_id, use_gmt_time, &snapshot_str_def);
    jstring snapshot_str = env->NewStringUTF(snapshot_str_def.str);
    bq::api::__api_release_snapshot_string((uint64_t)log_id, &snapshot_str_def);
    return snapshot_str;
}

static void BQ_STDCALL java_console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
{
    (void)length;

    bq::platform::jni_env env_holder;
    JNIEnv* env = env_holder.env;
    static jclass cls = env->FindClass("bq/log");
    if (!cls) {
        return;
    }
    static jmethodID mid = env->GetStaticMethodID(cls, "native_console_callbck", "(JIILjava/lang/String;)V");
    if (!mid) {
        return;
    }
    jstring message = env->NewStringUTF(content);
    env->CallStaticVoidMethod(cls, mid, log_id, category_idx, (int)log_level, message);
}
/*
 * Class:     bq_impl_log_invoker
 * Method:    __api_set_console_callback
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_bq_impl_log_1invoker__1_1api_1set_1console_1callback(JNIEnv* env, jclass, jboolean use_cb)
{
    (void)env;
    if (!use_cb) {
        bq::log::register_console_callback(java_console_callback);
        return;
    } else {
        bq::log::register_console_callback(java_console_callback);
    }
}

#ifdef __cplusplus
}
#endif
#endif
