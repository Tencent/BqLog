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
 * \class bq::miso_high_perform_buffer
 * 
 * \author pippocao
 * \date 2024/12/06
 */
 #include "bq_log/types/buffer/high_performance/miso_high_perform_buffer.h"
 #if BQ_WIN
 #include <Windows.h>
 #elif BQ_POSIX
 #include <pthread.h>
 #endif
 namespace bq {
    static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 16;

    miso_high_perform_buffer::miso_high_perform_buffer(log_buffer_config& config)
        : config_(config)
        , high_perform_buffer_(config, BLOCKS_PER_GROUP_NODE)
        , normal_buffer_(config)
    {
        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));
    }

    miso_high_perform_buffer::~miso_high_perform_buffer()
    {
    }

    BQ_TLS_STRUCT_DECLARE_BEGIN(hp_buffer_thread_cache_)
    BQ_TLS_STRUCT_FIELD(hp_buffer_thread_cache_, block_node_head*, hp_buffer_thread_cache_current_block_, nullptr)
    BQ_TLS_STRUCT_FIELD(hp_buffer_thread_cache_, uint64_t, hp_buffer_thread_cache_last_record_epoch_ms_, 0)
    BQ_TLS_STRUCT_FIELD(hp_buffer_thread_cache_, size_t, hp_buffer_thread_cache_call_times_, 0)
    BQ_TLS_STRUCT_DECLARE_END(hp_buffer_thread_cache_)

    
    struct block_misc_data {
        bool is_removed_;
        char padding_inner1_[8 - sizeof(is_removed_)];
        bool need_reallocate_;
        char padding_inner2_[8 - sizeof(need_reallocate_)];
#if BQ_WIN
        bq::platform::thread::thread_id thread_id_;
        uint64_t last_update_epoch_ms_;
        HANDLE thread_handle_;
#elif BQ_POSIX
#else
        assert(false, "unsupported platform!");
#endif
    };
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

#if BQ_POSIX
    static pthread_key_t hp_buffer_pthread_key_;
    static BQ_TSL is_pthread_destructor_registered_ = false;
    static void on_log_thread_exist(void*)
    {
        block_node_head*& block_cache = BQ_TLS_FIELD_REF(hp_buffer_thread_cache_, hp_buffer_thread_cache_current_block_);
        if (block_cache)
        {
            block_cache->mark_removed(true);
        }
    }
    struct hp_pthread_initer
    {
        hp_pthread_initer()
        {
            int32_t result = pthread_key_create(&hp_buffer_pthread_key_, &on_log_thread_exist);
            if (0 != result)
            {
                bq::util::log_device_console(bq::log_level::fatal, "failed to call pthread_key_create, err value:%d", result);
            }
        }
    };
    static hp_pthread_initer pthread_initer_;
