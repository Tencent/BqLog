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

    bq_forceinline static void mark_block_removed(block_node_head* block, bool removed)
    {
        BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(block->get_misc_data<log_buffer::block_misc_data>().is_removed_, bool).store_release(removed);
    }
    bq_forceinline static bool is_block_removed(block_node_head* block)
    {
        bool& is_removed_variable = block->get_misc_data<log_buffer::block_misc_data>().is_removed_;
        if (is_removed_variable) {
            // inter thread sync
            if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(is_removed_variable, bool).load_acquire()) {
                return true;
            }
        }
        return false;
    }
    bq_forceinline static void mark_block_need_reallocate(block_node_head* block, bool need_reallocate)
    {
        block->get_misc_data<log_buffer::block_misc_data>().need_reallocate_ = need_reallocate;
    }
    bq_forceinline static bool is_block_need_reallocate(block_node_head* block)
    {
        return block->get_misc_data<log_buffer::block_misc_data>().need_reallocate_;
    }

    log_buffer::log_tls_buffer_info::~log_tls_buffer_info()
    {
#if BQ_JAVA
        if (buffer_obj_for_lp_buffer_
            || buffer_obj_for_hp_buffer_) {
            bq::platform::jni_env env;
            if (buffer_obj_for_lp_buffer_) {
                env.env->DeleteGlobalRef(buffer_obj_for_lp_buffer_);
                buffer_obj_for_lp_buffer_ = NULL;
            }
            if (buffer_obj_for_hp_buffer_) {
                env.env->DeleteGlobalRef(buffer_obj_for_hp_buffer_);
                buffer_obj_for_hp_buffer_ = NULL;
            }
        }
#endif
    }

    log_buffer::log_tls_info::~log_tls_info()
    {
        if (log_map_) {
            for (auto pair : *log_map_) {
                {
                    bq::platform::scoped_spin_lock lock(pair.value()->destruction_mark_->lock_);
                    //make sure log_buffer obj is still alive.
                    if (!pair.value()->destruction_mark_->is_destructed_) {
                        if (pair.value()->cur_block_) {
                            pair.value()->cur_block_->get_misc_data<log_buffer::block_misc_data>().context_.is_thread_finished_ = true;
                            mark_block_removed(pair.value()->cur_block_, true);
                        } else {
                            while (true) {
                                auto finish_mark_handle = pair.value()->buffer_->lp_buffer_.alloc_write_chunk(sizeof(context_head*));
                                bq::scoped_log_buffer_handle scoped_handle(pair.value()->buffer_->lp_buffer_, finish_mark_handle);
                                if (enum_buffer_result_code::success == finish_mark_handle.result) {
                                    context_head* context = (context_head*)(finish_mark_handle.data_addr);
                                    context->version_ = pair.value()->buffer_->version_;
                                    context->set_tls_info(pair.value());
                                    context->is_thread_finished_ = true;
                                    context->seq_ = pair.value()->wt_data_.current_write_seq_++;
                                    break;
                                }
                            }
                        }
                    }
                }
                delete pair.value();
            }
            // the entries in the map will be destructed by consumer thread of log_buffer.
            delete log_map_;
            log_map_ = nullptr;
        }
        cur_log_buffer_id_ = 0;
        cur_buffer_info_ = nullptr;
    }

    static thread_local log_buffer::log_tls_info log_tls_info_;


    log_buffer::log_buffer(log_buffer_config& config)
        : config_(config)
        , hp_buffer_(config, BLOCKS_PER_GROUP_NODE)
        , lp_buffer_(config)
        , version_(config.need_recovery ? ++lp_buffer_.get_mmap_misc_data<lp_buffer_head_misc>().saved_version_ : 0)
        , destruction_mark_(bq::make_shared_for_overwrite<destruction_mark>())
    {
        static bq::platform::atomic<uint64_t> id_generator(0);
        id_ = id_generator.add_fetch_relaxed(1);
        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));
        if (config.need_recovery) {
            rt_cache_.current_reading_.version_ = static_cast<uint16_t>(version_ - MAX_RECOVERY_VERSION_RANGE);
            prepare_and_fix_recovery_data();
        } else {
            assert(lp_buffer_.get_mmap_misc_data<lp_buffer_head_misc>().saved_version_ == 0 && "invalid lp_buffer_head_misc init value");
            rt_cache_.current_reading_.version_ = 0;
        }

        block_node_head::alignment_assert();

        assert((BQ_POD_RUNTIME_OFFSET_OF(log_tls_buffer_info, wt_data_) % CACHE_LINE_SIZE == 0) && "log_tls_buffer_info current_write_seq_ must be 64 bytes aligned");
        assert((BQ_POD_RUNTIME_OFFSET_OF(log_tls_buffer_info, rt_data_) % CACHE_LINE_SIZE == 0) && "log_tls_buffer_info current_read_seq_ must be 64 bytes aligned");
        
    }

    log_buffer::~log_buffer()
    {
        bq::platform::scoped_spin_lock lock(destruction_mark_->lock_);
        destruction_mark_->is_destructed_ = true;
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
                    block_cache = alloc_new_hp_block();
                } else if (is_block_need_reallocate(block_cache)) {
                    mark_block_removed(block_cache, true); // mark removed
                    block_cache = alloc_new_hp_block();
                }
                result = block_cache->get_buffer().alloc_write_chunk(size);
                if (enum_buffer_result_code::err_not_enough_space == result.result) {
                    switch (config_.policy) {
                    case log_memory_policy::auto_expand_when_full:
                        mark_block_removed(block_cache, true); // mark removed
                        block_cache = alloc_new_hp_block();
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
                result = lp_buffer_.alloc_write_chunk(size + sizeof(context_head));
                if (enum_buffer_result_code::success == result.result) {
                    context_head* context = (context_head*)(result.data_addr);
                    context->version_ = version_;
                    context->is_thread_finished_ = false;
                    context->seq_ = tls_buffer.wt_data_.current_write_seq_++;
                    context->set_tls_info(&tls_buffer);
                    result.data_addr += sizeof(context_head);
                } else if (enum_buffer_result_code::err_not_enough_space == result.result) {
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
        auto& tls_buffer_info = log_tls_info_.get_buffer_info_directly(this);
        block_node_head*& block_cache = tls_buffer_info.cur_block_;
        bool is_high_frequency = block_cache;
        if (is_high_frequency) {
            block_cache->get_buffer().commit_write_chunk(handle);
        } else {
            log_buffer_write_handle handle_copy = handle;
            handle_copy.data_addr -= sizeof(context_head);
            lp_buffer_.commit_write_chunk(handle_copy);
        }
    }

    log_buffer_read_handle log_buffer::read_chunk()
    {
        bool lp_visited = false;
        bool full_visited = false;
        while (!full_visited) {
            // try read from current data source
            if (rt_cache_.current_reading_.block_) {
#if BQ_LOG_BUFFER_DEBUG
                assert(rt_cache_.current_reading_.group_);
#endif
                auto handle = rt_cache_.current_reading_.block_->get_buffer().read_chunk();
                if (enum_buffer_result_code::err_empty_log_buffer != handle.result) {
                    return handle;
                } else {
                    rt_cache_.current_reading_.block_->get_buffer().return_read_trunk(handle);
                }
            } else {
#if BQ_LOG_BUFFER_DEBUG
                assert(!rt_cache_.current_reading_.group_);
#endif
                bool veryfied_chunk_read = false;
                bq::log_buffer_read_handle handle;
                while (!veryfied_chunk_read) {
                    handle = rt_cache_.current_reading_.lp_snapshot_.result == enum_buffer_result_code::err_empty_log_buffer
                        ? lp_buffer_.read_chunk()
                        : rt_cache_.current_reading_.lp_snapshot_;
                    rt_cache_.current_reading_.lp_snapshot_.result = enum_buffer_result_code::err_empty_log_buffer;
                    if (enum_buffer_result_code::success == handle.result) {
                        const auto& context = *reinterpret_cast<const context_head*>(handle.data_addr);
                        auto verify_result = verify_context(context);
                        switch (verify_result) {
                        case context_verify_result::valid:
                            if (context.is_thread_finished_) {
                                deregister_seq(context);
                            } else {
                                veryfied_chunk_read = true;
                            }
                            break;
                        case context_verify_result::version_invalid:
                            // invalid block generated by mmap.
                            break;
                        case context_verify_result::version_waiting:
                        case context_verify_result::seq_pendding:
                            rt_cache_.current_reading_.lp_snapshot_ = handle;
                            handle.result = enum_buffer_result_code::err_empty_log_buffer;
                            veryfied_chunk_read = true;
                            break;
                        default:
                            break;
                        }
                        if (!veryfied_chunk_read) {
                            lp_buffer_.return_read_trunk(handle);
                        } else {
                            handle.data_addr += sizeof(context_head);
                            handle.data_size -= (uint32_t)sizeof(context_head);
                        }
                    } else {
                        veryfied_chunk_read = true;
                    }
                }
                if (enum_buffer_result_code::err_empty_log_buffer != handle.result) {
                    return handle;
                } else {
                    lp_buffer_.return_read_trunk(handle);
                }
            }
            // find next data source
            bool next_data_source_found = false;
            while (!next_data_source_found) {
                if (rt_cache_.current_reading_.group_) {
                    block_node_head* next_block = rt_cache_.current_reading_.block_
                        ? rt_cache_.current_reading_.group_.value().get_data_head().used_.next(rt_cache_.current_reading_.block_)
                        : rt_cache_.current_reading_.group_.value().get_data_head().used_.first();

                    if (!next_block) {
                        next_block = rt_cache_.current_reading_.group_.value().get_data_head().stage_.pop();
                        if (next_block) {
                            rt_cache_.current_reading_.group_.value().get_data_head().used_.push_after_thread_unsafe(rt_cache_.current_reading_.block_, next_block);
                        }
                    }

                    if (next_block) {
                        auto& context = next_block->get_misc_data<block_misc_data>().context_;
                        auto verify_result = verify_context(context);
                        switch (verify_result) {
                        case context_verify_result::valid:
                            next_data_source_found = true;
                            break;
                        case context_verify_result::version_invalid:
                            mark_block_removed(next_block, true); // invalid block generated by mmap.
                            break;
                        case context_verify_result::version_waiting:
                        case context_verify_result::seq_pendding:
                            break;
                        default:
                            break;
                        }
                        set_current_reading_block(next_block, verify_result);
                        continue;
                    }

                    auto next_group = hp_buffer_.next(rt_cache_.current_reading_.group_, group_list::lock_type::no_lock);
                    set_current_reading_block(nullptr, context_verify_result::version_invalid);
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
                        if (rt_cache_.current_reading_.version_ != version_) {
                            ++rt_cache_.current_reading_.version_;
                        } else {
                            full_visited = true;
                            break;
                        }
                    }
                    hp_buffer_.garbage_collect();
                    lp_visited = true;
                    auto next_group = hp_buffer_.first(group_list::lock_type::no_lock);
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
            if (enum_buffer_result_code::success == handle.result) {
                log_buffer_read_handle handle_cpy = handle;
                handle_cpy.data_addr -= sizeof(context_head);
                handle_cpy.data_size += (uint32_t)sizeof(context_head);
                const auto& context = *reinterpret_cast<const context_head*>(handle_cpy.data_addr);
                deregister_seq(context);
                lp_buffer_.return_read_trunk(handle_cpy);
            } else {
                lp_buffer_.return_read_trunk(handle);
            }
        }
    }

#if BQ_JAVA
    log_buffer::java_buffer_info log_buffer::get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle)
    {
#if BQ_LOG_BUFFER_DEBUG
        assert((this->id_ == log_tls_info_.cur_log_buffer_id_) && "tls cur_log_buffer_ check failed");
#endif
        auto& current_buffer_info = log_tls_info_.get_buffer_info_directly(this);
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
            if (current_buffer_info.buffer_ref_block_ != current_buffer_info.cur_block_) {
                env->SetObjectArrayElement(current_buffer_info.buffer_obj_for_hp_buffer_, 0, env->NewDirectByteBuffer(const_cast<uint8_t*>(ring_buffer.get_buffer_addr()), (jlong)(ring_buffer.get_block_size() * ring_buffer.get_total_blocks_count())));
                current_buffer_info.buffer_ref_block_ = current_buffer_info.cur_block_;
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

    bq::block_node_head* log_buffer::alloc_new_hp_block()
    {
        log_tls_buffer_info& tls_buffer_info = log_tls_info_.get_buffer_info_directly(this);
        block_misc_data misc_data;
        memset(&misc_data, 0, sizeof(misc_data));
        misc_data.context_.set_tls_info(&tls_buffer_info);
        misc_data.context_.version_ = version_;
        misc_data.context_.is_thread_finished_ = false;
        misc_data.context_.seq_ = tls_buffer_info.wt_data_.current_write_seq_++;
        auto new_node = hp_buffer_.alloc_new_block(&misc_data, sizeof(block_misc_data));
        return new_node;
    }

    bq::log_buffer::context_verify_result log_buffer::verify_context(const context_head& context)
    {
        if (context.version_ == rt_cache_.current_reading_.version_) {
            //This is the most common scenario. When uncertain about CPU out-of-order execution and branch prediction, placing the code upfront ensures efficiency.
            if (context.version_ == version_) {
                //for new data
                if (context.seq_ == context.get_tls_info()->rt_data_.current_read_seq_) {
                    return context_verify_result::valid;
                } 
                return context_verify_result::seq_pendding;
            } else {
                //for recovering
                auto iter = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)].find(context.get_tls_info());
#if BQ_LOG_BUFFER_DEBUG
                assert((iter != rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)].end()) && "unknowing thread found in recovering");
#endif
                if (iter->value() == context.seq_) {
                    return context_verify_result::valid;
                }
                return context_verify_result::seq_pendding;
            }
        }else if (static_cast<uint16_t>(version_ - context.version_) > static_cast<uint16_t>(version_ - rt_cache_.current_reading_.version_)) {
            // need discard.
            return context_verify_result::version_invalid;
        }
        return context_verify_result::version_waiting;
        
    }

    void log_buffer::deregister_seq(const context_head& context)
    {
#if BQ_LOG_BUFFER_DEBUG
        assert(context.version_ == rt_cache_.current_reading_.version_ && "invalid deregister_version");
#endif
        if (context.version_ == version_) {
            // for new data
            if (context.is_thread_finished_) {
                delete context.get_tls_info();
            }
            ++context.get_tls_info()->rt_data_.current_read_seq_;
        } else {
            // for recovering data
            auto& recover_map = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)];
