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
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {

    siso_ring_buffer::siso_ring_buffer(void* buffer, size_t buffer_size, bool is_memory_mapped)
        : is_memory_mapped_(is_memory_mapped)
    {
        //make sure it's cache line size aligned
        cursors_ = (cursors_set*)(((uintptr_t)(char*)cursors_storage_ + (uintptr_t)CACHE_LINE_SIZE - 1) & (~((uintptr_t)CACHE_LINE_SIZE - 1)));
        assert(((uintptr_t)&BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t)) % (uintptr_t)CACHE_LINE_SIZE == 0);
        assert(((uintptr_t)&BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t)) % (uintptr_t)CACHE_LINE_SIZE == 0);
        assert(((uintptr_t)&cursors_->rt_writing_cursor_cache_) % (uintptr_t)CACHE_LINE_SIZE == 0);
        assert(((uintptr_t)&cursors_->wt_reading_cursor_cache_) % (uintptr_t)CACHE_LINE_SIZE == 0);
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

#if BQ_LOG_BUFFER_DEBUG
        (void)padding_;
        total_write_bytes_ = 0;
        total_read_bytes_ = 0;
        for (int32_t i = 0; i < (int32_t)enum_buffer_result_code::result_code_count; ++i) {
            result_code_statistics_[i].store_release(0);
        }
#endif
        assert((BQ_POD_RUNTIME_OFFSET_OF(block::chunk_head_def, data) % 8 == 0) && "invalid chunk_head size, it must be a multiple of 8 to ensure the `data` is 8 bytes aligned");
        
    }

    siso_ring_buffer::~siso_ring_buffer()
    {
    }

    log_buffer_write_handle siso_ring_buffer::alloc_write_chunk(uint32_t size)
    {
#if BQ_LOG_BUFFER_DEBUG 
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
        log_buffer_write_handle handle;

        uint32_t size_required = size + (uint32_t)data_block_offset;
        uint32_t need_block_count = (size_required + (CACHE_LINE_SIZE - 1)) >> CACHE_LINE_SIZE_LOG2;
        if (need_block_count > aligned_blocks_count_ || need_block_count == 0) {
#if BQ_LOG_BUFFER_DEBUG
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
            handle.result = enum_buffer_result_code::err_alloc_size_invalid;
            return handle;
        }
        uint32_t current_write_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).load_raw();
        block& new_block = cursor_to_block(current_write_cursor);
        uint32_t left_blocks_to_tail = static_cast<uint32_t>(aligned_blocks_count_ - (uint32_t)(&new_block - aligned_blocks_));
        if (left_blocks_to_tail < need_block_count) {
            size_required = size;
            need_block_count = (size_required + (CACHE_LINE_SIZE - 1)) >> CACHE_LINE_SIZE_LOG2;
            need_block_count += left_blocks_to_tail;
            handle.data_addr = (uint8_t*)aligned_blocks_;
        } else {
            handle.data_addr = (uint8_t*)&new_block + data_block_offset;
        }

        uint32_t left_space = static_cast<uint32_t>(cursors_->wt_reading_cursor_cache_ + aligned_blocks_count_ - current_write_cursor);
#if BQ_LOG_BUFFER_DEBUG
        assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 1");
#endif
        if (left_space < need_block_count) {
            cursors_->wt_reading_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_acquire();
            left_space = static_cast<uint32_t>(cursors_->wt_reading_cursor_cache_ + aligned_blocks_count_ - current_write_cursor);
#if BQ_LOG_BUFFER_DEBUG
            assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 2");
#endif
            if (left_space < need_block_count) {
                handle.low_space_flag = true;
                // not enough space
#if BQ_LOG_BUFFER_DEBUG
                ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                handle.result = enum_buffer_result_code::err_not_enough_space;
                return handle;
            }
        }
        new_block.chunk_head.block_num = need_block_count;
        new_block.chunk_head.data_size = size;

        handle.low_space_flag = (left_space <= (aligned_blocks_count_ >> 1));
        handle.result = enum_buffer_result_code::success;
