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
 * It's not base class of bq::group_list and bq::miso_ring_buffer. But a proxy to instead of virtual functions to accelerate calling performance.
 * so.
 *
 * \author pippocao
 * \date 2024/12/17
 */
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_types.h"
#include "bq_log/types/buffer/normal/miso_ring_buffer.h"
#include "bq_log/types/buffer/high_performance/group_list.h"

namespace bq {
    class log_buffer {
    public: 
        /// <summary>
        /// Initializes a new instance of the log_buffer class, configured according to the specified settings.
        /// </summary>
        /// <param name="config">Configuration settings for the log buffer, encapsulating all necessary details
        /// for buffer management and behavior.</param>
        log_buffer(const log_buffer_config& config);

        log_buffer(const log_buffer& rhs) = delete;

        /// <summary>
        /// 
        /// </summary>
        ~log_buffer();


        bq_forceinline log_buffer_write_handle alloc_write_chunk(uint32_t size)
        {
            switch (type_) {
            case log_buffer_type::normal:
                return ((miso_ring_buffer*)buffer_impl_)->alloc_write_chunk(size);
                break;
            case log_buffer_type::high_performance:
                return ((group_list*)buffer_impl_)->alloc_write_chunk(size);
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                return log_buffer_write_handle();
                break;
            }
        }

        bq_forceinline void commit_write_chunk(const log_buffer_write_handle& handle)
        {
            switch (type_) {
            case log_buffer_type::normal:
                ((miso_ring_buffer*)buffer_impl_)->commit_write_chunk(handle);
                break;
            case log_buffer_type::high_performance:
                ((group_list*)buffer_impl_)->commit_write_chunk(handle);
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                break;
            }
        }

        bq_forceinline void begin_read()
        {
            switch (type_) {
            case log_buffer_type::normal:
                ((miso_ring_buffer*)buffer_impl_)->begin_read();
                break;
            case log_buffer_type::high_performance:
                ((group_list*)buffer_impl_)->begin_read();
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                break;
            }
        }

        bq_forceinline log_buffer_read_handle read()
        {
            switch (type_) {
            case log_buffer_type::normal:
                return ((miso_ring_buffer*)buffer_impl_)->read();
                break;
            case log_buffer_type::high_performance:
                return ((group_list*)buffer_impl_)->read();
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                return log_buffer_read_handle();
                break;
            }
        }

        bq_forceinline void end_read()
        {
            switch (type_) {
            case log_buffer_type::normal:
                ((miso_ring_buffer*)buffer_impl_)->end_read();
                break;
            case log_buffer_type::high_performance:
                ((group_list*)buffer_impl_)->end_read();
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                break;
            }
        }

        bq_forceinline void set_thread_check_enable(bool in_enable)
        {
            switch (type_) {
            case log_buffer_type::normal:
                ((miso_ring_buffer*)buffer_impl_)->set_thread_check_enable(in_enable);
                break;
            case log_buffer_type::high_performance:
                ((group_list*)buffer_impl_)->set_thread_check_enable(in_enable);
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                break;
            }
        }

#if BQ_JAVA
        bq_forceinline java_buffer_info get_java_buffer_info(JavaVM* jvm, const log_buffer_write_handle& handle)
        {
            switch (type_) {
            case log_buffer_type::normal:
                return ((miso_ring_buffer*)buffer_impl_)->get_java_buffer_info(jvm, handle);
                break;
            case log_buffer_type::high_performance:
                return ((group_list*)buffer_impl_)->get_java_buffer_info(jvm, handle);
                break;
            default:
                assert(false && "unknown log_buffer_type type");
                break;
            }
        }
#endif

    private:
        log_buffer_type type_;
        void* buffer_impl_ = nullptr;
    };
}