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
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "bq_log/types/siso_ring_buffer.h"

namespace bq {

    siso_ring_buffer::siso_ring_buffer(void* buffer, size_t buffer_size, bool is_memory_mapped)
    {
        //make sure it's cache line size aligned
        cursors_ = (cursors_set*)(((uintptr_t)(char*)cursors_storage_ + (uintptr_t)cache_line_size - 1) & (~((uintptr_t)cache_line_size - 1)));
        assert(((uintptr_t)&cursors_->read_cursor_) % (uintptr_t)cache_line_size == 0);
        assert(((uintptr_t)&cursors_->write_cursor_) % (uintptr_t)cache_line_size == 0);
        assert(((uintptr_t)&cursors_->rt_writing_cursor_cache_) % (uintptr_t)cache_line_size == 0);
        assert(((uintptr_t)&cursors_->wt_reading_cursor_cache_) % (uintptr_t)cache_line_size == 0);
        assert((uintptr_t)cursors_ + (uintptr_t)sizeof(cursors_set) <= (uintptr_t)cursors_storage_ + (uintptr_t)sizeof(cursors_storage_));

        assert(((uintptr_t)buffer % sizeof(uint32_t)) == 0 && "input buffer of siso_ring_buffer should be 4 bytes aligned");
        assert(buffer_size >= sizeof(block) * 4 && "input buffer size of siso_ring_buffer must be at least 256 bytes");
        if (is_memory_mapped) {
            if (!try_recover_from_exist_memory_map(buffer, buffer_size)) {
                init_with_memory_map(buffer, buffer_size);
            }
        } else {
            init_with_memory(buffer, buffer_size);
        }

#if BQ_RING_BUFFER_DEBUG
        (void)padding_;
        total_write_bytes_ = 0;
        total_read_bytes_ = 0;
        for (int32_t i = 0; i < (int32_t)enum_buffer_result_code::result_code_count; ++i) {
            result_code_statistics_[i].store(0);
        }
#endif
    }

    siso_ring_buffer::~siso_ring_buffer()
    {
    }

    ring_buffer_write_handle siso_ring_buffer::alloc_write_chunk(uint32_t size)
    {
#if BQ_RING_BUFFER_DEBUG 
        if (check_thread_)
        {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (write_thread_id_ == empty_thread_id_) {
                write_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == write_thread_id_ && "only single thread writing is supported for siso_ring_buffer!");
            }
        }
#endif
        ring_buffer_write_handle handle;

        uint32_t size_required = size + sizeof(block::block_head);
        uint32_t need_block_count = (size_required + (cache_line_size - 1)) >> cache_line_size_log2;
        if (need_block_count > aligned_blocks_count_ || need_block_count == 0) {
#if BQ_RING_BUFFER_DEBUG
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
            handle.result = enum_buffer_result_code::err_alloc_size_invalid;
            return handle;
        }
        uint32_t current_write_cursor = *(uint32_t*)&cursors_->write_cursor_;
        block& new_block = cursor_to_block(current_write_cursor);
        uint32_t left_blocks_to_tail = aligned_blocks_count_ - (uint32_t)(&new_block - aligned_blocks_);
        if (left_blocks_to_tail < need_block_count) {
            size_required = size;
            need_block_count = (size_required + (cache_line_size - 1)) >> cache_line_size_log2;
            need_block_count += left_blocks_to_tail;
            handle.data_addr = (uint8_t*)aligned_blocks_;
        } else {
            handle.data_addr = (uint8_t*)&new_block + data_block_offset;
        }

        uint32_t left_space = cursors_->wt_reading_cursor_cache_ - current_write_cursor;
#if BQ_RING_BUFFER_DEBUG
        assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 1");
#endif
        if (left_space < need_block_count) {
            cursors_->wt_reading_cursor_cache_ = cursors_->read_cursor_.load(bq::platform::memory_order::acquire);
            left_space = cursors_->wt_reading_cursor_cache_ - current_write_cursor;
#if BQ_RING_BUFFER_DEBUG
            assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 2");
#endif
            if (left_space < need_block_count) {
                handle.approximate_used_blocks_count = aligned_blocks_count_ - left_space;
                // not enough space
#if BQ_RING_BUFFER_DEBUG
                ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                handle.result = enum_buffer_result_code::err_not_enough_space;
                return handle;
            }
        }
        new_block.block_head.block_num = need_block_count;
        new_block.block_head.data_size = size;

