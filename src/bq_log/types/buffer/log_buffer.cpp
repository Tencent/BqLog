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
 * \class bq::log_buffer
 *
 * \author pippocao
 * \date 2024/12/06
 */
#include "bq_log/types/buffer/log_buffer.h"
#include "bq_log/types/buffer/block_list.h"
#if BQ_JAVA
#include <jni.h>
#endif


namespace bq {

    BQ_PACK_BEGIN
    struct block_misc_data {
        bool is_removed_;
        char padding_inner1_[8 - sizeof(is_removed_)];
        bool need_reallocate_;
        char padding_inner2_[8 - sizeof(need_reallocate_)];
    }
    BQ_PACK_END

    static_assert((offsetof(block_misc_data, is_removed_) % 8 == 0), "invalid alignment of block_misc_data");
    static_assert((offsetof(block_misc_data, need_reallocate_) % 8 == 0), "invalid alignment of block_misc_data");

    bq_forceinline void mark_block_removed(block_node_head* block, bool removed) { BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block->get_misc_data<block_misc_data>().is_removed_, bool).store(removed, bq::platform::memory_order::release); }
    bq_forceinline bool is_block_removed(block_node_head* block)
    {
        bool& is_removed_variable = block->get_misc_data<block_misc_data>().is_removed_;
        if (is_removed_variable) {
            // inter thread sync
            if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(is_removed_variable, bool).load(bq::platform::memory_order::acquire)) {
                return true;
            }
        }
        return false;
    }
    bq_forceinline void mark_block_need_reallocate(block_node_head* block, bool need_reallocate) { block->get_misc_data<block_misc_data>().need_reallocate_ = need_reallocate; }
    bq_forceinline bool is_block_need_reallocate(block_node_head* block) { return block->get_misc_data<block_misc_data>().need_reallocate_; }


    struct log_tls_buffer_info {
        block_node_head* cur_block_ = nullptr; // nullptr means use public miso_ring_buffer.
        uint64_t last_update_epoch_ms_ = 0;
        uint64_t update_times_ = 0;
#if BQ_JAVA
        jobjectArray buffer_obj_for_lp_buffer_ = NULL;  // miso_ring_buffer shared between low frequency threads;
        jobjectArray buffer_obj_for_hp_buffer_ = NULL;  // siso_ring_buffer on block_node;
        int32_t buffer_offset_;            
        block_node_head* buffer_ref_block = nullptr;
#endif
    };
    typedef bq::hash_map_inline<const log_buffer*, log_tls_buffer_info> log_tls_buffer_map_type;

    struct log_tls_info {
#if !BQ_LOG_BUFFER_DEBUG
    private:
#endif
        log_tls_buffer_map_type* log_map_;
        const log_buffer* cur_log_buffer_;
        log_tls_buffer_info* cur_buffer_info_;
    public:
        log_tls_info()
        {
            log_map_ = nullptr;
            cur_log_buffer_ = nullptr;
            cur_buffer_info_ = nullptr;
        }

        log_tls_buffer_info& get_buffer_info(const log_buffer* buffer)
        {
            if (buffer == cur_log_buffer_)
            {
                return *cur_buffer_info_;
            }
            if (!log_map_)
            {
                log_map_ = new log_tls_buffer_map_type();
                log_map_->set_expand_rate(4);
            }
            cur_log_buffer_ = buffer;
            cur_buffer_info_ = &(*log_map_)[buffer];
            return *cur_buffer_info_;
        }

        ~log_tls_info()
        {
            if (log_map_)
            {
                for (auto iter : *log_map_) {
                    auto& buffer_info = iter.value();
                    block_node_head* block = buffer_info.cur_block_;
                    if (block) {
                        mark_block_removed(block, true);
                    }
#if BQ_JAVA
                    if (buffer_info.buffer_obj_for_lp_buffer_
                        || buffer_info.buffer_obj_for_hp_buffer_) {
                        bq::platform::jni_env env;
                        if (buffer_info.buffer_obj_for_lp_buffer_) {
                            env.env->DeleteGlobalRef(buffer_info.buffer_obj_for_lp_buffer_);
                            buffer_info.buffer_obj_for_lp_buffer_ = NULL;
                        }
                        if (buffer_info.buffer_obj_for_hp_buffer_) {
                            env.env->DeleteGlobalRef(buffer_info.buffer_obj_for_hp_buffer_);
                            buffer_info.buffer_obj_for_hp_buffer_ = NULL;
                        }
                    }
#endif
                }
                delete log_map_;
                log_map_ = nullptr;
            }
            cur_log_buffer_ = nullptr;
            cur_buffer_info_ = nullptr;
        }
    };

    static thread_local log_tls_info log_tls_info_;


    log_buffer::log_buffer(log_buffer_config& config)
        : config_(config)
        , high_perform_buffer_(config, BLOCKS_PER_GROUP_NODE)
        , lp_buffer_(config)
    {
        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));
    }

    log_buffer::~log_buffer()
    {
    }

    log_buffer_write_handle log_buffer::alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms)
    {
        auto& tls_buffer = log_tls_info_.get_buffer_info(this);

        block_node_head*& block_cache = tls_buffer.cur_block_;
        uint64_t& thread_last_update_epoch_ms = tls_buffer.last_update_epoch_ms_;
        uint64_t& thread_update_times = tls_buffer.update_times_;

        // frequency check
        bool is_high_frequency = (bool)block_cache;
        if (current_epoch_ms >= thread_last_update_epoch_ms + HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_) {
            if (thread_update_times < config_.high_frequency_threshold_per_second) {
                is_high_frequency = false;
            }
            thread_last_update_epoch_ms = current_epoch_ms;
            thread_update_times = 0;
        }
        if (++thread_update_times >= config_.high_frequency_threshold_per_second) {
            is_high_frequency = true;
            thread_last_update_epoch_ms = current_epoch_ms;
            thread_update_times = 0;
        }
        log_buffer_write_handle result;
        while (true) {
            if (is_high_frequency) {
                if (!block_cache) {
                    block_cache = high_perform_buffer_.alloc_new_block();
                } else if (is_block_need_reallocate(block_cache)) {
                    mark_block_removed(block_cache, true); // mark removed
                    block_cache = high_perform_buffer_.alloc_new_block();
                }
                result = block_cache->get_buffer().alloc_write_chunk(size);
                if (enum_buffer_result_code::err_not_enough_space == result.result) {
                    switch (config_.policy) {
                    case log_memory_policy::auto_expand_when_full:
                        mark_block_removed(block_cache, true); // mark removed
                        block_cache = high_perform_buffer_.alloc_new_block();
                        // discard result and switch to high frequency mode and try again
                        lp_buffer_.commit_write_chunk(result);
                        continue;
                        break;
                    case log_memory_policy::block_when_full:
                        result.result = enum_buffer_result_code::err_wait_and_retry;
                        break;
                    default:
                        break;
                    }
                }
                break;
            } else {
                result = lp_buffer_.alloc_write_chunk(size);
                if (enum_buffer_result_code::err_not_enough_space == result.result) {
                    switch (config_.policy) {
                    case log_memory_policy::auto_expand_when_full:
                        is_high_frequency = true;
                        thread_last_update_epoch_ms = current_epoch_ms;
                        thread_update_times = 0;
                        // discard result and switch to high frequency mode and try again
                        lp_buffer_.commit_write_chunk(result);
                        continue;
                        break;
                    case log_memory_policy::block_when_full:
                        result.result = enum_buffer_result_code::err_wait_and_retry;
                        break;
                    default:
                        break;
                    }
                }
                if (block_cache) {
                    mark_block_removed(block_cache, true); // mark removed;
                    block_cache = nullptr;
                }
                break;
            }
        }
        return result;
    }

    void log_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        block_node_head*& block_cache = log_tls_info_.get_buffer_info(this).cur_block_;
        bool is_high_frequency = block_cache;
        if (is_high_frequency) {
            block_cache->get_buffer().commit_write_chunk(handle);
        } else {
            lp_buffer_.commit_write_chunk(handle);
        }
    }

    log_buffer_read_handle log_buffer::read_chunk()
    {
#if BQ_LOG_BUFFER_DEBUG
        if (check_thread_) {
            bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
            if (read_thread_id_ == empty_thread_id_) {
                read_thread_id_ = current_thread_id;
            } else {
                assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for miso_high_perform_buffer!");
            }
        }
#endif
        bool lp_visited = false;
        bool full_visited = false;
        while(!full_visited) {
            // try read from current data source
            if (rt_cache_.current_reading_.block_) {
#if BQ_LOG_BUFFER_DEBUG
                assert(rt_cache_.current_reading_.group_);
#endif
                auto handle = rt_cache_.current_reading_.block_->get_buffer().read_chunk();
                if (enum_buffer_result_code::err_empty_log_buffer != handle.result) {
                    return handle;
                }
                rt_cache_.current_reading_.block_->get_buffer().return_read_trunk(handle);
            } else {
#if BQ_LOG_BUFFER_DEBUG
                assert(!rt_cache_.current_reading_.group_);
#endif
                auto handle = lp_buffer_.read_chunk();
                if (enum_buffer_result_code::err_empty_log_buffer != handle.result) {
                    return handle;
                }
                lp_buffer_.return_read_trunk(handle);
            }
            // find next data source
            bool next_data_source_found = false;
            while (!next_data_source_found) {
                if (rt_cache_.current_reading_.group_) {
                    block_node_head* next_block = rt_cache_.current_reading_.block_
                        ? rt_cache_.current_reading_.group_.value().get_data_head().used_.next(rt_cache_.current_reading_.block_)
                        : rt_cache_.current_reading_.group_.value().get_data_head().used_.first();
                    if (next_block) {
                        next_data_source_found = true;
                        set_current_reading_block(next_block);
                        continue;
                    }
                    next_block = rt_cache_.current_reading_.group_.value().get_data_head().stage_.pop();
                    if (next_block) {
                        rt_cache_.current_reading_.group_.value().get_data_head().used_.push_after_thread_unsafe(rt_cache_.current_reading_.block_, next_block);
                        next_data_source_found = true;
                        set_current_reading_block(next_block);
                        continue;
                    }
                    auto next_group = high_perform_buffer_.next(rt_cache_.current_reading_.group_, group_list::lock_type::no_lock);
                    set_current_reading_block(next_block);
                    set_current_reading_group(next_group);
                    if (!next_group) {
                        // try lp_buffer_.
                        next_data_source_found = true;
                        continue;
                    }
                } else {
#if BQ_LOG_BUFFER_DEBUG
                    assert(!rt_cache_.current_reading_.block_);
#endif
                    if (lp_visited) {
                        full_visited = true;
                        break;
                    }
                    lp_visited = true;
                    auto next_group = high_perform_buffer_.first(group_list::lock_type::no_lock);
                    if (!next_group) {
                        next_data_source_found = true;
                        continue;
                    }
                    set_current_reading_group(next_group);
                }
            }
        }

        log_buffer_read_handle empty_handle;
        empty_handle.result = enum_buffer_result_code::err_empty_log_buffer;
        empty_handle.data_addr = nullptr;
        empty_handle.data_size = 0;
        return empty_handle;
    }

    void log_buffer::return_read_trunk(const log_buffer_read_handle& handle)
    {
        if (rt_cache_.current_reading_.block_) {
            rt_cache_.current_reading_.block_->get_buffer().return_read_trunk(handle);
        } else {
            lp_buffer_.return_read_trunk(handle);
        }
    }

    void log_buffer::set_thread_check_enable(bool in_enable)
    {
#if BQ_LOG_BUFFER_DEBUG
        check_thread_ = in_enable;
        if (!check_thread_) {
            read_thread_id_ = empty_thread_id_;
        }
        lp_buffer_.set_thread_check_enable(check_thread_);
#else
        (void)in_enable;
#endif
    }

