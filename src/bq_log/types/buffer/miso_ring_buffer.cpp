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

#include "bq_log/types/buffer/miso_ring_buffer.h"

namespace bq {

    struct miso_tls_buffer_info {
        uint32_t wt_read_cursor_cache_;
        bool is_new_created = true;
    };
    typedef bq::hash_map_inline<const miso_ring_buffer*, miso_tls_buffer_info> miso_tls_buffer_map_type;

    struct miso_tls_info {
    private:
        miso_tls_buffer_map_type* buffer_map_;
        const miso_ring_buffer* cur_buffer_;
        miso_tls_buffer_info* cur_buffer_info_;

    public:
        miso_tls_info()
        {
            buffer_map_ = nullptr;
            cur_buffer_ = nullptr;
            cur_buffer_info_ = nullptr;
        }

        miso_tls_buffer_info& get_buffer_info(const miso_ring_buffer* buffer)
        {
            if (buffer == cur_buffer_) {
                return *cur_buffer_info_;
            }
            if (!buffer_map_) {
                buffer_map_ = new miso_tls_buffer_map_type();
                buffer_map_->set_expand_rate(4);
            }
            cur_buffer_ = buffer;
            cur_buffer_info_ = &(*buffer_map_)[buffer];
            return *cur_buffer_info_;
        }

        ~miso_tls_info()
        {
            if (buffer_map_) {
                delete buffer_map_;
                buffer_map_ = nullptr;
            }
            cur_buffer_ = nullptr;
            cur_buffer_info_ = nullptr;
        }
    };
    BQ_TLS_NON_POD(miso_tls_info, miso_tls_info_)

    /**
     *
     * @param config
     */
    miso_ring_buffer::miso_ring_buffer(const log_buffer_config& config)
        : config_(config)
        , head_(nullptr)
        , aligned_blocks_(nullptr)
        , aligned_blocks_count_(0)
    {
        assert((BQ_POD_RUNTIME_OFFSET_OF(block::chunk_head_def, data) % 8 == 0) && "invalid chunk_head size, it must be a multiple of 8 to ensure the `data` is 8 bytes aligned");

        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::BQ_CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));

        assert((uintptr_t)&cursors_.write_cursor_ % (uintptr_t)BQ_CACHE_LINE_SIZE == 0);
        assert((uintptr_t)&cursors_.read_cursor_ % (uintptr_t)BQ_CACHE_LINE_SIZE == 0);

        auto mmap_result = create_memory_map();
        aligned_blocks_count_ = config_.default_buffer_size >> BQ_CACHE_LINE_SIZE_LOG2;
        head_ = static_cast<head*>(buffer_entity_->data());
        aligned_blocks_ = (miso_ring_buffer::block*)(head_ + 1);

        switch (mmap_result) {
        case bq::create_memory_map_result::failed:
            init_with_memory();
            break;
        case bq::create_memory_map_result::use_existed:
            if (!try_recover_from_exist_memory_map()) {
                init_with_memory_map();
            }
            break;
        case bq::create_memory_map_result::new_created:
            init_with_memory_map();
            break;
        default:
            break;
        }
#if defined(BQ_LOG_BUFFER_DEBUG)
        total_write_bytes_ = 0;
        total_read_bytes_ = 0;
        for (int32_t i = 0; i < (int32_t)enum_buffer_result_code::result_code_count; ++i) {
            result_code_statistics_[i].store_release(0);
        }
