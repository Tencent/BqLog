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

#include "bq_log/types/buffer/siso_ring_buffer.h"
#include "bq_log/global/log_vars.h"

namespace bq {

    siso_ring_buffer::siso_ring_buffer(void* buffer, size_t buffer_size, bool is_memory_recovery)
        : is_memory_recovery_(is_memory_recovery)
    {
        // make sure it's cache line size aligned
        assert((uintptr_t)buffer % BQ_CACHE_LINE_SIZE == 0 && "input buffer of siso_ring_buffer should be cache line size aligned");
        assert(buffer_size >= sizeof(block) * 4 && "input buffer size of siso_ring_buffer must be at least 256 bytes");
        if (is_memory_recovery) {
            if (!try_recover_from_exist_memory_map(buffer, buffer_size)) {
                init_with_memory_map(buffer, buffer_size);
            }
        } else {
            init_with_memory(buffer, buffer_size);
        }
        assert((uintptr_t)&head_->aligned_blocks_count_cache_ % BQ_CACHE_LINE_SIZE == 0 && "head_->aligned_blocks_count_cache_ of siso_ring_buffer should be cache line size aligned");
        assert((uintptr_t)&head_->wt_reading_cursor_cache_ % BQ_CACHE_LINE_SIZE == 0 && "head_->wt_reading_cursor_cache_ of siso_ring_buffer should be cache line size aligned");
        assert((uintptr_t)&head_->reading_cursor_ % BQ_CACHE_LINE_SIZE == 0 && "head_->reading_cursor_ of siso_ring_buffer should be cache line size aligned");
        assert((uintptr_t)&head_->writing_cursor_ % BQ_CACHE_LINE_SIZE == 0 && "head_->writing_cursor_ of siso_ring_buffer should be cache line size aligned");
        assert((uintptr_t)aligned_blocks_ % BQ_CACHE_LINE_SIZE == 0 && "aligned_blocks_ of siso_ring_buffer should be cache line size aligned");
#if defined(BQ_LOG_BUFFER_DEBUG)
        (void)padding_;
        total_write_bytes_ = 0;
        total_read_bytes_ = 0;
        for (int32_t i = 0; i < (int32_t)enum_buffer_result_code::result_code_count; ++i) {
            result_code_statistics_[i].store_release(0);
        }
#endif
        assert(((size_t)data_block_offset % BLOCK_SIZE == 0) && "invalid chunk_head size, it must be a multiple of 8 to ensure the `data` is 8 bytes aligned");
    }

    siso_ring_buffer::~siso_ring_buffer()
    {
    }

    void siso_ring_buffer::renew()
    {
        init_cursors();
    }

    log_buffer_write_handle siso_ring_buffer::alloc_write_chunk(uint32_t size)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (write_thread_id_ == empty_thread_id_) {
                write_thread_id_ = current_thread_id;
            } else if (current_thread_id != write_thread_id_) {
                bq::util::log_device_console(bq::log_level::error, "only single thread writing is supported for siso_ring_buffer! expected:%" PRIu64 ", but:%" PRIu64 "", write_thread_id_, current_thread_id);
                assert(false && "only single thread writing is supported for siso_ring_buffer!");
            }
        }
#endif
        log_buffer_write_handle handle;

        uint32_t size_required = size + (uint32_t)data_block_offset;
        uint32_t need_block_count = static_cast<uint32_t>((size_required + (BLOCK_SIZE - 1)) >> BLOCK_SIZE_LOG2);
        block& new_block = cursor_to_block(head_->wt_writing_cursor_cache_);
        uint32_t left_blocks_to_tail = static_cast<uint32_t>(aligned_blocks_count_ - (uint32_t)(&new_block - aligned_blocks_));
        BQ_UNLIKELY_IF(need_block_count > aligned_blocks_count_
            || need_block_count == 0
            || (need_block_count > left_blocks_to_tail && (need_block_count + left_blocks_to_tail - 1) > aligned_blocks_count_))
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
            handle.result = enum_buffer_result_code::err_alloc_size_invalid;
            return handle;
        }
        if (left_blocks_to_tail < need_block_count) {
            size_required = size;
            need_block_count = static_cast<uint32_t>((size_required + (BLOCK_SIZE - 1)) >> BLOCK_SIZE_LOG2);
            need_block_count += left_blocks_to_tail;
            handle.data_addr = (uint8_t*)aligned_blocks_;
        } else {
            handle.data_addr = new_block.to_chunk_head().data;
        }

        uint32_t left_space = static_cast<uint32_t>(head_->wt_reading_cursor_cache_ + aligned_blocks_count_ - head_->wt_writing_cursor_cache_);
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 1");
#endif
        if (left_space < need_block_count) {
            head_->wt_reading_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).load_acquire();
            left_space = static_cast<uint32_t>(head_->wt_reading_cursor_cache_ + aligned_blocks_count_ - head_->wt_writing_cursor_cache_);
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(left_space <= aligned_blocks_count_ && "siso ring_buffer wt_reading_cursor_cache_ error 2");
#endif
            if (left_space < need_block_count) {
                // not enough space
#if defined(BQ_LOG_BUFFER_DEBUG)
                ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                handle.result = enum_buffer_result_code::err_not_enough_space;
                return handle;
            }
        }
        new_block.to_chunk_head().block_num = need_block_count;
        new_block.to_chunk_head().data_size = size;

        handle.low_space_flag = ((aligned_blocks_count_ - left_space) << 1) >= aligned_blocks_count_;
        handle.result = enum_buffer_result_code::success;