#if BQ_JAVA
    log_buffer::java_buffer_info log_buffer::get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle)
    {
#if BQ_LOG_BUFFER_DEBUG
        assert((this == log_tls_info_.cur_log_buffer_) && "tls cur_log_buffer_ check failed");
#endif
        auto& current_buffer_info = log_tls_info_.get_buffer_info(this);
        java_buffer_info result;
        result.offset_store_ = &current_buffer_info.buffer_offset_;

        if (current_buffer_info.cur_block_) {
            auto& ring_buffer = current_buffer_info.cur_block_->get_buffer();
            if (!current_buffer_info.buffer_obj_for_hp_buffer_) {
                current_buffer_info.buffer_obj_for_hp_buffer_ = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
                env->NewGlobalRef(current_buffer_info.buffer_obj_for_hp_buffer_);
                auto offset_obj = env->GetObjectArrayElement(current_buffer_info.buffer_obj_for_lp_buffer_, 1);
#if BQ_LOG_BUFFER_DEBUG
                assert(offset_obj && "offset obj should not be null when jni hp alloc");
#endif
                env->SetObjectArrayElement(current_buffer_info.buffer_obj_for_hp_buffer_, 1, offset_obj); 
            }
            if (current_buffer_info.buffer_ref_block != current_buffer_info.cur_block_) {
                env->SetObjectArrayElement(current_buffer_info.buffer_obj_for_hp_buffer_, 0, env->NewDirectByteBuffer(const_cast<uint8_t*>(ring_buffer.get_buffer_addr()), (jlong)(ring_buffer.get_block_size() * ring_buffer.get_total_blocks_count())));
                current_buffer_info.buffer_ref_block = current_buffer_info.cur_block_;
            }
            result.buffer_array_obj_ = current_buffer_info.buffer_obj_for_hp_buffer_;
            result.buffer_base_addr_ = ring_buffer.get_buffer_addr();
            *result.offset_store_ = (int32_t)(handle.data_addr - result.buffer_base_addr_);
        } else {
            if (!current_buffer_info.buffer_obj_for_lp_buffer_) {
                current_buffer_info.buffer_obj_for_lp_buffer_ = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
                env->NewGlobalRef(current_buffer_info.buffer_obj_for_lp_buffer_);
                env->SetObjectArrayElement(current_buffer_info.buffer_obj_for_lp_buffer_, 0, env->NewDirectByteBuffer(const_cast<uint8_t*>(lp_buffer_.get_buffer_addr()), (jlong)(lp_buffer_.get_block_size() * lp_buffer_.get_total_blocks_count())));
                env->SetObjectArrayElement(current_buffer_info.buffer_obj_for_lp_buffer_, 1, env->NewDirectByteBuffer(&current_buffer_info.buffer_offset_, sizeof(current_buffer_info.buffer_offset_)));
            }
            result.buffer_array_obj_ = current_buffer_info.buffer_obj_for_lp_buffer_;
            result.buffer_base_addr_ = lp_buffer_.get_buffer_addr();
            *result.offset_store_ = (int32_t)(handle.data_addr - result.buffer_base_addr_);
        }
        return result;
    }
