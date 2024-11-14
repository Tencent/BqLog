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
#include "bq_common/bq_common.h"

#if BQ_ANDROID

#include <pthread.h>
#include <sys/prctl.h>
#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <android/asset_manager_jni.h>
#include <unwind.h>
#include <dlfcn.h>
#include <string.h>

namespace bq {
    namespace platform {
        // According to test result, benefit from VDSO.
        //"CLOCK_REALTIME_COARSE clock_gettime"  has higher performance
        //  than "gettimeofday" and event "TSC" on Android and Linux.
        uint64_t high_performance_epoch_ms()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            uint64_t epoch_milliseconds = (uint64_t)(ts.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec) / 1000000;
            return epoch_milliseconds;
        }

        static jobject android_asset_manager_java_instance = nullptr;
        static AAssetManager* android_asset_manager_inst = nullptr;
        static bq::string android_internal_storage_path;
        static bq::string android_external_storage_path;
        static bq::string android_id;
        static bq::string android_package_name;

        jobject get_global_context()
        {
            static bq::platform::atomic<jobject> context_inst_atomic = nullptr;
            jobject context_inst = context_inst_atomic.load(memory_order::relaxed);
            if (context_inst) {
                return context_inst;
            }
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            assert(env != NULL && "get jni env failed");

            jclass activity_thread_class = env->FindClass("android/app/ActivityThread");
            jmethodID current_activity_thread_method = env->GetStaticMethodID(activity_thread_class,
                "currentActivityThread",
                "()Landroid/app/ActivityThread;");
            jobject at = env->CallStaticObjectMethod(activity_thread_class, current_activity_thread_method);
            if (NULL == at) {
                assert(false && "If your are using BQ in Android Service, please call bq.android.init(Context) before BQ initialization");
            }
            jmethodID get_application_method = env->GetMethodID(activity_thread_class, "getApplication",
                "()Landroid/app/Application;");
            jobject context = env->CallObjectMethod(at, get_application_method);
            jobject context_inst_new = env->NewGlobalRef(context);
            if (context_inst_atomic.compare_exchange_strong(context_inst, context_inst_new)) {
                return context_inst_new;
            }
            env->DeleteGlobalRef(context_inst_new);
            return context_inst_atomic.load();
        }

        bool can_write_to_dir(bq::string path)
        {
            bq::string test_path = path + "/bq_test_dir";
            if (bq::platform::make_dir(test_path.c_str()) != 0) {
                return false;
            }
            int32_t index = bq::util::rand() % 1000000;
            char name[128] = { 0 };
            sprintf(name, "/test_%d.txt", index);
            bq::string test_file_name = test_path + name;
            bq::platform::platform_file_handle file_handle;
            if (bq::platform::open_file(test_file_name.c_str(), bq::platform::file_open_mode_enum::auto_create | bq::platform::file_open_mode_enum::read_write, file_handle) != 0)
                return false;
            if (bq::platform::close_file(file_handle) != 0)
                return false;
            if (bq::platform::remove_dir_or_file(test_file_name.c_str()) != 0)
                return false;
            if (bq::platform::remove_dir_or_file(test_path.c_str()) != 0)
                return false;
            return true;
        }

        bq::string get_files_dir()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jclass context_class = env->FindClass("android/content/Context");
            jmethodID get_files_dir_method = env->GetMethodID(context_class,
                "getFilesDir",
                "()Ljava/io/File;");
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject files_dir_obj = env->CallObjectMethod(context, get_files_dir_method);