#if BQ_LOG_BUFFER_DEBUG
        ++result_code_statistics_[(int32_t)enum_buffer_result_code::success];
#endif
        return handle;
    }

    void siso_ring_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (write_thread_id_ == empty_thread_id_) {
                write_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == write_thread_id_ && "only single thread writing is supported for siso_ring_buffer!");
            }
        }
#endif
        uint32_t current_write_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).load_raw();
        block* block_ptr = (handle.data_addr == (uint8_t*)aligned_blocks_) 
                ? &cursor_to_block(current_write_cursor) // splited
                : (block*)(handle.data_addr - data_block_offset);

        head_->write_cursor_cache_ = current_write_cursor + block_ptr->chunk_head.block_num;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(head_->write_cursor_cache_);
#if BQ_LOG_BUFFER_DEBUG
        total_write_bytes_ += block_ptr->chunk_head.block_num * sizeof(block);
#endif
    }

    log_buffer_read_handle siso_ring_buffer::read_chunk()
    {
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
        }
#endif 
        uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
#if BQ_LOG_BUFFER_DEBUG
        assert(static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
        log_buffer_read_handle handle;
        if (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
            cursors_->rt_writing_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).load_acquire();
#if BQ_LOG_BUFFER_DEBUG
            assert(static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
        }
        if (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
            handle.result = enum_buffer_result_code::err_empty_log_buffer;
#if BQ_LOG_BUFFER_DEBUG
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_empty_log_buffer];
#endif
            return handle;
        }
        block& current_block = cursor_to_block(current_read_cursor);
#if BQ_LOG_BUFFER_DEBUG
        auto block_count = current_block.chunk_head.block_num;
        assert(block_count > 0 && (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) >= block_count));