#endif
    void log_buffer::set_current_reading_group(const group_list::iterator& group)
    {
        rt_cache_.current_reading_.group_ = group;
        optimize_memory_for_group(group);
    }

    void log_buffer::set_current_reading_block(block_node_head* block)
    {
        rt_cache_.current_reading_.block_ = block;
        optimize_memory_for_block(block);
    }

    void log_buffer::optimize_memory_for_group(const group_list::iterator& group)
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
#if BQ_LOG_BUFFER_DEBUG
        assert(mem_opt.cur_group_using_blocks_num_ <= high_perform_buffer_.get_max_block_count_per_group());
#endif
        bool group_removed = false;
        if (mem_opt.cur_group_using_blocks_num_ == 0 && mem_opt.cur_group_) {
            // try lock
            group_list::iterator current_lock_iterator;
            if (mem_opt.last_group_) {
                current_lock_iterator = high_perform_buffer_.next(mem_opt.last_group_, group_list::lock_type::write_lock);
            } else {
                // must lock from head, because new group node is inserted after head pointer from other threads.
                while (current_lock_iterator != mem_opt.cur_group_) {
                    current_lock_iterator = (bool)current_lock_iterator
                        ? high_perform_buffer_.next(current_lock_iterator, group_list::lock_type::write_lock)
                        : high_perform_buffer_.first(group_list::lock_type::write_lock);
                    if (!current_lock_iterator) {
                        assert(false && "try lock removing group from head failed");
                    }
                }
            }
            assert(current_lock_iterator == mem_opt.cur_group_ && "try lock removing group failed");
            if ((!current_lock_iterator.value().get_data_head().used_.first())
                && (!current_lock_iterator.value().get_data_head().stage_.first())) {
                // remove it
                group_removed = true;
                high_perform_buffer_.delete_and_unlock_node_thread_unsafe(current_lock_iterator);
            } else {
                // unlock
                high_perform_buffer_.next(current_lock_iterator, group_list::lock_type::no_lock);
            }
        }

        mem_opt.left_holes_num_ += high_perform_buffer_.get_max_block_count_per_group() - rt_cache_.mem_optimize_.cur_group_using_blocks_num_;
        mem_opt.cur_group_using_blocks_num_ = 0;
        if (!group_removed) {
            mem_opt.last_group_ = mem_opt.cur_group_;
        }
        mem_opt.cur_group_ = group;
        if (!group) {
            rt_cache_.mem_optimize_.left_holes_num_ = 0;
        }
    }

    void log_buffer::optimize_memory_for_block(block_node_head* block)
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
        bool block_removed = false;
        if (mem_opt.cur_block_) {
            if (mem_opt.is_block_marked_removed) {
                // remove it
                if (mem_opt.cur_group_.value().get_data_head().used_.is_include(mem_opt.last_block_)) {
                    high_perform_buffer_.recycle_block_thread_unsafe(mem_opt.cur_group_, mem_opt.last_block_, mem_opt.cur_block_);
                } else {
                    high_perform_buffer_.recycle_block_thread_unsafe(mem_opt.cur_group_, nullptr, mem_opt.cur_block_);
                }
                block_removed = true;
            } else {
                ++mem_opt.cur_group_using_blocks_num_;
            }
        }

        if (block) {
            mem_opt.is_block_marked_removed = is_block_removed(block);
            if (mem_opt.left_holes_num_ > 0) {
                mark_block_need_reallocate(block, true);
                --mem_opt.left_holes_num_;
            }
        }
        if (!block_removed) {
            mem_opt.last_block_ = mem_opt.cur_block_;
        }
        mem_opt.cur_block_ = block;
    }
}