        handle.approximate_used_blocks_count = aligned_blocks_count_ - left_space;
        handle.result = enum_buffer_result_code::success;
#if BQ_RING_BUFFER_DEBUG
        ++result_code_statistics_[(int32_t)enum_buffer_result_code::success];
#endif
        return handle;
    }

    void siso_ring_buffer::commit_write_chunk(const ring_buffer_write_handle& handle)
    {
#if BQ_RING_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (write_thread_id_ == empty_thread_id_) {
                write_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == write_thread_id_ && "only single thread writing is supported for siso_ring_buffer!");
            }
        }
#endif
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
        block* block_ptr = (block*)(handle.data_addr - data_block_offset);
        if (handle.data_addr == (uint8_t*)aligned_blocks_ + data_block_offset) {  //maybe splited
            block_ptr = &cursor_to_block(*(uint32_t*)&cursors_->write_cursor_);
        } 
        cursors_->write_cursor_.store(*(uint32_t*)&cursors_->write_cursor_ + block_ptr->block_head.block_num, bq::platform::memory_order::release);
#if BQ_RING_BUFFER_DEBUG
        total_write_bytes_ += block_ptr->block_head.block_num * sizeof(block);
#endif
    }

    void siso_ring_buffer::begin_read()
    {
#if BQ_RING_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
            assert(cursors_->rt_reading_cursor_tmp_ == (uint32_t)-1 && "Please ensure that you call the functions in the following order: first begin_read() -> read() -> end_read().");
        }
#endif
        cursors_->rt_reading_cursor_tmp_ = *(uint32_t*)&cursors_->read_cursor_;
    }

    ring_buffer_read_handle siso_ring_buffer::read()
    {
#if BQ_RING_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            assert(cursors_->rt_reading_cursor_tmp_ != (uint32_t)-1 && "Please ensure that you call the functions in the following order: first begin_read() -> read() -> end_read().");
        }
#endif
        ring_buffer_read_handle handle;

        assert(cursors_->rt_writing_cursor_cache_ - cursors_->rt_reading_cursor_tmp_ <= aligned_blocks_count_);
        if (cursors_->rt_writing_cursor_cache_ - cursors_->rt_reading_cursor_tmp_ == 0) {
            cursors_->rt_writing_cursor_cache_ = cursors_->write_cursor_.load(bq::platform::memory_order::acquire);
        }
        if (cursors_->rt_writing_cursor_cache_ - cursors_->rt_reading_cursor_tmp_ == 0) {
            handle.result = enum_buffer_result_code::err_empty_ring_buffer;
#if BQ_RING_BUFFER_DEBUG
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_empty_ring_buffer];
#endif
            return handle;
        }
        block& current_block = cursor_to_block(cursors_->rt_reading_cursor_tmp_);
        auto block_count = current_block.block_head.block_num;
        assert(block_count > 0 && (cursors_->rt_writing_cursor_cache_ - cursors_->rt_reading_cursor_tmp_ >= block_count));
        handle.data_size = current_block.block_head.data_size;
        if ((uint8_t*)&current_block.block_head.data + current_block.block_head.data_size > (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) {
            //splited
            handle.data_addr = (uint8_t*)aligned_blocks_;
        } else {
            handle.data_addr = (uint8_t*)&(current_block.block_head.data);
        }
        handle.result = enum_buffer_result_code::success;
        cursors_->rt_reading_cursor_tmp_ += block_count;
#if BQ_RING_BUFFER_DEBUG
        total_read_bytes_ += block_count * sizeof(block);
#endif
        return handle;
    }

    void siso_ring_buffer::end_read()
    {
#if BQ_RING_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            assert(cursors_->rt_reading_cursor_tmp_ != (uint32_t)-1 && "Please ensure that you call the functions in the following order: first begin_read() -> read() -> end_read().");
        }
        cursors_->rt_reading_cursor_tmp_ = (uint32_t)-1;
