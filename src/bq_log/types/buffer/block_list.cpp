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
 * \class bq::block_list
 *
 * A lock free singly linked list.
 * 
 * \author pippocao
 * \date 2024/12/06
 */
 
 #include "bq_log/types/buffer/block_list.h"

 namespace bq {

    block_node_head::block_node_head(void* buffer, size_t buffer_size, bool is_memory_recovery)
    {
        (void)padding_;
        if (!is_memory_recovery) {
            next_.index() = static_cast<uint16_t>(-1);
            next_.aba_mark() = 0;
            memset(misc_data_, 0, sizeof(misc_data_));
        }
        new ((void*)&get_buffer(), bq::enum_new_dummy::dummy) siso_ring_buffer(buffer, buffer_size, is_memory_recovery);
        size_t min_size = bq::roundup_pow_of_two(buffer_size) == buffer_size ? buffer_size : (bq::roundup_pow_of_two(buffer_size) >> 1);
        assert(reinterpret_cast<uintptr_t>(static_cast<void*>(buffer_)) % CACHE_LINE_SIZE == 0 && "siso_ring_buffer is not properly aligned!");
        assert((reinterpret_cast<uintptr_t>(static_cast<void*>(buffer_)) - reinterpret_cast<uintptr_t>(static_cast<void*>(&next_)) == CACHE_LINE_SIZE) && "siso_ring_buffer is not properly aligned!");
        assert(((size_t)(get_buffer().get_block_size() * get_buffer().get_total_blocks_count()) == min_size) && "siso_ring_buffer usable size is unexpected, please check calculation as memory alignment");
    }

    block_node_head::~block_node_head()
    {
        get_buffer().~siso_ring_buffer();
    }


    void block_node_head::alignment_assert()
    {
        assert((BQ_POD_RUNTIME_OFFSET_OF(block_node_head, next_) == 0) && "invalid alignment of bq::block_node_head");
        assert((BQ_POD_RUNTIME_OFFSET_OF(block_node_head, misc_data_) % 8 == 0) && "invalid alignment of bq::block_node_head");
        assert((BQ_POD_RUNTIME_OFFSET_OF(block_node_head, buffer_) == CACHE_LINE_SIZE) && "invalid alignment of bq::block_node_head");
    }

    void block_node_head::set_misc_data(const void* data_src, size_t data_size)
    {
        assert(data_size <= sizeof(misc_data_) && "data_size is too large");
        memcpy(misc_data_, data_src, data_size);
    }

    void block_list::reset(uint16_t max_blocks_count, const uint8_t* buffers_base_addr, size_t blocks_total_buffer_size)
    {
        head_.index() = static_cast<uint16_t>(-1);
        head_.aba_mark() = 0;
        ptrdiff_t offset = buffers_base_addr - reinterpret_cast<uint8_t*>(this);
        assert(offset <= UINT16_MAX && "block_list buffer offset too large");
        offset_ = static_cast<uint16_t>(offset);
        max_blocks_count_ = max_blocks_count;
        buffer_size_per_block_ = (size_t)(blocks_total_buffer_size / max_blocks_count);
        buffer_size_per_block_ -= (buffer_size_per_block_ % CACHE_LINE_SIZE);
        data_range_start_ = reinterpret_cast<const uint8_t*>(this) + static_cast<ptrdiff_t>(offset_);
        data_range_end_ = data_range_start_ + static_cast<ptrdiff_t>(buffer_size_per_block_ * max_blocks_count_);
        assert((buffer_size_per_block_ > static_cast<size_t>(block_node_head::get_buffer_data_offset())) && "too small of block buffer size");
    }

    bool block_list::try_recover_from_memory_map(uint16_t max_blocks_count, const uint8_t* buffers_base_addr, size_t blocks_total_buffer_size)
    {
        if (max_blocks_count_ != max_blocks_count) {
            return false;
        }
        const ptrdiff_t offset = buffers_base_addr - reinterpret_cast<uint8_t*>(this);
        assert(offset <= UINT16_MAX && "block_list buffer offset too large");
        if (offset_ != offset) {
            return false;
        }
        auto buffer_size_per_block = (size_t)(blocks_total_buffer_size / max_blocks_count);
        buffer_size_per_block -= (buffer_size_per_block % CACHE_LINE_SIZE);
        if (buffer_size_per_block_ != buffer_size_per_block) {
            return false;
        }
        assert((buffer_size_per_block_ > static_cast<size_t>(block_node_head::get_buffer_data_offset())) && "too small of block buffer size");
        const block_node_head::pointer_type* current_ptr = &head_;
        uint16_t current_blocks_count = 0;
        while (!current_ptr->is_empty() && current_blocks_count <= max_blocks_count_) {
            ++current_blocks_count;
            if (current_ptr->index() >= max_blocks_count_) {
                return false;
            }
            const block_node_head& next_block_head = get_block_head_by_index(current_ptr->index()); 
            current_ptr = &(next_block_head.next_);
        }
        if (current_blocks_count > max_blocks_count_) {
            return false;
        }
        data_range_start_ = reinterpret_cast<const uint8_t*>(this) + static_cast<ptrdiff_t>(offset_);
        data_range_end_ = data_range_start_ + static_cast<ptrdiff_t>(buffer_size_per_block_ * max_blocks_count_);
        return true;
    }

    void block_list::recovery_blocks()
    {
        block_node_head* current_node = first();
        while (current_node) {
            new (static_cast<void*>(current_node), bq::enum_new_dummy::dummy) block_node_head(
                reinterpret_cast<uint8_t*>(current_node) + block_node_head::get_buffer_data_offset(), buffer_size_per_block_ - (size_t)block_node_head::get_buffer_data_offset(), true);
            current_node = next(current_node);
        }
    }

    block_list::block_list(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size, bool is_memory_recovery)
    {
        assert(reinterpret_cast<uintptr_t>(buffers_base_addr) % CACHE_LINE_SIZE == 0 && "buffers_base_addr is not properly aligned!");
        if (!is_memory_recovery) {
            reset(max_blocks_count, buffers_base_addr, blocks_total_buffer_size);
            return;
        }
        if (!try_recover_from_memory_map(max_blocks_count, buffers_base_addr, blocks_total_buffer_size)) {
            reset(max_blocks_count, buffers_base_addr, blocks_total_buffer_size);
        }

    }

    block_list::~block_list()
    {
    }
 }