            if (files_dir_obj == nullptr) {
                return "";
            }
            jclass file_class = env->GetObjectClass(files_dir_obj);
            jmethodID get_path_method = env->GetMethodID(file_class, "getPath",
                "()Ljava/lang/String;");
            jstring path_str = static_cast<jstring>(env->CallObjectMethod(files_dir_obj,
                get_path_method));
            if (path_str) {
                const char* path_c_str = env->GetStringUTFChars(path_str, NULL);
                bq::string path = path_c_str;
                env->ReleaseStringUTFChars(path_str, path_c_str);
                return path;
            }
            return "";
        }

        bq::string get_cache_dir()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jclass context_class = env->FindClass("android/content/Context");
            jmethodID get_cache_dir_method = env->GetMethodID(context_class,
                "getCacheDir",
                "()Ljava/io/File;");
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject cache_dir_obj = env->CallObjectMethod(context, get_cache_dir_method);

            if (cache_dir_obj == nullptr) {
                return "";
            }
            jclass file_class = env->GetObjectClass(cache_dir_obj);
            jmethodID get_path_method = env->GetMethodID(file_class, "getPath",
                "()Ljava/lang/String;");
            jstring path_str = static_cast<jstring>(env->CallObjectMethod(cache_dir_obj,
                get_path_method));
            if (path_str) {
                const char* path_c_str = env->GetStringUTFChars(path_str, NULL);
                bq::string path = path_c_str;
                env->ReleaseStringUTFChars(path_str, path_c_str);
                return path;
            }
            return "";
        }

        bq::string get_external_files_dir()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jclass context_class = env->FindClass("android/content/Context");
            jmethodID get_external_files_dir_method = env->GetMethodID(context_class,
                "getExternalFilesDir",
                "(Ljava/lang/String;)Ljava/io/File;");
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject external_files_dir_obj = env->CallObjectMethod(context, get_external_files_dir_method,
                nullptr);
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                __android_log_print(ANDROID_LOG_WARN, "Bq", "Exception is thrown while calling getExternalFilesDir, are you init BQ in isolateProcess?");
                return "";
            }
            if (external_files_dir_obj == nullptr) {
                return "";
            }
            jclass file_class = env->GetObjectClass(external_files_dir_obj);
            jmethodID get_path_method = env->GetMethodID(file_class, "getPath",
                "()Ljava/lang/String;");
            jstring path_str = static_cast<jstring>(env->CallObjectMethod(external_files_dir_obj,
                get_path_method));
            if (path_str) {
                const char* path_c_str = env->GetStringUTFChars(path_str, NULL);
                bq::string path = path_c_str;
                env->ReleaseStringUTFChars(path_str, path_c_str);
                return path;
            }
            return "";
        }

        void init_android_asset_manager()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jclass context_class = env->FindClass("android/content/Context");
            jmethodID get_asset_manager_method = env->GetMethodID(context_class,
                "getAssets",
                "()Landroid/content/res/AssetManager;");
            jobject context = get_global_context();
            jobject android_asset_manager_tmp = env->CallObjectMethod(context, get_asset_manager_method);
            android_asset_manager_java_instance = env->NewGlobalRef(android_asset_manager_tmp);
            android_asset_manager_inst = AAssetManager_fromJava(env, android_asset_manager_java_instance);
        }

        const bq::string& get_base_dir(bool is_sandbox)
        {
            assert(bq::platform::get_jvm() && "JNI_Onload was not invoked yet");
            if (is_sandbox) {
                return android_internal_storage_path;
            } else {
                return android_external_storage_path;
            }
        }

        const bq::string& get_android_id()
        {
            if (!android_id.is_empty())
                return android_id;
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            // get contentResolver
            jclass activity = env->GetObjectClass(context);
            jmethodID method = env->GetMethodID(activity, "getContentResolver",
                "()Landroid/content/ContentResolver;");
            jobject resolver_instance = env->CallObjectMethod(context, method);

            // get android_id from android Settings$Secure
            jclass android_settings_class = env->FindClass("android/provider/Settings$Secure");
            jmethodID method_id = env->GetStaticMethodID(android_settings_class, "getString",
                "(Landroid/content/ContentResolver;Ljava/lang/"
                "String;)Ljava/lang/String;");
            jstring param_android_id = env->NewStringUTF("android_id");
            jstring android_id_string = (jstring)env->CallStaticObjectMethod(
                android_settings_class, method_id, resolver_instance, param_android_id);
            const char* android_id_c_str = env->GetStringUTFChars(android_id_string, JNI_FALSE);
            __android_log_print(ANDROID_LOG_INFO, "Bq", "android_id: %s\n", android_id_c_str);
            android_id = android_id_c_str;
            env->ReleaseStringUTFChars(android_id_string, android_id_c_str);
            return android_id;
        }

        const bq::string& get_package_name()
        {
            if (!android_package_name.is_empty())
                return android_package_name;
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            jclass context_class = env->GetObjectClass(context);
            jmethodID get_package_name_method = env->GetMethodID(context_class, "getPackageName",
                "()Ljava/lang/String;");
            jstring package_name_string = (jstring)env->CallObjectMethod(context,
                get_package_name_method);
            const char* package_name_c_str = env->GetStringUTFChars(package_name_string, JNI_FALSE);
            __android_log_print(ANDROID_LOG_INFO, "Bq", "android_package_name: %s\n", package_name_c_str);
            android_package_name = package_name_c_str;
            env->ReleaseStringUTFChars(package_name_string, package_name_c_str);
            return android_package_name;
        }

        const bq::string& get_apk_path()
        {
            static bq::string apk_path;
            if (!apk_path.is_empty()) {
                return apk_path;
            }
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            jclass context_class = env->GetObjectClass(context);
            jmethodID get_application_info_method = env->GetMethodID(context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                __android_log_print(ANDROID_LOG_ERROR, "Bq", "context.getApplicationInfo() exception!");
                return apk_path;
            }
            jobject application_info_object = env->CallObjectMethod(context, get_application_info_method);
            jclass application_info_class = env->GetObjectClass(application_info_object);
            jfieldID source_dir_field_id = env->GetFieldID(application_info_class, "sourceDir", "Ljava/lang/String;");
            jstring source_dir_jstring = (jstring)env->GetObjectField(application_info_object, source_dir_field_id);
            const char* source_dir_cstr = env->GetStringUTFChars(source_dir_jstring, NULL);
            apk_path = source_dir_cstr;
            env->ReleaseStringUTFChars(source_dir_jstring, source_dir_cstr);
            return apk_path;
        }

        android_asset_file_handle android_asset_file_open(const bq::string& path)
        {
            android_asset_file_handle result;
            result.asset = (void*)AAssetManager_open(android_asset_manager_inst, path.c_str(), AASSET_MODE_UNKNOWN);
            result.path = path.trim();
            return result;
        }

        void android_asset_file_close(const android_asset_file_handle& handle)
        {
            AAsset_close((AAsset*)handle.asset);
        }

        int32_t android_asset_file_read(const android_asset_file_handle& handle, uint8_t* target_addr, size_t read_size, size_t& out_real_read_size)
        {
            out_real_read_size = 0;
            size_t max_size_pertime = static_cast<size_t>(INT32_MAX);
            size_t max_read_size_current = 0;
            while (out_real_read_size < read_size) {
                size_t need_read_size_this_time = bq::min_value(max_size_pertime, read_size - max_read_size_current);
                int32_t out_size = AAsset_read((AAsset*)handle.asset, target_addr, need_read_size_this_time);
                if (out_size < 0) {
                    return out_size;
                }
                out_real_read_size += static_cast<size_t>(out_size);
                if (out_size < static_cast<int32_t>(need_read_size_this_time)) {
                    return 0;
                }
            }
            return 0;
        }

        size_t android_asset_get_file_length(const android_asset_file_handle& handle)
        {
            return static_cast<size_t>(AAsset_getLength64((AAsset*)handle.asset));
        }

        // return current pos, <0 on error
        int64_t android_asset_seek(const android_asset_file_handle& handle, file_seek_option opt, int64_t offset)
        {
            int32_t opt_platform = SEEK_CUR;
            switch (opt) {
            case bq::platform::file_seek_option::current:
                opt_platform = SEEK_CUR;
                break;
            case bq::platform::file_seek_option::begin:
                opt_platform = SEEK_SET;
                break;
            case bq::platform::file_seek_option::end:
                opt_platform = SEEK_END;
                break;
            default:
                break;
            }
            auto result = AAsset_seek64((AAsset*)handle.asset, static_cast<off64_t>(offset), opt_platform);
            return (int64_t)result;
        }

        android_asset_dir_handle android_asset_dir_open(const bq::string& path)
        {
            android_asset_dir_handle result;
            result.asset = (void*)AAssetManager_openDir(android_asset_manager_inst, path.c_str());
            result.path = path.trim();
            // AAssetManager_openDir will never return null whether it is directory or not.
            // so we use this method to ensure it is a real directory.
            //(tips: APK will remove empty folders in /assets.
            // so empty folder is not considered.
            if (android_asset_dir_get_sub_file_names(result).is_empty()
                && android_asset_dir_get_sub_dir_names(result).is_empty()) {
                if (result.asset) {
                    AAssetDir_close((AAssetDir*)result.asset);
                    result.asset = nullptr;
                }
            }
            return result;
        }

        void android_asset_dir_close(const android_asset_dir_handle& handle)
        {
            AAssetDir_close((AAssetDir*)handle.asset);
        }

        bq::array<bq::string> android_asset_dir_get_sub_file_names(const android_asset_dir_handle& handle)
        {
            bq::array<bq::string> list;
            if (!handle) {
                return list;
            }
            while (true) {
                const char* next_name = AAssetDir_getNextFileName((AAssetDir*)handle.asset);
                if (!next_name) {
                    break;
                }
                list.push_back(next_name);
            }
            AAssetDir_rewind((AAssetDir*)handle.asset);
            return list;
        }

        bq::array<bq::string> android_asset_dir_get_sub_dir_names(const android_asset_dir_handle& handle)
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            bq::array<bq::string> list;
            jclass cls = env->GetObjectClass(android_asset_manager_java_instance);
            jmethodID list_method = env->GetMethodID(cls, "list", "(Ljava/lang/String;)[Ljava/lang/String;");
            jstring java_path = env->NewStringUTF(handle.path.c_str());
            jobjectArray string_array = (jobjectArray)env->CallObjectMethod(android_asset_manager_java_instance, list_method, java_path);
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                bq::util::log_device_console(bq::log_level::error, "AssetManager.list Exception Occurred!");
                return list;
            }
            jint length = env->GetArrayLength(string_array);
            for (jint i = 0; i < length; i++) {
                jstring str = (jstring)env->GetObjectArrayElement(string_array, i);
                const char* raw_string = env->GetStringUTFChars(str, nullptr);
                bq::string name = raw_string;
                env->ReleaseStringUTFChars(str, raw_string);
                bq::string full_path;
                if (handle.path.is_empty() || handle.path.end_with("/")) {
                    full_path = handle.path + name;
                } else {
                    full_path = handle.path + "/" + name;
                }
                auto dir_handle = android_asset_dir_open(full_path);
                if (dir_handle) {
                    list.push_back(name);
                    android_asset_dir_close(dir_handle);
                }
            }
            return list;
        }

        void android_jni_onload()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            assert(env != NULL && "get jni env failed");

            init_android_asset_manager();

            android_internal_storage_path = get_files_dir();
            android_external_storage_path = get_external_files_dir();

            // priority:
            // for sand box dir: internal storage -> external storage -> cache storage
            // for not in sand box dir: external storage -> internal storage -> cache storage
            bool need_alarm = false;
            if (android_internal_storage_path.is_empty() || !can_write_to_dir(android_internal_storage_path)) {
                bq::string candidiate_path;
                if (android_external_storage_path.is_empty() || !can_write_to_dir(android_external_storage_path)) {
                    need_alarm = true;
                    candidiate_path = get_cache_dir();
                } else {
                    candidiate_path = android_external_storage_path;
                }
                __android_log_print(ANDROID_LOG_WARN, "Bq",
                    "android_internal_storage_path:\"%s\" can_write_to_dir = false, use \"%s\" instead",
                    android_internal_storage_path.c_str(),
                    candidiate_path.c_str());
                android_internal_storage_path = candidiate_path;
            }
            if (android_external_storage_path.is_empty() || !can_write_to_dir(android_external_storage_path)) {
                bq::string candidiate_path;
                if (android_internal_storage_path.is_empty() || !can_write_to_dir(android_internal_storage_path)) {
                    need_alarm = true;
                    candidiate_path = get_cache_dir();
                } else {
                    candidiate_path = android_internal_storage_path;
                }
                __android_log_print(ANDROID_LOG_WARN, "Bq",
                    "android_external_storage_path:\"%s\" can_write_to_dir = false, use \"%s\" instead",
                    android_external_storage_path.c_str(),
                    candidiate_path.c_str());
                android_external_storage_path = candidiate_path;
            }
            __android_log_print(ANDROID_LOG_INFO, "Bq", "Storage Path In Sand Box:%s", android_internal_storage_path.c_str());
            __android_log_print(ANDROID_LOG_INFO, "Bq", "Storage Path Out Sand Box:%s", android_external_storage_path.c_str());
            if (need_alarm) {
                __android_log_print(ANDROID_LOG_ERROR, "Bq", "If you are using BQ in isolateProcess Android Service, you may not have any I/O read/write permissions, and all file read/write operations will fail.");
            }
        }

        static constexpr uint32_t max_stack_trace_char_count = 1024 * 16;
        // thread local is not valid when stl = none, T_T.
        static BQ_TLS char stack_trace_current_str_[max_stack_trace_char_count];
        static BQ_TLS char16_t stack_trace_current_str_u16_[max_stack_trace_char_count];
        static BQ_TLS uint32_t stack_trace_str_len_;
        static BQ_TLS uint32_t stack_trace_str_u16_len_;
        static constexpr size_t max_stack_size_ = 128;

        struct android_backtrace_state {
            void** current;
            void** end;
        };

        static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
        {
            android_backtrace_state* state = static_cast<android_backtrace_state*>(arg);
            uintptr_t pc = _Unwind_GetIP(context);
            if (pc) {
                if (state->current == state->end) {
                    return _URC_END_OF_STACK;
                } else {
                    *state->current++ = reinterpret_cast<void*>(pc);
                }
            }
            return _URC_NO_REASON;
        }

        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            stack_trace_str_len_ = 0;
            void* buffer[max_stack_size_];
            android_backtrace_state state = { buffer, buffer + max_stack_size_ };
            _Unwind_Backtrace(unwindCallback, &state);
            size_t stack_count = state.current - buffer;
            uint32_t valid_frame_count = 0;
            for (int32_t idx = skip_frame_count; idx < (int32_t)stack_count; ++idx) {
                if (stack_trace_str_len_ >= max_stack_trace_char_count) {
                    break;
                }
                const void* addr = buffer[idx];
                const char* symbol = "(unknown symbol)";
                Dl_info info;
                if (dladdr(addr, &info) && info.dli_sname) {
                    symbol = info.dli_sname;
                }
                if (valid_frame_count == 0) {
                    if (strstr(symbol, "get_stack_trace")) {
                        continue;
                    }
                    valid_frame_count = 1;
                }
                if (valid_frame_count++ <= skip_frame_count) {
                    continue;
                }

                uint32_t max_len = max_stack_trace_char_count - stack_trace_str_len_;
                int32_t write_number = snprintf(stack_trace_current_str_ + stack_trace_str_len_, max_len, "\n#%d %p %s", idx, addr, symbol);
                if (write_number < 0) {
                    out_str_ptr = "[get stack trace error]";
                    out_char_count = (uint32_t)strlen("[get stack trace error]");
                    return;
                }
                stack_trace_str_len_ += write_number;
            }
            out_str_ptr = stack_trace_current_str_;
            out_char_count = stack_trace_str_len_;
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_str_u16_len_ = bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_current_str_u16_, max_stack_trace_char_count);
            out_str_ptr = stack_trace_current_str_u16_;
            out_char_count = stack_trace_str_u16_len_;
        }
    }
}
#endif