#endif
    }

    miso_ring_buffer::~miso_ring_buffer()
    {
        head_ = nullptr;
    }

    log_buffer_write_handle miso_ring_buffer::alloc_write_chunk(uint32_t size)
    {
        log_buffer_write_handle handle;

        uint32_t size_required = size + (uint32_t)data_block_offset;
        uint32_t need_block_count_tmp = static_cast<uint32_t>((size_required + (BQ_CACHE_LINE_SIZE - 1)) >> BQ_CACHE_LINE_SIZE_LOG2);
        if (need_block_count_tmp > aligned_blocks_count_ || need_block_count_tmp == 0 || need_block_count_tmp > block::MAX_BLOCK_NUM_PER_CHUNK) {
#if defined(BQ_LOG_BUFFER_DEBUG)
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
            handle.result = enum_buffer_result_code::err_alloc_size_invalid;
            return handle;
        }
        uint32_t need_block_count = need_block_count_tmp;
        uint32_t current_write_cursor = cursors_.write_cursor_.load_relaxed();
        uint32_t read_cursor_tmp_when_tls_recycled;
        uint32_t* read_cursor_ptr = nullptr;
        if (miso_tls_info_) {
            auto& tls_info = miso_tls_info_.get().get_buffer_info(this);
            if (tls_info.is_new_created) {
                tls_info.is_new_created = false;
                tls_info.wt_read_cursor_cache_ = cursors_.read_cursor_.load_acquire();
            }
            read_cursor_ptr = &tls_info.wt_read_cursor_cache_;
        } else {
            read_cursor_ptr = &read_cursor_tmp_when_tls_recycled;
            read_cursor_tmp_when_tls_recycled = cursors_.read_cursor_.load_acquire();
        }
        uint32_t& read_cursor_ref = *read_cursor_ptr;

        uint32_t used_blocks_count = static_cast<uint32_t>(current_write_cursor - read_cursor_ref);
        while (true) {
            uint32_t new_cursor = current_write_cursor + need_block_count;
            if (static_cast<uint32_t>(new_cursor - read_cursor_ref) > aligned_blocks_count_) {
                read_cursor_ref = cursors_.read_cursor_.load_acquire();
                if (static_cast<uint32_t>(new_cursor - read_cursor_ref) > aligned_blocks_count_) {
                    BQ_UNLIKELY_IF((need_block_count << 1) > aligned_blocks_count_)
                    {
                        // not enough space
#if defined(BQ_LOG_BUFFER_DEBUG)
                        ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
                        handle.result = enum_buffer_result_code::err_alloc_size_invalid;
                        return handle;
                    }
                    // not enough space
#if defined(BQ_LOG_BUFFER_DEBUG)
                    ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                    handle.result = enum_buffer_result_code::err_not_enough_space;
                    return handle;
                }
            }

            {
                //  This version has better high-concurrency performance compared to the CAS version, but it is not rigorous enough.
                //  In a hypothetical scenario with concurrent thread competition, if the total memory allocated by all threads exceeds 256GB, it could lead to an overflow issue.
                current_write_cursor = cursors_.write_cursor_.fetch_add_relaxed(need_block_count);
                uint32_t next_write_cursor = current_write_cursor + need_block_count;
                if (static_cast<uint32_t>(next_write_cursor - read_cursor_ref) >= aligned_blocks_count_) {
                    while (static_cast<uint32_t>(next_write_cursor - (read_cursor_ref = cursors_.read_cursor_.load_acquire())) >= aligned_blocks_count_) {
                        // fall back
                        uint32_t expected = next_write_cursor;
                        if (cursors_.write_cursor_.compare_exchange_strong(expected, current_write_cursor, platform::memory_order::relaxed, platform::memory_order::relaxed)) {
                            BQ_UNLIKELY_IF((need_block_count << 1) > aligned_blocks_count_)
                            {
                                // not enough space
#if defined(BQ_LOG_BUFFER_DEBUG)
                                ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
                                handle.result = enum_buffer_result_code::err_alloc_size_invalid;
                                return handle;
                            }
                            // not enough space
#if defined(BQ_LOG_BUFFER_DEBUG)
                            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                            handle.result = enum_buffer_result_code::err_not_enough_space;
                            return handle;
                        }
                    }
                }
            }

            block& new_block = cursor_to_block(current_write_cursor);
            new_block.chunk_head.set_block_num(need_block_count);
            if ((current_write_cursor & (aligned_blocks_count_ - 1)) + need_block_count > aligned_blocks_count_) {
                // data must be guaranteed to be contiguous in memory
                handle.result = enum_buffer_result_code::err_data_not_contiguous;
#if defined(BQ_LOG_BUFFER_DEBUG)
                ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_data_not_contiguous];
#endif
                BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(new_block.chunk_head.status, block_status).store_release(bq::miso_ring_buffer::block_status::invalid);
                continue;
            }
            new_block.chunk_head.data_size = size;
            handle.result = enum_buffer_result_code::success;
            handle.data_addr = new_block.chunk_head.data;
            used_blocks_count += need_block_count;
            handle.low_space_flag = ((used_blocks_count << 1) >= aligned_blocks_count_);
#if defined(BQ_LOG_BUFFER_DEBUG)
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::success];
#endif
            return handle;
        }
        return handle;
    }

    void miso_ring_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
        block* block_ptr = (block*)(handle.data_addr - data_block_offset);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block_ptr->chunk_head.status, block_status).store_release(bq::miso_ring_buffer::block_status::used);
#if defined(BQ_LOG_BUFFER_DEBUG)
        total_write_bytes_ += block_ptr->chunk_head.get_block_num() * sizeof(block);
#endif
    }

    log_buffer_read_handle miso_ring_buffer::read_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(!is_read_chunk_waiting_for_return_ && "You cant read a chunk before you returning last chunk back");
        if (check_thread_) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
        is_read_chunk_waiting_for_return_ = true;
