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
#if defined(BQ_OHOS)
#include <pthread.h>
#include <time.h>
#include <filemanagement/environment/oh_environment.h>
#include <filemanagement/file_uri/oh_file_uri.h>
#include <hilog/log.h>
#include "bundle/native_interface_bundle.h"
#include "napi/native_api.h"
#include <js_native_api_types.h>

namespace bq {
    namespace platform {
        static napi_env ohos_env = NULL;
        // According to test result, benifit from VDSO.
        //"CLOCK_REALTIME_COARSE clock_gettime"  has higher performance
        //  than "gettimeofday" and event "TSC" on Android and Linux.
        uint64_t high_performance_epoch_ms()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            uint64_t epoch_milliseconds = (uint64_t)(ts.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec) / 1000000;
            return epoch_milliseconds;
        }

        base_dir_initializer::base_dir_initializer()
        {
            // empty implementation
        }

        static void ohos_onload()
        {
            string harmony_external_storage_path;
            string harmony_internal_storage_path;
            char *urlpath = nullptr;
            char *relPath = nullptr;
            char *Path = NULL;
            FileManagement_ErrCode ret = OH_Environment_GetUserDocumentDir (&Path);
            if (ret == 0) {
                harmony_external_storage_path = Path;
                free(Path);
                OH_NativeBundle_ApplicationInfo nativeApplicationInfo = OH_NativeBundle_GetCurrentApplicationInfo();
                harmony_external_storage_path = harmony_external_storage_path +"/"+nativeApplicationInfo.bundleName;
                common_global_vars::get().base_dir_init_inst_.base_dir_0_ = harmony_external_storage_path;
                free(nativeApplicationInfo.bundleName);
            } else {
                 OH_LOG_INFO(LOG_APP,"GetDocumentPath failed, error code is %d", ret);
            }
    
            string uri = "/data/storage/el2/base/files/";
            FileManagement_ErrCode errid = OH_FileUri_GetUriFromPath(uri.c_str(),uri.size(),&urlpath);
            if(errid!=0)
            {
                harmony_internal_storage_path = harmony_external_storage_path;
                common_global_vars::get().base_dir_init_inst_.base_dir_1_ = harmony_internal_storage_path;
                 OH_LOG_INFO(LOG_APP,"OH_FileUri_GetUriFromPath failed, error code is %d", errid);
                return;
            }
            errid = OH_FileUri_GetPathFromUri(urlpath,strlen(urlpath),&relPath);
            if(errid == 0)
            {
                harmony_internal_storage_path = relPath;
                if(harmony_external_storage_path.is_empty())
                    harmony_external_storage_path = harmony_internal_storage_path;
            
                common_global_vars::get().base_dir_init_inst_.base_dir_1_ = harmony_internal_storage_path;
                common_global_vars::get().base_dir_init_inst_.base_dir_0_ = harmony_external_storage_path;
                free(relPath);
            }
            else{
                 OH_LOG_INFO(LOG_APP,"OH_FileUri_GetUriFromPath failed, error code is %d", errid);
                 harmony_internal_storage_path = "/data/storage/el2/base/files";                
                 common_global_vars::get().base_dir_init_inst_.base_dir_1_ = harmony_internal_storage_path;
            }
            free(urlpath);            
        }
        
        const bq::string& get_base_dir(bool is_sandbox)
        {
            assert(ohos_env && "JNI_Onload was not invoked yet");
            if(is_sandbox)
                return common_global_vars::get().base_dir_init_inst_.base_dir_1_;
            else
                return common_global_vars::get().base_dir_init_inst_.base_dir_0_;
        }



        static napi_value NAPI_Global_add(napi_env env, napi_callback_info info)
        {
            size_t argc = 2;
            napi_value args[2] = {nullptr};
        
            napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);
        
            napi_valuetype valuetype0;
            napi_typeof(env, args[0], &valuetype0);
        
            napi_valuetype valuetype1;
            napi_typeof(env, args[1], &valuetype1);
        
            double value0;
            napi_get_value_double(env, args[0], &value0);
        
            double value1;
            napi_get_value_double(env, args[1], &value1);
        
            napi_value sum;
            napi_create_double(env, value0 + value1, &sum);
        
            return sum;
        
        }

        EXTERN_C_START
        static napi_value Init(napi_env env, napi_value exports)
        {
            bq::platform::ohos_env = env;
                napi_property_descriptor desc[] = {
                { "add", nullptr, NAPI_Global_add, nullptr, nullptr, nullptr, napi_default, nullptr }
            };
            napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
            OH_NativeBundle_ApplicationInfo nativeApplicationInfo = OH_NativeBundle_GetCurrentApplicationInfo();
            // Native接口获取的应用包名转为js对象里的bundleName属性
            OH_LOG_Print(LOG_APP, LogLevel::LOG_DEBUG, 0, "Bq", nativeApplicationInfo.bundleName,"");
            bq::platform::ohos_onload();
            return exports;
        }
        EXTERN_C_END
        
        static napi_module nm_BqLog = {
            1,                // nm_version
            0,                // nm_flags
            nullptr,          // nm_filename
            Init,             // nm_register_func
            "BqLog",          // nm_modname
            ((void*)0),       // nm_priv
            { 0 }             // reserved
        }; 
        
        extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
        {
            napi_module_register(&nm_BqLog);
        }

        bool share_file(const char* file_path)
        {
            (void)file_path;
            return false;
        }
    }
}
#endif
