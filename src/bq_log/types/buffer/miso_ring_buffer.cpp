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
    static thread_local miso_tls_info miso_tls_info_;

    /**
     *
     * @param config
     */
    miso_ring_buffer::miso_ring_buffer(const log_buffer_config& config)
        : config_(config)
        , cursors_storage_{0}
        , real_buffer_(nullptr)
        , head_(nullptr)
        , aligned_blocks_(nullptr)
        , aligned_blocks_count_(0)
#if BQ_LOG_BUFFER_DEBUG
        , padding_{0}
#endif
    {
        assert((BQ_POD_RUNTIME_OFFSET_OF(block::chunk_head_def, data) % 8 == 0) && "invalid chunk_head size, it must be a multiple of 8 to ensure the `data` is 8 bytes aligned");
        
        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));
        // make sure it's cache line size aligned
        cursors_ = (cursors_set*)(((uintptr_t)(char*)cursors_storage_ + (uintptr_t)CACHE_LINE_SIZE - 1) & (~((uintptr_t)CACHE_LINE_SIZE - 1)));

        assert(((uintptr_t)&BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t)) % (uintptr_t)CACHE_LINE_SIZE == 0);
        assert(((uintptr_t)&BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t)) % (uintptr_t)CACHE_LINE_SIZE == 0);
        assert((uintptr_t)cursors_ + (uintptr_t)sizeof(cursors_set) <= (uintptr_t)cursors_storage_ + (uintptr_t)sizeof(cursors_storage_));

        auto mmap_result = create_memory_map();
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

#if BQ_LOG_BUFFER_DEBUG
        (void)padding_;
        total_write_bytes_ = 0;
        total_read_bytes_ = 0;
        for (int32_t i = 0; i < (int32_t)enum_buffer_result_code::result_code_count; ++i) {
            result_code_statistics_[i].store_release(0);
        }
#endif
    }

    miso_ring_buffer::~miso_ring_buffer()
    {
        if (!uninit_memory_map()) {
            free(real_buffer_);
        }
    }

    log_buffer_write_handle miso_ring_buffer::alloc_write_chunk(uint32_t size)
    {
        log_buffer_write_handle handle;

        uint32_t size_required = size + (uint32_t)data_block_offset;
        uint32_t need_block_count_tmp = (size_required + (CACHE_LINE_SIZE - 1)) >> CACHE_LINE_SIZE_LOG2;
        if (need_block_count_tmp > aligned_blocks_count_ || need_block_count_tmp == 0 || need_block_count_tmp > block::MAX_BLOCK_NUM_PER_CHUNK) {
#if BQ_LOG_BUFFER_DEBUG
            ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_alloc_size_invalid];
#endif
            handle.result = enum_buffer_result_code::err_alloc_size_invalid;
            return handle;
        }
        uint32_t need_block_count = need_block_count_tmp;
        uint32_t current_write_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).load_raw();
        auto& tls_info = miso_tls_info_.get_buffer_info(this);
        if (tls_info.is_new_created) {
            tls_info.is_new_created = false;
            tls_info.wt_read_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_acquire();
        }
        uint32_t used_blocks_count = static_cast<uint32_t>(current_write_cursor - tls_info.wt_read_cursor_cache_);
        while(true) {
            uint32_t new_cursor = current_write_cursor + need_block_count;
            if (static_cast<uint32_t>(new_cursor - tls_info.wt_read_cursor_cache_) > aligned_blocks_count_) {
                tls_info.wt_read_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_acquire();
                if (static_cast<uint32_t>(new_cursor - tls_info.wt_read_cursor_cache_) > aligned_blocks_count_) {
                    // not enough space
#if BQ_LOG_BUFFER_DEBUG
                    ++result_code_statistics_[(int32_t)enum_buffer_result_code::err_not_enough_space];
#endif
                    handle.result = enum_buffer_result_code::err_not_enough_space;
                    return handle;
                }
            }

            {
                //  This version has better high-concurrency performance compared to the CAS version, but it is not rigorous enough.
                //  In a hypothetical scenario with concurrent thread competition, if the total memory allocated by all threads exceeds 256GB, it could lead to an overflow issue.
                current_write_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).fetch_add_relaxed(need_block_count);
                uint32_t next_write_cursor = current_write_cursor + need_block_count;
                if (static_cast<uint32_t>(next_write_cursor - tls_info.wt_read_cursor_cache_) >= aligned_blocks_count_) {
                    while (static_cast<uint32_t>(next_write_cursor - (tls_info.wt_read_cursor_cache_ = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_acquire())) >= aligned_blocks_count_) {
                        // fall back
                        uint32_t expected = next_write_cursor;
                        if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).compare_exchange_strong(expected, current_write_cursor, platform::memory_order::relaxed, platform::memory_order::relaxed)) {
                            // not enough space
#if BQ_LOG_BUFFER_DEBUG
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
#if BQ_LOG_BUFFER_DEBUG
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
#if BQ_LOG_BUFFER_DEBUG
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
#if BQ_LOG_BUFFER_DEBUG
        total_write_bytes_ += block_ptr->chunk_head.get_block_num() * sizeof(block);
#endif
    }

    log_buffer_read_handle miso_ring_buffer::read_chunk()
    {
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
#endif
        log_buffer_read_handle handle;
        while (true) {
            uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
            block& block_ref = cursor_to_block(current_read_cursor);
            auto status = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block_ref.chunk_head.status, block_status).load_acquire();
            auto block_count = block_ref.chunk_head.get_block_num();
            switch (status) {
            case block_status::invalid:
#if BQ_LOG_BUFFER_DEBUG
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count > aligned_blocks_count_);
                assert(block_count <= aligned_blocks_count_);
#endif
                block_ref.chunk_head.status = block_status::unused;
                head_->read_cursor_cache_ = current_read_cursor + block_count;
                BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(head_->read_cursor_cache_);
                continue;
                break;
            case block_status::unused:
                handle.result = enum_buffer_result_code::err_empty_log_buffer;
                break;
            case block_status::used:
#if BQ_LOG_BUFFER_DEBUG
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count <= aligned_blocks_count_);
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

#if BQ_LOG_BUFFER_DEBUG
        ++result_code_statistics_[(int32_t)handle.result];
#endif
        return handle;
    }

    void miso_ring_buffer::return_read_trunk(const log_buffer_read_handle& handle)
    {
        if (handle.result != enum_buffer_result_code::success) {
            return;
        }
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
        assert((handle.data_addr <= (uint8_t*)(aligned_blocks_ + aligned_blocks_count_)) && (handle.data_addr >= (uint8_t*)aligned_blocks_)
            && "please don't return a read handle not belongs to this miso_ring_buffer");
#endif
        uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
        block* block_ptr = (handle.data_addr == (uint8_t*)aligned_blocks_)
            ? &cursor_to_block(current_read_cursor) // splited
            : (block*)(handle.data_addr - data_block_offset);

        uint32_t start_cursor = current_read_cursor;
        uint32_t block_count = block_ptr->chunk_head.get_block_num();
        if (block_count > 0) {
            for (uint32_t i = 0; i < block_count; ++i) {
                cursor_to_block(start_cursor + i).chunk_head.status = block_status::unused;
            }
#if BQ_LOG_BUFFER_DEBUG
            total_read_bytes_ += block_count * sizeof(block);
#endif
            head_->read_cursor_cache_ = current_read_cursor + block_count;

            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(head_->read_cursor_cache_);
        }
    }

    
    void miso_ring_buffer::data_traverse(void (*in_callback)(uint8_t* data, uint32_t size, void* user_data), void* in_user_data)
    {
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            if (0 == read_thread_id_) {
                read_thread_id_ = bq::platform::thread::get_current_thread_id();
            }
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_ring_buffer!");
        }