#endif
        log_buffer_read_handle handle;
        while (true) {
            block& block_ref = cursor_to_block(head_->read_cursor_cache_);
            auto status = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block_ref.chunk_head.status, block_status).load_acquire();
            auto block_count = block_ref.chunk_head.get_block_num();
            switch (status) {
            case block_status::invalid:
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert((head_->read_cursor_cache_ & (aligned_blocks_count_ - 1)) + block_count > aligned_blocks_count_);
                assert(block_count <= aligned_blocks_count_);
#endif
                block_ref.chunk_head.status = block_status::unused;
                head_->read_cursor_cache_ += block_count;
                continue;
                break;
            case block_status::unused:
                handle.result = enum_buffer_result_code::err_empty_log_buffer;
                break;
            case block_status::used:
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert((head_->read_cursor_cache_ & (aligned_blocks_count_ - 1)) + block_count <= aligned_blocks_count_);
#endif
                handle.result = enum_buffer_result_code::success;
                handle.data_addr = block_ref.chunk_head.data;
                handle.data_size = block_ref.chunk_head.data_size;
                break;
            default:
                assert(false && "invalid read ring buffer block status");
                break;
            }
            break;
        }

#if defined(BQ_LOG_BUFFER_DEBUG)
        ++result_code_statistics_[(int32_t)handle.result];
#endif
        return handle;
    }

    log_buffer_read_handle miso_ring_buffer::read_an_empty_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(!is_read_chunk_waiting_for_return_ && "You cant read a chunk before you returning last chunk back");
        if (check_thread_) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
        is_read_chunk_waiting_for_return_ = true;