#if defined(BQ_LOG_BUFFER_DEBUG)
        ++result_code_statistics_[(int32_t)enum_buffer_result_code::success];
#endif
        return handle;
    }

    void siso_ring_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (write_thread_id_ == empty_thread_id_) {
                write_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == write_thread_id_ && "only single thread writing is supported for siso_ring_buffer!");
            }
        }
#endif
        block* block_ptr = (handle.data_addr == (uint8_t*)aligned_blocks_)
            ? &cursor_to_block(head_->wt_writing_cursor_cache_) // splited
            : (block*)(handle.data_addr - data_block_offset);

        head_->wt_writing_cursor_cache_ += block_ptr->to_chunk_head().block_num;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).store_release(head_->wt_writing_cursor_cache_);
#if defined(BQ_LOG_BUFFER_DEBUG)
        total_write_bytes_ += block_ptr->to_chunk_head().block_num * sizeof(block);
#endif
    }

    log_buffer_read_handle siso_ring_buffer::read_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(!is_read_chunk_waiting_for_return_ && "You cant read a chunk before you returning last chunk back");
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
        }
#endif
#if defined(BQ_LOG_BUFFER_DEBUG)
        is_read_chunk_waiting_for_return_ = true;
        assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) <= aligned_blocks_count_);
#endif
        log_buffer_read_handle handle;
        if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) == 0) {
            head_->rt_writing_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).load_acquire();
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) <= aligned_blocks_count_);
#endif
        }
        if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) == 0) {
            handle.result = enum_buffer_result_code::err_empty_log_buffer;
#if defined(BQ_LOG_BUFFER_DEBUG)
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_empty_log_buffer];
#endif
            return handle;
        }
        block& current_block = cursor_to_block(head_->rt_reading_cursor_cache_);
#if defined(BQ_LOG_BUFFER_DEBUG)
        auto block_count = current_block.to_chunk_head().block_num;
        assert(block_count > 0 && (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) >= block_count));
#endif
        handle.data_size = current_block.to_chunk_head().data_size;
        handle.data_addr = get_block_data_addr(current_block);
        handle.result = enum_buffer_result_code::success;
#if defined(BQ_LOG_BUFFER_DEBUG)
        total_read_bytes_ += block_count * sizeof(block);
#endif
        return handle;
    }

    log_buffer_read_handle siso_ring_buffer::read_an_empty_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(!is_read_chunk_waiting_for_return_ && "You cant read a chunk before you returning last chunk back");
        if (is_thread_check_enable()) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
        }
        is_read_chunk_waiting_for_return_ = true;
#endif
        log_buffer_read_handle handle;
        handle.result = enum_buffer_result_code::err_empty_log_buffer;
        return handle;
    }

    void siso_ring_buffer::discard_read_chunk(log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(is_read_chunk_waiting_for_return_ && "You cant discard a chunk has not been read yet");
        is_read_chunk_waiting_for_return_ = false;
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
#endif
        handle.result = enum_buffer_result_code::err_empty_log_buffer;
    }

    void siso_ring_buffer::return_read_chunk(const log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(is_read_chunk_waiting_for_return_ && "You cant return a chunk has not been read yet");
        is_read_chunk_waiting_for_return_ = false;

        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
        }
