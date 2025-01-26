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
/*!
 * Thread Local Storage Datas for log buffer.
 *
 * \author pippocao
 * \date 2025/1/24
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/block_list.h"
#if BQ_JAVA
#include <jni.h>
#endif

namespace bq {
#if BQ_WIN
    BQ_STRUCT_PACK(struct tls_data_for_win {
        bq::platform::thread::thread_id thread_id_;
        uint64_t last_update_epoch_ms_;
        HANDLE thread_handle_;
#if BQ_JAVA
        jobjectArray direct_buffer_array_;
        uint64_t offset_buffer_;
#endif
    });
    extern BQ_TLS block_node_head* windows_tls_node_;

    static constexpr uint64_t THREAD_ALIVE_UPDATE_INTERVAL = 200;
    static constexpr uint64_t BLOCK_THREAD_VALID_CHECK_THRESHOLD = THREAD_ALIVE_UPDATE_INTERVAL * 2;

    void create_windows_tls_node();
#elif BQ_POSIX
    extern BQ_TLS bool is_posix_log_thread_registered_;
#if BQ_JAVA
    extern BQ_TLS jobjectArray direct_buffer_array_;
    extern BQ_TLS uint64_t offset_buffer_;
#endif

    void register_posix_log_thread();
#else
    assert(false, "unsupported platform!");
#endif


    bq_forceinline void mark_current_log_thread_alive(uint64_t current_epoch_ms)
    {
#if BQ_WIN
        if (!windows_tls_node_) {
            create_windows_tls_node();
        }
        auto& misc_data = windows_tls_node_->get_misc_data<tls_data_for_win>();
        if (misc_data.last_update_epoch_ms_ + THREAD_ALIVE_UPDATE_INTERVAL <= current_epoch_ms) {
            misc_data.last_update_epoch_ms_ = current_epoch_ms;
        }
#else
        (void)current_epoch_ms;
        if (!is_posix_log_thread_registered_) {
            register_posix_log_thread();
        }
#endif
    }

#if BQ_JAVA
    bq_forceinline jobjectArray get_buffer_array_for_current_log_thread(JNIEnv* env, const log_buffer_write_handle& handle)
    {
#if BQ_WIN
        jobjectArray& jobject_array = windows_tls_node_->get_misc_data<tls_data_for_win>().direct_buffer_array_;
#else
        jobjectArray& jobject_array = direct_buffer_array_;
#endif
        if (jobject_array) {
            return;
        }
        jobjectArray jobject_array = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
    }
#endif
}