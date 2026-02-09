/*
 * Copyright (C) 2025 Tencent.
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
 * \class bq::oversize_buffer
 *
 * Used when where data chunks are larger than
 * default buffer size of miso_buffer or siso_buffer.
 *
 * \author pippocao
 * \date 2025/07/26
 */

#include "bq_log/types/buffer/oversize_buffer.h"

namespace bq {
    oversize_buffer::oversize_buffer(uint32_t required_siso_buffer_size, const bq::string& mmap_file_abs_path, bool auto_create)
        : normal_buffer(static_cast<size_t>(calculate_size_of_memory(required_siso_buffer_size)), mmap_file_abs_path, auto_create)
    {
        auto mmap_result = get_mmap_result();
        switch (mmap_result) {
        case bq::create_memory_map_result::failed:
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), static_cast<uint8_t*>(data()) + static_cast<ptrdiff_t>(HEAD_SIZE), calculate_size_of_memory(required_siso_buffer_size) - static_cast<uint32_t>(HEAD_SIZE), false);
            init_with_memory();
            break;
        case bq::create_memory_map_result::use_existed:
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), static_cast<uint8_t*>(data()) + static_cast<ptrdiff_t>(HEAD_SIZE), calculate_size_of_memory(required_siso_buffer_size) - static_cast<uint32_t>(HEAD_SIZE), true);
            if (!try_recover_from_exist_memory_map()) {
                init_with_memory_map();
            }
            break;
        case bq::create_memory_map_result::new_created:
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), static_cast<uint8_t*>(data()) + static_cast<ptrdiff_t>(HEAD_SIZE), calculate_size_of_memory(required_siso_buffer_size) - static_cast<uint32_t>(HEAD_SIZE), true);
            init_with_memory_map();
            break;
        default:
            break;
        }
    }

    oversize_buffer::~oversize_buffer()
    {
        bq::object_destructor<bq::siso_ring_buffer>::destruct(&get_buffer());
    }

    uint32_t oversize_buffer::calculate_size_of_memory(uint32_t required_siso_buffer_size)
    {
        return bq::siso_ring_buffer::calculate_min_size_of_memory(required_siso_buffer_size) + static_cast<uint32_t>(HEAD_SIZE);
    }

    bool oversize_buffer::try_recover_from_exist_memory_map()
    {
        if (get_buffer().get_memory_map_buffer_state() != bq::memory_map_buffer_state::recover_from_memory_map) {
            return false;
        }
        return true;
    }

    void oversize_buffer::init_with_memory_map()
    {
        memset(data(), 0, HEAD_SIZE);
    }

    void oversize_buffer::init_with_memory()
    {
        memset(data(), 0, HEAD_SIZE);
    }
}