#endif
        uint32_t current_read_cursor = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).load_raw();
        bool finished = false;
        while (!finished) {
            block& block_ref = cursor_to_block(current_read_cursor);
            auto status = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block_ref.chunk_head.status, block_status).load_acquire();
            auto block_count = block_ref.chunk_head.get_block_num();
            switch (status) {
            case block_status::invalid:
#if BQ_LOG_BUFFER_DEBUG
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count > aligned_blocks_count_);
                assert(block_count <= aligned_blocks_count_);
#endif
                continue;
                break;
            case block_status::unused:
                finished = true;
                break;
            case block_status::used:
#if BQ_LOG_BUFFER_DEBUG
                assert((current_read_cursor & (aligned_blocks_count_ - 1)) + block_count <= aligned_blocks_count_);
#endif
                in_callback(block_ref.chunk_head.data, block_ref.chunk_head.data_size, in_user_data); 
                break;
            default:
                assert(false && "invalid read ring buffer block status");
                break;
            }
            current_read_cursor = current_read_cursor + block_count;
        }
    }

    create_memory_map_result miso_ring_buffer::create_memory_map()
    {
        if (!bq::memory_map::is_platform_support() || !config_.use_mmap) {
            return create_memory_map_result::failed;
        }
        bq::string path = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name + "/" + config_.log_name + ".mmap", true);
        memory_map_file_ = bq::file_manager::instance().open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        if (!memory_map_file_.is_valid()) {
            bq::util::log_device_console(bq::log_level::warning, "failed to open mmap file %s, use memory instead of mmap file, error code:%d", path.c_str(), bq::file_manager::get_and_clear_last_file_error());
            return create_memory_map_result::failed;
        }
        aligned_blocks_count_ = config_.default_buffer_size >> CACHE_LINE_SIZE_LOG2;
        size_t head_size = sizeof(head);
        size_t map_size = (uint32_t)(config_.default_buffer_size + head_size);
        size_t file_size = bq::memory_map::get_min_size_of_memory_map_file(0, map_size);
        create_memory_map_result result = create_memory_map_result::failed;
        size_t current_file_size = bq::file_manager::instance().get_file_size(memory_map_file_);
        if (current_file_size != file_size) {
            if (!bq::file_manager::instance().truncate_file(memory_map_file_, file_size)) {
                bq::util::log_device_console(log_level::warning, "ring buffer truncate memory map file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
                bq::file_manager::instance().close_file(memory_map_file_);
                bq::file_manager::remove_file_or_dir(path);
                return create_memory_map_result::failed;
            }
            result = create_memory_map_result::new_created;
        } else {
            result = create_memory_map_result::use_existed;
        }

        memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, map_size);
        if (!memory_map_handle_.has_been_mapped()) {
            bq::util::log_device_console(log_level::warning, "ring buffer create memory map failed from file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
            return create_memory_map_result::failed;
        }

        if (((uintptr_t)memory_map_handle_.get_mapped_data() & (CACHE_LINE_SIZE - 1)) != 0) {
            bq::util::log_device_console(log_level::warning, "ring buffer memory map file \"%s\" memory address is not aligned, use memory instead.", memory_map_file_.abs_file_path().c_str());
            bq::memory_map::release_memory_map(memory_map_handle_);
            return create_memory_map_result::failed;
        }
        real_buffer_ = (uint8_t*)memory_map_handle_.get_mapped_data();
        head_ = (head*)real_buffer_;
        aligned_blocks_ = (miso_ring_buffer::block*)(real_buffer_ + head_size);
        return result;
    }

    void miso_ring_buffer::init_with_memory_map()
    {
        for (uint32_t i = 0; i < aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(aligned_blocks_[i].chunk_head.status, block_status).store_release(bq::miso_ring_buffer::block_status::unused);
        }
        head_->read_cursor_cache_ = 0;
        head_->log_checksum_ = config_.calculate_check_sum();
        memset(head_->mmap_misc_data_, 0, sizeof(head_->mmap_misc_data_));

        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(0);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(0);
    }

    bool miso_ring_buffer::try_recover_from_exist_memory_map()
    {
        // verify
        if (head_->log_checksum_ != config_.calculate_check_sum()) {
            return false;
        }

        head_->read_cursor_cache_ = head_->read_cursor_cache_ & (aligned_blocks_count_ - 1);
        uint32_t current_cursor = head_->read_cursor_cache_;
        bool data_parse_finished = false;
        while ((static_cast<uint32_t>(current_cursor - head_->read_cursor_cache_) < aligned_blocks_count_) && !data_parse_finished) {
            block& current_block = cursor_to_block(current_cursor);
            auto block_num = current_block.chunk_head.get_block_num();
            auto data_size = current_block.chunk_head.data_size;
            if (current_cursor + block_num > head_->read_cursor_cache_ + aligned_blocks_count_) {
                return false;
            }
            switch (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(current_block.chunk_head.status, block_status).load_relaxed()) {
            case block_status::used:
                if ((current_cursor & (aligned_blocks_count_ - 1)) + block_num > aligned_blocks_count_) {
                    return false;
                }
                if ((size_t)data_size > (block_num * sizeof(block) - data_block_offset)
                    || (size_t)data_size <= (block_num == 1 ? 0 : (block_num * sizeof(block) - sizeof(block) - data_block_offset))) {
                    return false;
                }
                current_cursor += block_num;
                break;
            case block_status::invalid:
                current_cursor += block_num;
                if ((current_cursor & (aligned_blocks_count_ - 1)) + block_num <= aligned_blocks_count_) {
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
        }
        if (current_cursor - head_->read_cursor_cache_ > aligned_blocks_count_) {
            return false;
        }
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(current_cursor);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(head_->read_cursor_cache_);
        

        for (uint32_t i = current_cursor; i < head_->read_cursor_cache_ + aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursor_to_block(i).chunk_head.status, block_status).store_release(block_status::unused);
        }
        return true;
    }

    void miso_ring_buffer::init_with_memory()
    {
        aligned_blocks_count_ = config_.default_buffer_size >> CACHE_LINE_SIZE_LOG2;

        size_t head_size = sizeof(head);

        size_t alloc_size = (uint32_t)(config_.default_buffer_size + head_size + CACHE_LINE_SIZE);
        real_buffer_ = (uint8_t*)malloc(sizeof(uint8_t) * alloc_size);

        size_t align_unit = CACHE_LINE_SIZE;
        uintptr_t aligned_addr = (uintptr_t)real_buffer_;
        aligned_addr = (aligned_addr + align_unit - 1) & (~(align_unit - 1));
        head_ = (head*)aligned_addr;
        aligned_blocks_ = (miso_ring_buffer::block*)(aligned_addr + head_size);
        for (uint32_t i = 0; i < aligned_blocks_count_; ++i) {
            BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(aligned_blocks_[i].chunk_head.status, block_status).store(bq::miso_ring_buffer::block_status::unused, platform::memory_order::release);
        }
        head_->read_cursor_cache_ = 0;
        head_->log_checksum_ = 0;
        memset(head_->mmap_misc_data_, 0, sizeof(head_->mmap_misc_data_));

        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->write_cursor_, uint32_t).store_release(0);
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(cursors_->read_cursor_, uint32_t).store_release(0);
    }

    bool miso_ring_buffer::uninit_memory_map()
    {
        if (memory_map_handle_.has_been_mapped()) {
            bq::memory_map::release_memory_map(memory_map_handle_);
            return true;
        }
        return false;
    }

}
