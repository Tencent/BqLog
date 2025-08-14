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

#if defined(BQ_ANDROID)

#include <pthread.h>
#include <sys/prctl.h>
#include <jni.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unwind.h>
#include <dlfcn.h>
#include <string.h>
#include "bq_common/bq_common.h"

namespace bq {
	BQ_TLS_NON_POD(bq::string, stack_trace_current_str_);
	BQ_TLS_NON_POD(bq::u16string, stack_trace_current_str_u16_);
	namespace platform {
        static jclass cls_android_application_;
		static jclass cls_android_activity_thread_;
        static jclass cls_android_context_;
        static jclass cls_android_setting_secure_;

		static jmethodID method_get_application_;
		static jmethodID method_get_files_dir_;
		static jmethodID method_get_path_;
        static jmethodID method_get_cache_dir_;
		static jmethodID method_get_external_files_dir_;
		static jmethodID method_get_asset_manager_;
		static jmethodID method_get_content_resolver_;
        static jmethodID method_get_package_name_;
        static jmethodID method_get_application_info_;
		static jmethodID method_assets_manager_paths_list_;


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

            jmethodID current_activity_thread_method = env->GetStaticMethodID(cls_android_activity_thread_,
                "currentActivityThread",
                "()Landroid/app/ActivityThread;");
            jobject at = env->CallStaticObjectMethod(cls_android_activity_thread_, current_activity_thread_method);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get ActivityThread exception!");
			}
            if (NULL == at) {
                assert(false && "If your are using BQ in Android Service, please call bq.android.init(Context) before BQ initialization");
            }
            jobject context = env->CallObjectMethod(at, method_get_application_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get application exception!");
			}
            jobject context_inst_new = env->NewGlobalRef(context);
            if (context_inst_atomic.compare_exchange_strong(context_inst, context_inst_new)) {
                return context_inst_new;
            }
            env->DeleteGlobalRef(context_inst_new);
            return context_inst_atomic.load();
        }

        base_dir_initializer::base_dir_initializer(){
            //empty implementation
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
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject files_dir_obj = env->CallObjectMethod(context, method_get_files_dir_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get files exception!");
			}
            if (files_dir_obj == nullptr) {
                return "";
            }
			jstring path_str = static_cast<jstring>(env->CallObjectMethod(files_dir_obj, method_get_path_));			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "getPath exception!");
			}
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
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject cache_dir_obj = env->CallObjectMethod(context, method_get_cache_dir_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get cache dir exception!");
			}
            if (cache_dir_obj == nullptr) {
                return "";
            }
            jstring path_str = static_cast<jstring>(env->CallObjectMethod(cache_dir_obj, method_get_path_));
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "getPath exception!");
			}
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
            jobject context = get_global_context();
            if (context == nullptr) {
                return "";
            }
            jobject external_files_dir_obj = env->CallObjectMethod(context, method_get_external_files_dir_, nullptr);
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                __android_log_print(ANDROID_LOG_WARN, "Bq", "Exception is thrown while calling getExternalFilesDir, are you init BQ in isolateProcess?");
                return "";
            }
            if (external_files_dir_obj == nullptr) {
                return "";
            }
            jstring path_str = static_cast<jstring>(env->CallObjectMethod(external_files_dir_obj, method_get_path_));
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get external dir exception!");
			}
            if (path_str) {
                const char* path_c_str = env->GetStringUTFChars(path_str, NULL);
                bq::string path = path_c_str;
                env->ReleaseStringUTFChars(path_str, path_c_str);
                return path;
            }
            return "";
        }

        static void init_android_reflection_variables()
		{
			jni_env env_holder;
			JNIEnv* env = env_holder.env;
            cls_android_application_ = env->FindClass("android/app/Application");
			cls_android_activity_thread_ = env->FindClass("android/app/ActivityThread");
			cls_android_context_ = env->FindClass("android/content/Context");
			cls_android_setting_secure_ = env->FindClass("android/provider/Settings$Secure");
            assert(cls_android_application_ 
                && cls_android_activity_thread_
                && cls_android_context_
                && cls_android_setting_secure_
                && "Find Class failed");
			method_get_application_ = env->GetMethodID(cls_android_activity_thread_, "getApplication", "()Landroid/app/Application;");
			method_get_files_dir_ = env->GetMethodID(cls_android_context_, "getFilesDir", "()Ljava/io/File;");
			jclass file_class = env->FindClass("java/io/File");
			method_get_path_ = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
            method_get_cache_dir_ = env->GetMethodID(cls_android_context_, "getCacheDir", "()Ljava/io/File;");
			method_get_external_files_dir_ = env->GetMethodID(cls_android_context_, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
            method_get_asset_manager_ = env->GetMethodID(cls_android_context_, "getAssets", "()Landroid/content/res/AssetManager;");
            method_get_content_resolver_ = env->GetMethodID(cls_android_application_, "getContentResolver", "()Landroid/content/ContentResolver;");
            method_get_package_name_ = env->GetMethodID(cls_android_application_, "getPackageName", "()Ljava/lang/String;");
            method_get_application_info_ = env->GetMethodID(cls_android_application_, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
            jclass assets_manager_class = env->FindClass("android/content/res/AssetManager");
            method_assets_manager_paths_list_ = env->GetMethodID(assets_manager_class, "list", "(Ljava/lang/String;)[Ljava/lang/String;");

			assert(method_get_application_
				&& method_get_files_dir_
				&& method_get_path_
				&& method_get_cache_dir_
				&& method_get_external_files_dir_
				&& method_get_asset_manager_
				&& method_get_content_resolver_
				&& method_get_package_name_
				&& method_get_application_info_
				&& method_assets_manager_paths_list_
				&& "Find method failed");
      }

        static void init_android_asset_manager()
        {
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            jobject android_asset_manager_tmp = env->CallObjectMethod(context, method_get_asset_manager_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get asset manager exception!");
			}
            common_global_vars::get().android_asset_manager_java_instance_ = env->NewGlobalRef(android_asset_manager_tmp);
            common_global_vars::get().android_asset_manager_inst_ = AAssetManager_fromJava(env, common_global_vars::get().android_asset_manager_java_instance_);
        }

        const bq::string& get_base_dir(bool is_sandbox)
        {
            assert(bq::platform::get_jvm() && "JNI_Onload was not invoked yet");
            if (is_sandbox) {
                return common_global_vars::get().base_dir_init_inst_.base_dir_0_;
            } else {
                return common_global_vars::get().base_dir_init_inst_.base_dir_1_;
            }
        }

        const bq::string& get_android_id()
        {
            if (!common_global_vars::get().android_id_.is_empty())
                return common_global_vars::get().android_id_;
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            // get contentResolver
            jobject resolver_instance = env->CallObjectMethod(context, method_get_content_resolver_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get content resolver exception!");
			}
            // get common_global_vars::get().android_id_ from android Settings$Secure
            jmethodID method_id = env->GetStaticMethodID(cls_android_setting_secure_, "getString",
                "(Landroid/content/ContentResolver;Ljava/lang/"
                "String;)Ljava/lang/String;");
            jstring android_id = env->NewStringUTF("common_global_vars::get().android_id_");
            jstring android_id_string = (jstring)env->CallStaticObjectMethod(
                cls_android_setting_secure_, method_id, resolver_instance, android_id);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get android id exception!");
			}
            const char* android_id_c_str = env->GetStringUTFChars(android_id_string, JNI_FALSE);
            __android_log_print(ANDROID_LOG_INFO, "Bq", "common_global_vars::get().android_id_: %s\n", android_id_c_str);
            common_global_vars::get().android_id_ = android_id_c_str;
            env->ReleaseStringUTFChars(android_id_string, android_id_c_str);
            return common_global_vars::get().android_id_;
        }

        const bq::string& get_package_name()
        {
            if (!common_global_vars::get().android_package_name_.is_empty())
                return common_global_vars::get().android_package_name_;
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            jstring package_name_string = (jstring)env->CallObjectMethod(context,
                method_get_package_name_);
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "get package name exception!");
			}
            const char* package_name_c_str = env->GetStringUTFChars(package_name_string, JNI_FALSE);
            __android_log_print(ANDROID_LOG_INFO, "Bq", "common_global_vars::get().android_package_name_: %s\n", package_name_c_str);
            common_global_vars::get().android_package_name_ = package_name_c_str;
            env->ReleaseStringUTFChars(package_name_string, package_name_c_str);
            return common_global_vars::get().android_package_name_;
        }

        const bq::string& get_apk_path()
        {
            bq::string& apk_path = common_global_vars::get().apk_path_;
            bq::platform::scoped_spin_lock lock(common_global_vars::get().apk_path_spin_lock_);
            if (!apk_path.is_empty()) {
                return apk_path;
            }
            jni_env env_holder;
            JNIEnv* env = env_holder.env;
            jobject context = get_global_context();
            jobject application_info_object = env->CallObjectMethod(context, method_get_application_info_);
			if (env->ExceptionOccurred()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				__android_log_print(ANDROID_LOG_ERROR, "Bq", "context.getApplicationInfo() exception!");
				return apk_path;
			}
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
            result.asset = (void*)AAssetManager_open(common_global_vars::get().android_asset_manager_inst_, path.c_str(), AASSET_MODE_UNKNOWN);
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
            result.asset = (void*)AAssetManager_openDir(common_global_vars::get().android_asset_manager_inst_, path.c_str());
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
            jstring java_path = env->NewStringUTF(handle.path.c_str());
            jobjectArray string_array = (jobjectArray)env->CallObjectMethod(common_global_vars::get().android_asset_manager_java_instance_, method_assets_manager_paths_list_, java_path);
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

            init_android_reflection_variables();
            init_android_asset_manager();

            common_global_vars::get().base_dir_init_inst_.base_dir_0_ = get_files_dir();
            common_global_vars::get().base_dir_init_inst_.base_dir_1_ = get_external_files_dir();

            // priority:
            // for sand box dir: internal storage -> external storage -> cache storage
            // for not in sand box dir: external storage -> internal storage -> cache storage
            bool need_alarm = false;
            if (common_global_vars::get().base_dir_init_inst_.base_dir_0_.is_empty() || !can_write_to_dir(common_global_vars::get().base_dir_init_inst_.base_dir_0_)) {
                bq::string candidiate_path;
                if (common_global_vars::get().base_dir_init_inst_.base_dir_1_.is_empty() || !can_write_to_dir(common_global_vars::get().base_dir_init_inst_.base_dir_1_)) {
                    need_alarm = true;
                    candidiate_path = get_cache_dir();
                } else {
                    candidiate_path = common_global_vars::get().base_dir_init_inst_.base_dir_1_;
                }
                __android_log_print(ANDROID_LOG_WARN, "Bq",
                    "common_global_vars::get().base_dir_init_inst_.base_dir_0_:\"%s\" can_write_to_dir = false, use \"%s\" instead",
                    common_global_vars::get().base_dir_init_inst_.base_dir_0_.c_str(),
                    candidiate_path.c_str());
                common_global_vars::get().base_dir_init_inst_.base_dir_0_ = candidiate_path;
            }
            if (common_global_vars::get().base_dir_init_inst_.base_dir_1_.is_empty() || !can_write_to_dir(common_global_vars::get().base_dir_init_inst_.base_dir_1_)) {
                bq::string candidiate_path;
                if (common_global_vars::get().base_dir_init_inst_.base_dir_0_.is_empty() || !can_write_to_dir(common_global_vars::get().base_dir_init_inst_.base_dir_0_)) {
                    need_alarm = true;
                    candidiate_path = get_cache_dir();
                } else {
                    candidiate_path = common_global_vars::get().base_dir_init_inst_.base_dir_0_;
                }
                __android_log_print(ANDROID_LOG_WARN, "Bq",
                    "common_global_vars::get().base_dir_init_inst_.base_dir_1_:\"%s\" can_write_to_dir = false, use \"%s\" instead",
                    common_global_vars::get().base_dir_init_inst_.base_dir_1_.c_str(),
                    candidiate_path.c_str());
                common_global_vars::get().base_dir_init_inst_.base_dir_1_ = candidiate_path;
            }
            __android_log_print(ANDROID_LOG_INFO, "Bq", "Storage Path In Sand Box:%s", common_global_vars::get().base_dir_init_inst_.base_dir_0_.c_str());
            __android_log_print(ANDROID_LOG_INFO, "Bq", "Storage Path Out Sand Box:%s", common_global_vars::get().base_dir_init_inst_.base_dir_1_.c_str());
            if (need_alarm) {
                __android_log_print(ANDROID_LOG_ERROR, "Bq", "If you are using BQ in isolateProcess Android Service, you may not have any I/O read/write permissions, and all file read/write operations will fail.");
            }
        }

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
            if (!bq::stack_trace_current_str_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::string& stack_trace_str_ref = bq::stack_trace_current_str_.get();
            stack_trace_str_ref.clear();
            void* buffer[max_stack_size_];
            android_backtrace_state state = { buffer, buffer + max_stack_size_ };
            _Unwind_Backtrace(unwindCallback, &state);
            size_t stack_count = static_cast<size_t>(state.current - buffer);
            uint32_t valid_frame_count = 0;
            for (int32_t idx = static_cast<int32_t>(skip_frame_count); idx < static_cast<int32_t>(stack_count); ++idx) {
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
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "\n#%d %p ", idx, addr);
                stack_trace_str_ref += tmp;
                stack_trace_str_ref += symbol;
            }
            out_str_ptr = stack_trace_str_ref.c_str();
            out_char_count = static_cast<uint32_t>(stack_trace_str_ref.size());
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            if (!bq::stack_trace_current_str_u16_) {
                out_str_ptr = nullptr;
                out_char_count = 0;
                return; // This occurs when program exit in Main thread.
            }
            bq::u16string& stack_trace_str_ref = bq::stack_trace_current_str_u16_.get();
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_str_ref.clear();
            stack_trace_str_ref.fill_uninitialized((u8_char_count << 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_str_ref.begin(), (uint32_t)stack_trace_str_ref.size());
            assert(encoded_size < stack_trace_str_ref.size());
            stack_trace_str_ref.erase(stack_trace_str_ref.begin() + encoded_size, stack_trace_str_ref.size() - encoded_size);
            out_str_ptr = stack_trace_str_ref.begin();
            out_char_count = (uint32_t)stack_trace_str_ref.size();
        }
    }
}
#endif
