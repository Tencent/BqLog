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
 * \class bq::ring_buffer_base
 * It's not base class of bq::siso_ring_buffer and bq::miso_ring_buffer. But a simple code file with some data type definitions
 * and a proxy to instead of virtual functions to accelerate calling performance.
 * so, please don't include this header file, just include "siso_ring_buffer.h" or "miso_ring_buffer.h" directly.
 *
 * \author pippocao
 * \date 2024/11/19
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"

#ifndef NDEBUG
#define BQ_RING_BUFFER_DEBUG 1
#else
#define BQ_RING_BUFFER_DEBUG 0
#endif

namespace bq {
    struct ring_buffer_handle_base {
        enum_buffer_result_code result = enum_buffer_result_code::err_alloc_size_invalid;
        uint8_t* data_addr = nullptr;
    };

    struct ring_buffer_write_handle : public ring_buffer_handle_base {
        uint32_t approximate_used_blocks_count = 0; // just approximate because of multi-thread
    };

    struct ring_buffer_read_handle : public ring_buffer_handle_base {
        uint32_t data_size;
    };
}