#endif
        cursors_->read_cursor_.store(cursors_->rt_reading_cursor_tmp_, bq::platform::memory_order::release);
    }

    void siso_ring_buffer::init_with_memory_map(void* buffer, size_t buffer_size)
    {
        init_with_memory(buffer, buffer_size);
    }

    bool siso_ring_buffer::try_recover_from_exist_memory_map(void* buffer, size_t buffer_size)
    {
        // parse and verify
        mmap_head_ = (mmap_head*)buffer;
        uintptr_t data_start_addr = (uintptr_t)((uint8_t*)buffer + sizeof(mmap_head_));
        size_t align_unit = cache_line_size;
        uintptr_t aligned_addr = (data_start_addr + align_unit - 1) & (~(align_unit - 1));
        aligned_blocks_ = (block*)(aligned_addr);
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - aligned_addr) >> cache_line_size_log2;
        aligned_blocks_count_ = 1;
        assert(max_block_count > aligned_blocks_count_ && "siso_buffer init buffer size is not enough");
        while ((aligned_blocks_count_ << 1) <= max_block_count) {
            aligned_blocks_count_ <<= 1;
        }

        if (mmap_head_->aligned_blocks_count_cache_ != aligned_blocks_count_) {
            return false;
        }
        if (mmap_head_->write_cursor_cache_ - mmap_head_->read_cursor_cache_ > aligned_blocks_count_) {
            return false;
        }

        uint32_t current_cursor = mmap_head_->read_cursor_cache_;
        while (current_cursor < mmap_head_->write_cursor_cache_) {
            block& current_block = cursor_to_block(current_cursor);
            auto block_count = current_block.block_head.block_num;
            auto data_size = current_block.block_head.data_size;
            if (block_count == 0) {
                return false;
            }
            bool is_split = (current_cursor >> cache_line_size_log2) != ((current_cursor + block_count - 1) >> cache_line_size_log2);
            
            size_t max_data_size = 0;
            if (is_split) {
                max_data_size = ((current_cursor + block_count) & (aligned_blocks_count_ - 1)) * sizeof(block);
                assert(max_data_size >= sizeof(block));
            } else {
                max_data_size = block_count * sizeof(block) - data_block_offset;
            }
            size_t min_data_size = (max_data_size < sizeof(block)) ? 1 : max_data_size - sizeof(block) + 1;
            if (data_size < min_data_size || data_size > max_data_size) {
                return false;
            }
            current_cursor += block_count;
        }
        if (current_cursor != mmap_head_->write_cursor_cache_) {
            return false;
        }

        cursors_->write_cursor_.store(mmap_head_->write_cursor_cache_, platform::memory_order::release);
        cursors_->read_cursor_.store(mmap_head_->read_cursor_cache_, platform::memory_order::release);
        cursors_->rt_reading_cursor_tmp_ = (uint32_t)-1;
        cursors_->rt_writing_cursor_cache_ = mmap_head_->write_cursor_cache_;
        cursors_->wt_reading_cursor_cache_ = mmap_head_->read_cursor_cache_;
        return true;
    }

    void siso_ring_buffer::init_with_memory(void* buffer, size_t buffer_size)
    {
        mmap_head_ = (mmap_head*)buffer;
        uintptr_t data_start_addr = (uintptr_t)((uint8_t*)buffer + sizeof(mmap_head_));
        size_t align_unit = cache_line_size;
        uintptr_t aligned_addr = (data_start_addr + align_unit - 1) & (~(align_unit - 1));
        aligned_blocks_ = (block*)(aligned_addr);
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - aligned_addr) >> cache_line_size_log2;
        aligned_blocks_count_ = 1;
        assert(max_block_count > aligned_blocks_count_ && "siso_buffer init buffer size is not enough");
        while (aligned_blocks_count_ <= (max_block_count >> 1)) {
            aligned_blocks_count_ <<= 1;
        }
        cursors_->rt_reading_cursor_tmp_ = (uint32_t)-1;
        cursors_->rt_writing_cursor_cache_ = 0;
        cursors_->wt_reading_cursor_cache_ = 0;
        mmap_head_->read_cursor_cache_ = 0;
        mmap_head_->write_cursor_cache_ = 0;
        mmap_head_->aligned_blocks_count_cache_ = aligned_blocks_count_;
        cursors_->write_cursor_.store(0, platform::memory_order::release);
        cursors_->read_cursor_.store(0, platform::memory_order::release);
    }

}
