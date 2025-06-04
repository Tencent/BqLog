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
#if defined(BQ_ANDROID)
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <android/log.h>
#include <android/asset_manager_jni.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        enum class file_seek_option : uint8_t;
        void android_jni_onload();

        struct android_asset_file_handle {
            void* asset = nullptr;
            bq::string path;
            operator bool() const
            {
                return asset != nullptr;
            }
        };

        struct android_asset_dir_handle {
            void* asset = nullptr;
            bq::string path;
            operator bool() const
            {
                return asset != nullptr;
            }
        };

        jobject get_global_context();

        bq::string get_files_dir();

        bq::string get_cache_dir();

        bq::string get_external_files_dir();

        const bq::string& get_apk_path();

        android_asset_file_handle android_asset_file_open(const bq::string& path);

        void android_asset_file_close(const android_asset_file_handle& handle);

        // return 0 if success, <0 on error
        int32_t android_asset_file_read(const android_asset_file_handle& handle, uint8_t* target_addr, size_t read_size, size_t& out_real_read_size);

        size_t android_asset_get_file_length(const android_asset_file_handle& handle);

        // return current pos, <0 on error
        int64_t android_asset_seek(const android_asset_file_handle& handle, file_seek_option opt, int64_t offset);

        android_asset_dir_handle android_asset_dir_open(const bq::string& path);

        void android_asset_dir_close(const android_asset_dir_handle& handle);

        bq::array<bq::string> android_asset_dir_get_sub_file_names(const android_asset_dir_handle& handle);

        bq::array<bq::string> android_asset_dir_get_sub_dir_names(const android_asset_dir_handle& handle);
    }
}
#endif