#endif
        handle.data_size = current_block.chunk_head.data_size;
        if ((uint8_t*)&current_block.chunk_head.data + current_block.chunk_head.data_size > (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) {
            //splited
            handle.data_addr = (uint8_t*)aligned_blocks_;
        } else {
            handle.data_addr = (uint8_t*)&(current_block.chunk_head.data);
        }
        handle.result = enum_buffer_result_code::success;
#if BQ_LOG_BUFFER_DEBUG
        total_read_bytes_ += block_count * sizeof(block);
#endif
        return handle;
    }

    void siso_ring_buffer::return_read_trunk(const log_buffer_read_handle& handle)
    {
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
        }
        assert((handle.data_addr <= (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) && (handle.data_addr >= (uint8_t*)aligned_blocks_)
            && "please don't return a read handle not belongs to this siso_ring_buffer");
#endif
        uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
        block* block_ptr = (handle.data_addr == (uint8_t*)aligned_blocks_)
            ? &cursor_to_block(current_read_cursor) // splited
            : (block*)(handle.data_addr - data_block_offset);

#if BQ_LOG_BUFFER_DEBUG
        assert(block_ptr->chunk_head.block_num <= aligned_blocks_count_);
#endif

        head_->read_cursor_cache_ = current_read_cursor + block_ptr->chunk_head.block_num;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(head_->read_cursor_cache_);
    }

    void siso_ring_buffer::data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data)
    {
#if BQ_LOG_BUFFER_DEBUG 
        if (check_thread_)
        {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
        }
#endif 
        uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
#if BQ_LOG_BUFFER_DEBUG
        assert(static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
        while (true) {
            if (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
                cursors_->rt_writing_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).load_acquire();
#if BQ_LOG_BUFFER_DEBUG
                assert(static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
            }
            if (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
                break;
            }
            block& current_block = cursor_to_block(current_read_cursor);
            auto block_count = current_block.chunk_head.block_num;
#if BQ_LOG_BUFFER_DEBUG
            assert(block_count > 0 && (static_cast<uint32_t>(cursors_->rt_writing_cursor_cache_ - current_read_cursor) >= block_count));
#endif
            if ((uint8_t*)&current_block.chunk_head.data + current_block.chunk_head.data_size > (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) {
                // splited
                in_callback((uint8_t*)aligned_blocks_, current_block.chunk_head.data_size, in_user_data);
            } else {
                in_callback((uint8_t*)&(current_block.chunk_head.data), current_block.chunk_head.data_size, in_user_data);
            }
            current_read_cursor += block_count;
        }
    }

    uint32_t siso_ring_buffer::calculate_min_size_of_memory(uint32_t expected_buffer_size)
    {
        expected_buffer_size = bq::roundup_pow_of_two(expected_buffer_size);
        return expected_buffer_size + (uint32_t)(sizeof(head) + CACHE_LINE_SIZE);
    }

    void siso_ring_buffer::init_with_memory_map(void* buffer, size_t buffer_size)
    {
        init_with_memory(buffer, buffer_size);
    }

    bool siso_ring_buffer::try_recover_from_exist_memory_map(void* buffer, size_t buffer_size)
    {
        // parse and verify
        head_ = (head*)buffer;
        uintptr_t data_start_addr = (uintptr_t)((uint8_t*)buffer + sizeof(head_));
        size_t align_unit = CACHE_LINE_SIZE;
        uintptr_t aligned_addr = (data_start_addr + align_unit - 1) & (~(align_unit - 1));
        aligned_blocks_ = (block*)(aligned_addr);
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - aligned_addr) >> CACHE_LINE_SIZE_LOG2;
        assert(max_block_count > 1 && "siso_buffer init buffer size is not enough");
        aligned_blocks_count_ = bq::roundup_pow_of_two((uint32_t)max_block_count);
        if (aligned_blocks_count_ > max_block_count) {
            aligned_blocks_count_ >>= 1;
        }

        if (head_->aligned_blocks_count_cache_ != aligned_blocks_count_) {
            return false;
        }
        if (static_cast<uint32_t>(head_->write_cursor_cache_ - head_->read_cursor_cache_) > aligned_blocks_count_) {
            return false;
        }

        uint32_t current_cursor = head_->read_cursor_cache_;
        while (current_cursor < head_->write_cursor_cache_) {
            block& current_block = cursor_to_block(current_cursor);
            auto block_count = current_block.chunk_head.block_num;
            auto data_size = current_block.chunk_head.data_size;
            if (block_count == 0) {
                return false;
            }
            bool is_split = (current_cursor >> CACHE_LINE_SIZE_LOG2) != ((current_cursor + block_count - 1) >> CACHE_LINE_SIZE_LOG2);
            
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
        if (current_cursor != head_->write_cursor_cache_) {
            return false;
        }

        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(head_->write_cursor_cache_);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(head_->read_cursor_cache_);
        cursors_->rt_writing_cursor_cache_ = head_->write_cursor_cache_;
        cursors_->wt_reading_cursor_cache_ = head_->read_cursor_cache_;
        return true;
    }

    void siso_ring_buffer::init_with_memory(void* buffer, size_t buffer_size)
    {
        head_ = (head*)buffer;
        uintptr_t data_start_addr = (uintptr_t)((uint8_t*)buffer + sizeof(head_));
        size_t align_unit = CACHE_LINE_SIZE;
        uintptr_t aligned_addr = (data_start_addr + align_unit - 1) & (~(align_unit - 1));
        aligned_blocks_ = (block*)(aligned_addr);
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - aligned_addr) >> CACHE_LINE_SIZE_LOG2;
        aligned_blocks_count_ = 1;
        assert(max_block_count > aligned_blocks_count_ && "siso_buffer init buffer size is not enough");
        while (aligned_blocks_count_ <= (max_block_count >> 1)) {
            aligned_blocks_count_ <<= 1;
        }
        cursors_->rt_writing_cursor_cache_ = 0;
        cursors_->wt_reading_cursor_cache_ = 0;
        head_->read_cursor_cache_ = 0;
        head_->write_cursor_cache_ = 0;
        head_->aligned_blocks_count_cache_ = aligned_blocks_count_;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(0);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(0);
    }

}