#endif

    log_buffer_write_handle miso_high_perform_buffer::alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms)
    {
        block_node_head*& block_cache = BQ_TLS_FIELD_REF(hp_buffer_thread_cache_, hp_buffer_thread_cache_current_block_);
        uint64_t& last_record_epoch_ms = BQ_TLS_FIELD_REF(hp_buffer_thread_cache_, hp_buffer_thread_cache_last_record_epoch_ms_);
        size_t& thread_cache_call_times = BQ_TLS_FIELD_REF(hp_buffer_thread_cache_, hp_buffer_thread_cache_call_times_);

        //frequency check
        bool is_high_frequency = (bool)block_cache;
        if (current_epoch_ms >= last_record_epoch_ms + HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL_) {
            if (thread_cache_call_times < HP_BUFFER_CALL_FREQUENCY_CHECK_THRESHOLD_) {
                is_high_frequency = false;
            }
            last_record_epoch_ms = current_epoch_ms;
            thread_cache_call_times = 0;
        }
        if (++thread_cache_call_times >= HP_BUFFER_CALL_FREQUENCY_CHECK_THRESHOLD_) {
            is_high_frequency = true;
            last_record_epoch_ms = current_epoch_ms;
            thread_cache_call_times = 0;
        }
        log_buffer_write_handle result;
        while (true)
        {
            if (is_high_frequency) {
                if (!block_cache) {
                    block_cache = high_perform_buffer_.alloc_new_block();
                } else if (is_block_need_reallocate(block_cache)) {
                    mark_block_removed(block_cache, true);// mark removed
                    block_cache = high_perform_buffer_.alloc_new_block();
                }
                result = block_cache->get_buffer().alloc_write_chunk(size);
                if (config_.auto_expand && (enum_buffer_result_code::err_not_enough_space == result.result)) {
                    mark_block_removed(block_cache, true); // mark removed
                    block_cache = high_perform_buffer_.alloc_new_block();
                    result = block_cache->get_buffer().alloc_write_chunk(size);
                }
#if BQ_WIN
                auto& misc_data = block_cache->get_misc_data<block_misc_data>();
                if (misc_data.last_update_epoch_ms_ + THREAD_ALIVE_UPDATE_INTERVAL <= current_epoch_ms)
                {
                    misc_data.last_update_epoch_ms_ = current_epoch_ms;
                }
#endif
                break;
            } else {
                result = normal_buffer_.alloc_write_chunk(size);
                if (config_.auto_expand && (enum_buffer_result_code::err_not_enough_space == result.result)) {
                    is_high_frequency = true;
                    last_record_epoch_ms = current_epoch_ms;
                    thread_cache_call_times = 0;
                    continue;
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

    void miso_high_perform_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        block_node_head*& block_cache = BQ_TLS_FIELD_REF(hp_buffer_thread_cache_, hp_buffer_thread_cache_current_block_);
        bool is_high_frequency = (!block_cache);
        if (is_high_frequency) {
            block_cache->get_buffer().commit_write_chunk(handle);
        } else {
            normal_buffer_.commit_write_chunk(handle);
        }
    }


    log_buffer_read_handle miso_high_perform_buffer::read_chunk(uint64_t current_epoch_ms)
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
        const block_node_head const* start_node = rt_cache_.current_reading_.block_;
        do{
            //try read from current data source
            if (rt_cache_.current_reading_.block_) {
#if BQ_LOG_BUFFER_DEBUG
                assert(rt_cache_.current_reading_.group_);
#endif
                auto handle = rt_cache_.current_reading_.block_->get_buffer().read_chunk();
                if (enum_buffer_result_code::err_empty_ring_buffer != handle.result) {
                    return handle;
                }
            } else {
#if BQ_LOG_BUFFER_DEBUG
                assert(!rt_cache_.current_reading_.group_);
#endif
                auto handle = normal_buffer_.read_chunk(current_epoch_ms);
                if (enum_buffer_result_code::err_empty_ring_buffer != handle.result) {
                    return handle;
                }
            }
            //find next data source
            bool next_data_source_found = false;
            while (!next_data_source_found)
            {
                if (rt_cache_.current_reading_.group_) {
                    block_node_head* next_block = rt_cache_.current_reading_.block_ 
                        ? rt_cache_.current_reading_.group_.value().get_data().used_.next(rt_cache_.current_reading_.block_)
                        : rt_cache_.current_reading_.group_.value().get_data().used_.first();
                    if (next_block) {
                        next_data_source_found = true;
                        optimize_memory_for_block(next_block, current_epoch_ms);
                        rt_cache_.current_reading_.block_ = next_block;
                        break;
                    }
                    next_block = rt_cache_.current_reading_.group_.value().get_data().stage_.pop();
                    if (next_block) {
                        rt_cache_.current_reading_.group_.value().get_data().used_.push_after_thread_unsafe(rt_cache_.current_reading_.block_, next_block);
                        next_data_source_found = true;
                        optimize_memory_for_block(next_block, current_epoch_ms);
                        rt_cache_.current_reading_.block_ = next_block;
                        break;
                    }
                    auto next_group = high_perform_buffer_.next(rt_cache_.current_reading_.group_, group_list::lock_type::no_lock);
                    optimize_memory_for_block(nullptr, current_epoch_ms);
                    optimize_memory_for_group(next_group, current_epoch_ms);
                    rt_cache_.current_reading_.group_ = next_group;
                    rt_cache_.current_reading_.block_ = nullptr;
                    if (!rt_cache_.current_reading_.group_)
                    {
                        // try normal_buffer_.
                        next_data_source_found = true;
                        optimize_memory_end();
                        break;
                    }
                } else {
#if BQ_LOG_BUFFER_DEBUG
                    assert(!rt_cache_.current_reading_.block_);
#endif
                    rt_cache_.current_reading_.group_ = high_perform_buffer_.first(group_list::lock_type::no_lock);
                    if (!rt_cache_.current_reading_.group_) {
                        next_data_source_found = true;
                        break;
                    }
                    optimize_memory_begin(current_epoch_ms);
                    optimize_memory_for_group(rt_cache_.current_reading_.group_, current_epoch_ms);
                }
            }
        } while (rt_cache_.current_reading_.block_ != start_node);

        log_buffer_read_handle empty_handle;
        empty_handle.result = enum_buffer_result_code::err_empty_ring_buffer;
        empty_handle.data_addr = nullptr;
        empty_handle.data_size = 0;
        return empty_handle;
    }

    void miso_high_perform_buffer::return_read_trunk(const log_buffer_read_handle& handle)
    {
        if (rt_cache_.current_reading_.block_) {
            rt_cache_.current_reading_.block_->get_buffer().return_read_trunk(handle);
        }else{
            normal_buffer_.return_read_trunk(handle);
        }
    }


    void miso_high_perform_buffer::set_thread_check_enable(bool in_enable)
    {
#if BQ_LOG_BUFFER_DEBUG
        check_thread_ = in_enable;
        if (!check_thread_) {
            read_thread_id_ = empty_thread_id_;
        }
        normal_buffer_.set_thread_check_enable(check_thread_);
#else
        (void)in_enable;
#endif
    }

#if BQ_JAVA
    java_buffer_info miso_high_perform_buffer::get_java_buffer_info(JavaVM* jvm, const log_buffer_write_handle& handle)
    {
    }

#endif


    bq_forceinline block_node_head* miso_high_perform_buffer::alloc_new_high_performance_block()
    {
        block_node_head* result = high_perform_buffer_.alloc_new_block();
#if BQ_WIN
        auto& misc_data = result->get_misc_data<block_misc_data>();
        misc_data.thread_id_ = bq::platform::thread::get_current_thread_id();
        misc_data.last_update_epoch_ms_ = 0;
        misc_data.thread_handle_ = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, misc_data.thread_id_);
        if (!misc_data.thread_handle_)
        {
            // compatible to windows earlier than Windows Vista.
            misc_data.thread_handle_ = OpenThread(THREAD_QUERY_INFORMATION, FALSE, misc_data.thread_id_);
        }
#elif BQ_POSIX
        if (!is_pthread_destructor_registered_) {
            pthread_setspecific(hp_buffer_pthread_key_, (void*)this);
            is_pthread_destructor_registered_ = true;
        }
#endif
    }

    void miso_high_perform_buffer::optimize_memory_begin(uint64_t current_epoch_ms)
    {
        rt_cache_.mem_optimize_.last_block_ = nullptr;
        rt_cache_.mem_optimize_.last_group_ = group_list::iterator();
        rt_cache_.mem_optimize_.left_holes_num_ = 0;
        rt_cache_.mem_optimize_.cur_group_using_blocks_num_ = 0;
        rt_cache_.mem_optimize_.is_block_marked_removed = false;
    }
    
    void miso_high_perform_buffer::optimize_memory_for_group(const group_list::iterator& next_group, uint64_t current_epoch_ms)
    {
        (void)current_epoch_ms;
#if BQ_LOG_BUFFER_DEBUG
        assert(rt_cache_.mem_optimize_.cur_group_using_blocks_num_ <= high_perform_buffer_.get_max_block_count_per_group());
#endif
        if (rt_cache_.mem_optimize_.cur_group_using_blocks_num_ == 0) {
            //try lock 
            group_list::iterator current_lock_iterator;
            if (rt_cache_.mem_optimize_.last_group_) {
                current_lock_iterator = high_perform_buffer_.next(rt_cache_.mem_optimize_.last_group_, group_list::lock_type::write_lock);
                assert(current_lock_iterator == rt_cache_.current_reading_.group_ && "try lock removing group failed");
            } else {
                // must lock from head, because new group node is inserted after head pointer from other threads.
                while (current_lock_iterator != rt_cache_.current_reading_.group_) {
                    current_lock_iterator = (bool)current_lock_iterator 
                        ? high_perform_buffer_.next(current_lock_iterator, group_list::lock_type::write_lock)
                        : high_perform_buffer_.first(group_list::lock_type::write_lock);
                    if (!current_lock_iterator) {
                        assert(false && "try lock removing group from head failed");
                    }
                }
            }
            if ((!current_lock_iterator.value().get_data().used_.first())
                && (!current_lock_iterator.value().get_data().stage_.first())) {
                //remove it
                high_perform_buffer_.delete_and_unlock_node_thread_unsafe(current_lock_iterator);
            } else {
                //unlock
                high_perform_buffer_.next(current_lock_iterator, group_list::lock_type::no_lock);
            }
        }

        rt_cache_.mem_optimize_.left_holes_num_ += high_perform_buffer_.get_max_block_count_per_group() - rt_cache_.mem_optimize_.cur_group_using_blocks_num_;
        rt_cache_.mem_optimize_.cur_group_using_blocks_num_ = 0;

        rt_cache_.mem_optimize_.last_group_ = rt_cache_.current_reading_.group_;
    }

    void miso_high_perform_buffer::optimize_memory_for_block(block_node_head* next_block, uint64_t current_epoch_ms)
    {
        if (rt_cache_.current_reading_.block_) {
            if (rt_cache_.mem_optimize_.is_block_marked_removed) {
                //remove it
                rt_cache_.current_reading_.group_.value().get_data().used_.remove_thread_unsafe(rt_cache_.mem_optimize_.last_block_, rt_cache_.current_reading_.block_);
                rt_cache_.current_reading_.group_.value().get_data().free_.push(rt_cache_.current_reading_.block_);
            } else {
                ++rt_cache_.mem_optimize_.cur_group_using_blocks_num_;
            }
        }

        if (next_block) {
            rt_cache_.mem_optimize_.is_block_marked_removed = is_block_removed(next_block);
#if BQ_WIN
            //check thread alive
            if (!rt_cache_.mem_optimize_.is_block_marked_removed) {
                auto& misc_data = next_block->get_misc_data<block_misc_data>();
                if (misc_data.last_update_epoch_ms_ + BLOCK_THREAD_VALID_CHECK_THRESHOLD < current_epoch_ms) {
                    DWORD exit_code = STILL_ACTIVE;
                    if (GetExitCodeThread(misc_data.thread_handle_, &exit_code)) {
                        if (exit_code != STILL_ACTIVE) {
                            rt_cache_.mem_optimize_.is_block_marked_removed = true;
                            CloseHandle(misc_data.thread_handle_);
                        }
                    }
                }
            }
#endif
            if (rt_cache_.mem_optimize_.left_holes_num_ > 0) {
                mark_block_need_reallocate(next_block, true);
                --rt_cache_.mem_optimize_.left_holes_num_;
            }
        }
        rt_cache_.mem_optimize_.last_block_ = rt_cache_.current_reading_.block_;
    }

    void miso_high_perform_buffer::optimize_memory_end()
    {
        rt_cache_.mem_optimize_.last_block_ = nullptr;
        rt_cache_.mem_optimize_.last_group_ = group_list::iterator();
        rt_cache_.mem_optimize_.left_holes_num_ = 0;
        rt_cache_.mem_optimize_.cur_group_using_blocks_num_ = 0;
        rt_cache_.mem_optimize_.is_block_marked_removed = false;
    }
 }