#endif
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
        block* block_ptr = (handle.data_addr == (uint8_t*)aligned_blocks_)
            ? &cursor_to_block(head_->rt_reading_cursor_cache_) // split
            : (block*)(handle.data_addr - data_block_offset);

#if defined(BQ_LOG_BUFFER_DEBUG)
        assert((handle.data_addr <= (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) && (handle.data_addr >= (uint8_t*)aligned_blocks_)
            && "please don't return a read handle not belongs to this siso_ring_buffer");
        assert(block_ptr->to_chunk_head().block_num <= aligned_blocks_count_);
#endif

        head_->rt_reading_cursor_cache_ += block_ptr->to_chunk_head().block_num;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).store_release(head_->rt_reading_cursor_cache_);
    }

    siso_ring_buffer::siso_buffer_batch_read_handle siso_ring_buffer::batch_read()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(!is_read_chunk_waiting_for_return_ && "You cant read chunks before you returning last chunk back");
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
        }
        is_read_chunk_waiting_for_return_ = true;
        assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) <= aligned_blocks_count_);
#endif
        siso_buffer_batch_read_handle handle;
        if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) == 0) {
            head_->rt_writing_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).load_acquire();
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) <= aligned_blocks_count_);
#endif
        }
        handle.parent_ = this;
        handle.start_cursor_ = head_->rt_reading_cursor_cache_;
        handle.end_cursor_ = head_->rt_writing_cursor_cache_;
        handle.current_cursor_ = handle.start_cursor_;
        if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) == 0) {
            handle.result = enum_buffer_result_code::err_empty_log_buffer;
#if defined(BQ_LOG_BUFFER_DEBUG)
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_empty_log_buffer];
#endif
            return handle;
        }
#if defined(BQ_LOG_BUFFER_DEBUG)
        block& current_block = cursor_to_block(head_->rt_reading_cursor_cache_);
        auto block_count = current_block.to_chunk_head().block_num;
        assert(block_count > 0 && (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) >= block_count));
        total_read_bytes_ += (handle.end_cursor_ - handle.start_cursor_) * sizeof(block);
#endif
        handle.result = enum_buffer_result_code::success;
        return handle;
    }

    void siso_ring_buffer::return_batch_read_chunks(const siso_ring_buffer::siso_buffer_batch_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(is_read_chunk_waiting_for_return_ && "You cant return a chunk has not been read yet");
        is_read_chunk_waiting_for_return_ = false;
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
        }
#endif
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(handle.end_cursor_ == handle.current_cursor_ && "siso_buffer_batch_read_handle::return_batch_read_chunks called with not finished reading handle");
        assert(handle.start_cursor_ == BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).load_relaxed() && "invalid batch handle start cursor");
        assert(handle.end_cursor_ == head_->rt_reading_cursor_cache_ && "invalid batch handle end cursor");
        assert(handle.end_cursor_ == head_->rt_writing_cursor_cache_ && "invalid batch handle end cursor");
#endif
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).store_release(head_->rt_reading_cursor_cache_);
    }

    void siso_ring_buffer::data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (is_thread_check_enable()) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for siso_ring_buffer!");
            }
        }
#endif
        uint32_t current_read_cursor = head_->rt_reading_cursor_cache_;
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
        while (true) {
            if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
                head_->rt_writing_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).load_acquire();
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - current_read_cursor) <= aligned_blocks_count_);
#endif
            }
            if (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - current_read_cursor) == 0) {
                break;
            }
            block& current_block = cursor_to_block(current_read_cursor);
            auto block_count = current_block.to_chunk_head().block_num;
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(block_count > 0 && (static_cast<uint32_t>(head_->rt_writing_cursor_cache_ - current_read_cursor) >= block_count));
#endif
            uint8_t* data_addr = get_block_data_addr(current_block);
            in_callback(data_addr, current_block.to_chunk_head().data_size, in_user_data);
            current_read_cursor += block_count;
        }
    }

    uint32_t siso_ring_buffer::calculate_min_size_of_memory(uint32_t expected_buffer_size)
    {
        expected_buffer_size = bq::roundup_pow_of_two(expected_buffer_size);
        return expected_buffer_size + (uint32_t)(sizeof(head) + BQ_CACHE_LINE_SIZE);
    }

    bool siso_ring_buffer::is_thread_check_enable() const
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        return check_thread_ && log_global_vars::get().is_thread_check_enabled();
#else
        return false;