#endif
        log_buffer_read_handle handle;
        handle.result = enum_buffer_result_code::err_empty_log_buffer;
        return handle;
    }

    void miso_ring_buffer::discard_read_chunk(log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(is_read_chunk_waiting_for_return_ && "You cant discard a chunk has not been read yet");
        is_read_chunk_waiting_for_return_ = false;
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
#endif
        handle.result = enum_buffer_result_code::err_empty_log_buffer;
    }

    void miso_ring_buffer::return_read_chunk(const log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(is_read_chunk_waiting_for_return_ && "You cant return a chunk has not been read yet");
        is_read_chunk_waiting_for_return_ = false;
#endif
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
        if (handle.result == enum_buffer_result_code::success) {
            assert((handle.data_addr <= (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) && (handle.data_addr >= (uint8_t*)aligned_blocks_)
                && "please don't return a read handle not belongs to this miso_ring_buffer");
        }
#endif
        bool need_flush = false;
        uint32_t start_cursor = head_->read_cursor_start_cache_;
        if (handle.result == enum_buffer_result_code::err_empty_log_buffer) {
            need_flush = true;
        } else if (handle.result == enum_buffer_result_code::success) {
            head_->read_cursor_cache_ += cursor_to_block(head_->read_cursor_cache_).chunk_head.get_block_num();
            need_flush = (head_->read_cursor_cache_ - start_cursor >= (aligned_blocks_count_ >> 2));
        }
        if (!need_flush) {
            return;
        }
        uint32_t block_count = head_->read_cursor_cache_ - start_cursor;
        if (block_count > 0) {
            for (uint32_t i = 0; i < block_count; ++i) {
                cursor_to_block(start_cursor + i).chunk_head.status = block_status::unused;
            }
#if defined(BQ_LOG_BUFFER_DEBUG)
            total_read_bytes_ += block_count * sizeof(block);
#endif
            head_->read_cursor_start_cache_ = head_->read_cursor_cache_;
            cursors_.read_cursor_.store_release(head_->read_cursor_cache_);
        }
    }

    void miso_ring_buffer::data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (check_thread_) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
#endif
        uint32_t current_read_cursor = cursors_.read_cursor_.load_acquire();
        bool finished = false;
        while (!finished) {
            block& block_ref = cursor_to_block(current_read_cursor);
            auto status = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block_ref.chunk_head.status, block_status).load_acquire();
            auto block_count = block_ref.chunk_head.get_block_num();
            switch (status) {
            case block_status::invalid:
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count > aligned_blocks_count_);
                assert(block_count <= aligned_blocks_count_);
#endif
                break;
            case block_status::unused:
                finished = true;
                break;
            case block_status::used:
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count <= aligned_blocks_count_);
#endif
                in_callback(block_ref.chunk_head.data, block_ref.chunk_head.data_size, in_user_data);
                break;
            default:
                assert(false && "invalid read ring buffer block status");
                break;
            }
            current_read_cursor = current_read_cursor + block_count;
            if (current_read_cursor - cursors_.read_cursor_.load_raw() >= aligned_blocks_count_) {
                finished = true;
            }
        }
    }

    create_memory_map_result miso_ring_buffer::create_memory_map()
    {
        bq::string path = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name + "/" + config_.log_name + ".mmap", 0); 
        size_t head_size = sizeof(head);
        size_t map_size = (uint32_t)(config_.default_buffer_size + head_size);
        buffer_entity_ = bq::make_unique<normal_buffer>(map_size, config_.need_recovery ? path : "", true);
        return buffer_entity_->get_mmap_result();
    }

    void miso_ring_buffer::init_with_memory_map()
    {
        for (uint32_t i = 0; i < aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(aligned_blocks_[i].chunk_head.status, block_status).store_release(bq::miso_ring_buffer::block_status::unused);
        }
        head_->read_cursor_cache_ = 0;
        head_->read_cursor_start_cache_ = 0;
        head_->log_checksum_ = config_.calculate_check_sum();
        memset(head_->mmap_misc_data_, 0, sizeof(head_->mmap_misc_data_));

        cursors_.write_cursor_.store_release(0);
        cursors_.read_cursor_.store_release(0);
        mmap_buffer_state_ = memory_map_buffer_state::init_with_memory;
    }

    bool miso_ring_buffer::try_recover_from_exist_memory_map()
    {
        // verify
        if (head_->log_checksum_ != config_.calculate_check_sum()) {
            return false;
        }

        head_->read_cursor_cache_ = head_->read_cursor_cache_ & (aligned_blocks_count_ - 1);
        head_->read_cursor_start_cache_ = head_->read_cursor_cache_;
        uint32_t current_cursor = head_->read_cursor_cache_;
        bool data_parse_finished = false;
        while (!data_parse_finished) {
            block& current_block = cursor_to_block(current_cursor);
            auto block_num = current_block.chunk_head.get_block_num();
            auto data_size = current_block.chunk_head.data_size;
            auto status = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(current_block.chunk_head.status, block_status).load_raw();
            switch (status) {
            case block_status::used:
                if ((current_cursor & (aligned_blocks_count_ - 1)) + block_num > aligned_blocks_count_) {
                    return false;
                }
                if ((size_t)data_size > static_cast<size_t>(static_cast<intptr_t>(block_num * sizeof(block)) - data_block_offset)
                    || (size_t)data_size <= (block_num == 1 ? 0 : static_cast<size_t>((static_cast<intptr_t>(block_num * sizeof(block)) - static_cast<ptrdiff_t>(sizeof(block)) - data_block_offset)))) {
                    return false;
                }
                current_cursor += block_num;
                break;
            case block_status::invalid:
                current_cursor += block_num;
                if ((current_cursor & (aligned_blocks_count_ - 1)) + block_num > aligned_blocks_count_) {
                    return false;
                }
                break;
            case block_status::unused:
                data_parse_finished = true;
                break;
            default:
                return false;
                break;
            }
            uint32_t total_size_parsed = static_cast<uint32_t>(current_cursor - head_->read_cursor_cache_);
            if (total_size_parsed == aligned_blocks_count_) {
                data_parse_finished = true;
            } else if (total_size_parsed > aligned_blocks_count_) {
                return false;
            }
        }
        if (current_cursor - head_->read_cursor_cache_ > aligned_blocks_count_) {
            return false;
        }
        cursors_.write_cursor_.store_release(current_cursor);
        cursors_.read_cursor_.store_release(head_->read_cursor_cache_);

        for (uint32_t i = current_cursor; i < head_->read_cursor_cache_ + aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursor_to_block(i).chunk_head.status, block_status).store_release(block_status::unused);
        }
        mmap_buffer_state_ = memory_map_buffer_state::recover_from_memory_map;
        return true;
    }

    void miso_ring_buffer::init_with_memory()
    {
        for (uint32_t i = 0; i < aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(aligned_blocks_[i].chunk_head.status, block_status).store(bq::miso_ring_buffer::block_status::unused, platform::memory_order::release);
        }
        head_->read_cursor_cache_ = 0;
        head_->read_cursor_start_cache_ = 0;
        head_->log_checksum_ = 0;
        memset(head_->mmap_misc_data_, 0, sizeof(head_->mmap_misc_data_));
        cursors_.write_cursor_.store_release(0);
        cursors_.read_cursor_.store_release(0);
        mmap_buffer_state_ = memory_map_buffer_state::init_with_memory;
    }
}