#if BQ_LOG_BUFFER_DEBUG
            assert((recover_map.find(context.get_tls_info()) != recover_map.end()) && "unregister none exist seq");
#endif
            ++recover_map[context.get_tls_info()];
        }
    }

    void log_buffer::set_current_reading_group(const group_list::iterator& group)
    {
        rt_cache_.current_reading_.group_ = group;
        optimize_memory_for_group(group);
    }

    void log_buffer::set_current_reading_block(block_node_head* block, context_verify_result verify_result)
    {
        rt_cache_.current_reading_.block_ = block;
        optimize_memory_for_block(block, verify_result);
    }

    void log_buffer::optimize_memory_for_group(const group_list::iterator& group)
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
#if BQ_LOG_BUFFER_DEBUG
        assert(mem_opt.cur_group_using_blocks_num_ <= hp_buffer_.get_max_block_count_per_group());
#endif
        bool group_removed = false;
        if (mem_opt.cur_group_using_blocks_num_ == 0 && mem_opt.cur_group_) {
            // try lock
            group_list::iterator current_lock_iterator;
            if (mem_opt.last_group_) {
                current_lock_iterator = hp_buffer_.next(mem_opt.last_group_, group_list::lock_type::write_lock);
            } else {
                // must lock from head, because new group node is inserted after head pointer from other threads.
                while (current_lock_iterator != mem_opt.cur_group_) {
                    current_lock_iterator = (bool)current_lock_iterator
                        ? hp_buffer_.next(current_lock_iterator, group_list::lock_type::write_lock)
                        : hp_buffer_.first(group_list::lock_type::write_lock);
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
                hp_buffer_.delete_and_unlock_thread_unsafe(current_lock_iterator);
            } else {
                // unlock
                hp_buffer_.next(current_lock_iterator, group_list::lock_type::no_lock);
            }
        }
        
        if (mem_opt.cur_group_) {
            mem_opt.left_holes_num_ += hp_buffer_.get_max_block_count_per_group() - rt_cache_.mem_optimize_.cur_group_using_blocks_num_;
        }
        mem_opt.cur_group_using_blocks_num_ = 0;
        if (!group_removed) {
            mem_opt.last_group_ = mem_opt.cur_group_;
        }
        mem_opt.cur_group_ = group;
        if (!group) {
            rt_cache_.mem_optimize_.left_holes_num_ = 0;
        }
    }

    void log_buffer::optimize_memory_for_block(block_node_head* block, context_verify_result verify_result)
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
        bool block_removed = false;
        if (mem_opt.cur_block_) {
            if (mem_opt.is_block_marked_removed) {
                const auto& context = mem_opt.cur_block_->get_misc_data<block_misc_data>().context_;
                if (context_verify_result::valid == mem_opt.verify_result) {
                    deregister_seq(context);
                }
                // remove it
                if (mem_opt.cur_group_.value().get_data_head().used_.is_include(mem_opt.last_block_)) {
                    hp_buffer_.recycle_block_thread_unsafe(mem_opt.cur_group_, mem_opt.last_block_, mem_opt.cur_block_);
                } else {
                    hp_buffer_.recycle_block_thread_unsafe(mem_opt.cur_group_, nullptr, mem_opt.cur_block_);
                }
                block_removed = true;
            } else {
                ++mem_opt.cur_group_using_blocks_num_;
            }
        }

        if (block) {
            mem_opt.is_block_marked_removed = (verify_result == context_verify_result::valid || verify_result == context_verify_result::version_invalid) && is_block_removed(block);
            mem_opt.verify_result = verify_result;
            if (mem_opt.left_holes_num_ > 0) {
                mark_block_need_reallocate(block, true);
                --mem_opt.left_holes_num_;
            }
        } else {
            mem_opt.is_block_marked_removed = false;
            mem_opt.verify_result = verify_result;
        }
        if (!block_removed) {
            mem_opt.last_block_ = mem_opt.cur_block_;
        }
        mem_opt.cur_block_ = block;
    }

    /**
     * @brief Fixes mmap data errors by invalidating data with the same version as the current run.
     *
     * Although both lp_buffer_ and hp_buffer_ have their own mmap recovery error correction capabilities,
     * log_buffer needs to handle some logical layer error corrections itself. This function primarily
     * addresses mmap saved data errors where data with the same version as the current run appears.
     * Such data needs to be invalidated.
     */
    void log_buffer::prepare_and_fix_recovery_data()
    {
        for (uint32_t i = 0; i < MAX_RECOVERY_VERSION_RANGE; ++i) {
            rt_cache_.current_reading_.recovery_records_.push_back(decltype(rt_cache_.current_reading_.recovery_records_)::value_type());
        }
        lp_buffer_.set_thread_check_enable(false);
        lp_buffer_.data_traverse([](uint8_t* data, uint32_t size, void* user_data) {
            (void)size;
            log_buffer& buffer = *reinterpret_cast<log_buffer*>(user_data);
            context_head& context = *reinterpret_cast<context_head*>(data);
            if (context.version_ == buffer.version_) {
                context.version_++;  //mark invalid
            } else if (static_cast<uint16_t>(buffer.version_ - 1 - context.version_) < MAX_RECOVERY_VERSION_RANGE) {
                auto& recovery_map = buffer.rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(buffer.version_ - 1 - context.version_)];
                auto iter = recovery_map.find(context.get_tls_info());
                if (iter == recovery_map.end()) {
                    recovery_map[context.get_tls_info()] = context.seq_;
                } else {
                    iter->value() = bq::min_value(iter->value(), context.seq_);
                }
            }
        }, this);
        lp_buffer_.set_thread_check_enable(true);
        
        auto group = hp_buffer_.first(group_list::lock_type::no_lock);
        while (group) {
            auto block = group.value().get_data_head().used_.first();
            while (block) {
                auto& context = block->get_misc_data<block_misc_data>().context_;
                if (context.version_ == version_) {
                    context.version_++; // mark invalid
                } else if (static_cast<uint16_t>(version_ - 1 - context.version_) < MAX_RECOVERY_VERSION_RANGE) {
                    auto& recovery_map = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_) < MAX_RECOVERY_VERSION_RANGE];
                    auto iter = recovery_map.find(context.get_tls_info());
                    if (iter == recovery_map.end()) {
                        recovery_map[context.get_tls_info()] = context.seq_;
                    } else {
                        iter->value() = bq::min_value(iter->value(), context.seq_);
                    }
                }
                block = group.value().get_data_head().used_.next(block);
            }
            block = group.value().get_data_head().stage_.first();
            while (block) {
                auto& context = block->get_misc_data<block_misc_data>().context_;
                if (context.version_ == version_) {
                    context.version_++; // mark invalid
                } else if (static_cast<uint16_t>(version_ - 1 - context.version_) < MAX_RECOVERY_VERSION_RANGE) {
                    auto& recovery_map = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)];
                    auto iter = recovery_map.find(context.get_tls_info());
                    if (iter == recovery_map.end()) {
                        recovery_map[context.get_tls_info()] = context.seq_;
                    } else {
                        iter->value() = bq::min_value(iter->value(), context.seq_);
                    }
                }
                block = group.value().get_data_head().stage_.next(block);
            }
            group = hp_buffer_.next(group, group_list::lock_type::no_lock);
        }
    }

}