#endif
    }


    uint32_t siso_ring_buffer::get_max_alloc_size() const
    {
        return aligned_blocks_count_* BLOCK_SIZE - static_cast<uint32_t>(data_block_offset);
    }

    void siso_ring_buffer::init_with_memory_map(void* buffer, size_t buffer_size)
    {
        init_with_memory(buffer, buffer_size);
        mmap_buffer_state_ = memory_map_buffer_state::init_with_memmap;
    }

    bool siso_ring_buffer::try_recover_from_exist_memory_map(void* buffer, size_t buffer_size)
    {
        // parse and verify
        head_ = (head*)buffer;
        aligned_blocks_ = (block*)((uint8_t*)buffer + sizeof(head));
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - (uintptr_t)aligned_blocks_) >> BLOCK_SIZE_LOG2;
        aligned_blocks_count_ = 1;
        assert(max_block_count > aligned_blocks_count_ && "siso_buffer init buffer size is not enough");
        while (aligned_blocks_count_ <= (max_block_count >> 1)) {
            aligned_blocks_count_ <<= 1;
        }

        if (head_->aligned_blocks_count_cache_ != aligned_blocks_count_) {
            return false;
        }
        if (static_cast<uint32_t>(head_->wt_writing_cursor_cache_ - head_->rt_reading_cursor_cache_) > aligned_blocks_count_) {
            return false;
        }

        uint32_t current_cursor = head_->rt_reading_cursor_cache_;
        while (current_cursor < head_->wt_writing_cursor_cache_) {
            block& current_block = cursor_to_block(current_cursor);
            auto block_count = current_block.to_chunk_head().block_num;
            auto data_size = current_block.to_chunk_head().data_size;
            if (block_count == 0) {
                return false;
            }
            bool is_split = ((uint8_t*)&current_block.to_chunk_head().data + current_block.to_chunk_head().data_size > (uint8_t*)(aligned_blocks_ + aligned_blocks_count_));

            uint32_t expected_block_count = 0;

            if (is_split) {
                uint32_t left_blocks_to_tail = static_cast<uint32_t>(aligned_blocks_count_ - (uint32_t)(&current_block - aligned_blocks_));
                expected_block_count = left_blocks_to_tail + static_cast<uint32_t>((data_size + (BLOCK_SIZE - 1)) >> BLOCK_SIZE_LOG2);
            } else {
                expected_block_count = static_cast<uint32_t>((data_size + (uint32_t)data_block_offset + (BLOCK_SIZE - 1)) >> BLOCK_SIZE_LOG2);
            }
            if (expected_block_count != block_count) {
                return false;
            }
            current_cursor += block_count;
        }
        if (current_cursor != head_->wt_writing_cursor_cache_) {
            return false;
        }
        head_->rt_writing_cursor_cache_ = head_->wt_writing_cursor_cache_;
        head_->wt_reading_cursor_cache_ = head_->rt_reading_cursor_cache_;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).store_release(head_->rt_reading_cursor_cache_);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).store_release(head_->wt_writing_cursor_cache_);
        mmap_buffer_state_ = memory_map_buffer_state::recover_from_memory_map;
        return true;
    }

    void siso_ring_buffer::init_with_memory(void* buffer, size_t buffer_size)
    {
        head_ = (head*)buffer;
        aligned_blocks_ = (block*)((uint8_t*)buffer + sizeof(head));
        size_t max_block_count = (size_t)((uintptr_t)buffer + (uintptr_t)buffer_size - (uintptr_t)aligned_blocks_) >> BLOCK_SIZE_LOG2;
        aligned_blocks_count_ = 1;
        assert(max_block_count > aligned_blocks_count_ && "siso_buffer init buffer size is not enough");
        while (aligned_blocks_count_ <= (max_block_count >> 1)) {
            aligned_blocks_count_ <<= 1;
        }
        head_->aligned_blocks_count_cache_ = aligned_blocks_count_;
        init_cursors();
        mmap_buffer_state_ = memory_map_buffer_state::init_with_memory;
    }


    void siso_ring_buffer::init_cursors()
    {
        head_->rt_writing_cursor_cache_ = 0;
        head_->rt_reading_cursor_cache_ = 0;
        head_->wt_writing_cursor_cache_ = 0;
        head_->wt_reading_cursor_cache_ = 0;
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->reading_cursor_, uint32_t).store_release(0);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_->writing_cursor_, uint32_t).store_release(0);
    }

}
