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
 /*
 * \author pippocao
 * \date 2025/1/24
 */
#include "bq_log/types/buffer/log_buffer_tls.h"
#include "bq_log/types/buffer/group_list.h"
#if BQ_WIN
#include <Windows.h>
#elif BQ_POSIX
#include <pthread.h>
#endif

namespace bq {
#if BQ_WIN
    BQ_TLS block_node_head* windows_tls_node_ = nullptr;
    static group_list tls_group_list_({ log_buffer_type::high_performance, "", bq::array<string>(), 1, false, false }, 64);

    void create_windows_tls_node()
    {
        windows_tls_node_ = tls_group_list_.alloc_new_block();
        auto& misc_data = windows_tls_node_->get_misc_data<tls_data_for_win>();
        misc_data.thread_id_ = bq::platform::thread::get_current_thread_id();
        misc_data.last_update_epoch_ms_ = 0;
        misc_data.thread_handle_ = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, misc_data.thread_id_);
        if (!misc_data.thread_handle_) {
            // compatible to windows earlier than Windows Vista.
            misc_data.thread_handle_ = OpenThread(THREAD_QUERY_INFORMATION, FALSE, misc_data.thread_id_);
        }
#if BQ_JAVA
        misc_data.direct_buffer_array_ = NULL;
        misc_data.offset_buffer_ = 0;
#endif
    }
#elif BQ_POSIX
    BQ_TLS bool is_posix_log_thread_registered_ = false;
#if BQ_JAVA
    BQ_TLS jobjectArray direct_buffer_array_ = NULL;
    BQ_TLS uint64_t offset_buffer_ = 0;
#endif

    static pthread_key_t log_buffer_pthread_key_;
    static void on_log_thread_exist(void*)
    {
        
    }
    struct log_pthread_initer {
        hp_pthread_initer()
        {
            int32_t result = pthread_key_create(&log_buffer_pthread_key_, &on_log_thread_exist);
            if (0 != result) {
                bq::util::log_device_console(bq::log_level::fatal, "failed to call pthread_key_create, err value:%d", result);
            }
        }
    };
    static log_pthread_initer pthread_initer_;

    void register_posix_log_thread()
    {
        pthread_setspecific(log_buffer_pthread_key_, (void*)&log_buffer_pthread_key_);
        is_posix_log_thread_registered_ = true;
    }
#endif


}
