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
 * \class bq::log_buffer
 *
 * \author pippocao
 * \date 2024/12/06
 */

#include "bq_log/types/buffer/log_buffer.h"
#include "bq_log/types/buffer/block_list.h"

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
#if defined(BQ_JAVA)
        if (java_.buffer_obj_for_lp_buffer_
            || java_.buffer_obj_for_hp_buffer_
            || java_.buffer_obj_for_oversize_buffer_) {
            bq::platform::jni_env env;
            if (java_.buffer_obj_for_lp_buffer_) {
                env.env->DeleteGlobalRef(java_.buffer_obj_for_lp_buffer_);
                java_.buffer_obj_for_lp_buffer_ = NULL;
            }
            if (java_.buffer_obj_for_hp_buffer_) {
                env.env->DeleteGlobalRef(java_.buffer_obj_for_hp_buffer_);
                java_.buffer_obj_for_hp_buffer_ = NULL;
            }
            if (java_.buffer_obj_for_oversize_buffer_) {
                env.env->DeleteGlobalRef(java_.buffer_obj_for_oversize_buffer_);
                java_.buffer_obj_for_oversize_buffer_ = NULL;
            }
        }
#endif
    }

    log_buffer::log_tls_info::~log_tls_info()
    {
        if (log_map_) {
            for (auto pair : *log_map_) {
                bq::shared_ptr<destruction_mark> destruction_protector = pair.value()->destruction_mark_;
                bq::platform::scoped_spin_lock lock(pair.value()->destruction_mark_->lock_);
                // make sure log_buffer obj is still alive.
                if (!pair.value()->destruction_mark_->is_destructed_) {
                    if (pair.value()->cur_block_) {
                        pair.value()->cur_block_->get_misc_data<log_buffer::block_misc_data>().context_.is_thread_finished_ = true;
                        mark_block_removed(pair.value()->cur_block_, true);
                    } else {
                        while (true) {
                            auto finish_mark_handle = pair.value()->buffer_->lp_buffer_.alloc_write_chunk(sizeof(context_head));
                            bq::scoped_log_buffer_handle<miso_ring_buffer> scoped_handle(pair.value()->buffer_->lp_buffer_, finish_mark_handle);
                            if (enum_buffer_result_code::success == finish_mark_handle.result) {
                                auto* context = reinterpret_cast<context_head*>(finish_mark_handle.data_addr);
                                context->version_ = pair.value()->buffer_->version_;
                                context->set_tls_info(pair.value());
                                context->is_thread_finished_ = true;
                                context->seq_ = pair.value()->wt_data_.current_write_seq_++;
                                context->is_external_ref_ = false;
                                break;
                            }
                        }
                    }
                } else {
                    bq::util::aligned_delete(pair.value());
                }
            }
            // the entries in the map will be destructed by consumer thread of log_buffer.
            delete log_map_;
            log_map_ = nullptr;
        }
        cur_log_buffer_id_ = 0;
        cur_buffer_info_ = nullptr;
    }

    BQ_TLS_NON_POD(log_buffer::log_tls_info, log_tls_info_)

    log_buffer::log_buffer(log_buffer_config& config)
        : config_(config)
        , hp_buffer_(config_, BLOCKS_PER_GROUP_NODE)
        , lp_buffer_(config_)
        , version_(config_.need_recovery ? ++lp_buffer_.get_mmap_misc_data<lp_buffer_head_misc>().saved_version_ : 0)
        , destruction_mark_(bq::make_shared_for_overwrite<destruction_mark>())
        , current_oversize_buffer_index_(0)
    {
        static bq::platform::atomic<uint64_t> id_generator(0);
        id_ = id_generator.add_fetch_relaxed(1);
        const_cast<log_buffer_config&>(config_).default_buffer_size = bq::max_value((uint32_t)(16 * bq::BQ_CACHE_LINE_SIZE), bq::roundup_pow_of_two(config_.default_buffer_size));
        if (config.need_recovery) {
            rt_cache_.current_reading_.version_ = static_cast<uint16_t>(version_ - MAX_RECOVERY_VERSION_RANGE);
            prepare_and_fix_recovery_data();
        } else {
            assert(lp_buffer_.get_mmap_misc_data<lp_buffer_head_misc>().saved_version_ == 0 && "invalid lp_buffer_head_misc init value");
            rt_cache_.current_reading_.version_ = 0;
            clear_recovery_data();
        }

        block_node_head::alignment_assert();

        assert((BQ_POD_RUNTIME_OFFSET_OF(log_tls_buffer_info, wt_data_) % BQ_CACHE_LINE_SIZE == 0) && "log_tls_buffer_info current_write_seq_ must be 64 bytes aligned");
        assert((BQ_POD_RUNTIME_OFFSET_OF(log_tls_buffer_info, rt_data_) % BQ_CACHE_LINE_SIZE == 0) && "log_tls_buffer_info current_read_seq_ must be 64 bytes aligned");
    }

    log_buffer::~log_buffer()
    {
        bq::platform::scoped_spin_lock lock(destruction_mark_->lock_);
        destruction_mark_->is_destructed_ = true;
    }

    log_buffer_write_handle log_buffer::alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms)
    {
        auto& tls_buffer = log_tls_info_.get().get_buffer_info(this);

        block_node_head*& block_cache = tls_buffer.cur_block_;
        uint64_t& thread_last_update_epoch_ms = tls_buffer.last_update_epoch_ms_;
        uint64_t& thread_update_times = tls_buffer.update_times_;

        // frequency check
        bool is_high_frequency = (bool)block_cache;
        BQ_UNLIKELY_IF(current_epoch_ms >= thread_last_update_epoch_ms + HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL)
        {
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
                BQ_UNLIKELY_IF(!block_cache)
                {
                    block_cache = alloc_new_hp_block();
                }
                else BQ_UNLIKELY_IF(is_block_need_reallocate(block_cache))
                {
                    mark_block_removed(block_cache, true); // mark removed
                    block_cache = alloc_new_hp_block();
                }
                result = block_cache->get_buffer().alloc_write_chunk(size);
                if (enum_buffer_result_code::err_not_enough_space == result.result) {
                    switch (config_.policy) {
                    case log_memory_policy::auto_expand_when_full:
                        // discard result and switch to high frequency mode and try again
                        block_cache->get_buffer().commit_write_chunk(result);
                        mark_block_removed(block_cache, true); // mark removed
                        block_cache = alloc_new_hp_block();
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
                result = lp_buffer_.alloc_write_chunk(size + static_cast<uint32_t>(sizeof(context_head)));
                if (enum_buffer_result_code::success == result.result) {
                    auto* context = reinterpret_cast<context_head*>(result.data_addr);
                    context->version_ = version_;
                    context->is_thread_finished_ = false;
                    context->seq_ = tls_buffer.wt_data_.current_write_seq_++;
                    context->is_external_ref_ = false;
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

        // For chunks larger than lp_buffer size and hp_buffer size.
        BQ_UNLIKELY_IF(enum_buffer_result_code::err_alloc_size_invalid == result.result && size > 0)
        {
            return wt_alloc_oversize_write_chunk(size, current_epoch_ms);
        }
        return result;
    }

    void log_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        auto& tls_buffer_info = log_tls_info_.get().get_buffer_info_directly(this);
        block_node_head*& block_cache = tls_buffer_info.cur_block_;
        bool is_high_frequency = (block_cache != nullptr);
        if (is_high_frequency) {
            block_cache->get_buffer().commit_write_chunk(handle);
        } else {
            if (handle.result == enum_buffer_result_code::success) {
                log_buffer_write_handle handle_copy = handle;
                handle_copy.data_addr -= sizeof(context_head);
                auto* context = reinterpret_cast<context_head*>(handle_copy.data_addr);
                bool is_external_ref_ = context->is_external_ref_ && handle.result == enum_buffer_result_code::success;
                BQ_UNLIKELY_IF(is_external_ref_)
                {
                    wt_commit_oversize_write_chunk(handle_copy);
                }
                else
                {
                    lp_buffer_.commit_write_chunk(handle_copy);
                }
            } else {
                lp_buffer_.commit_write_chunk(handle);
            }
        }
    }

    log_buffer_read_handle log_buffer::read_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (empty_thread_id_ == read_thread_id_) {
            read_thread_id_ = bq::platform::thread::get_current_thread_id();
        }
        bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
        assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for log_buffer!");
#endif
        auto& rt_reading = rt_cache_.current_reading_;
        if (rt_reading.hp_handle_cache_.result == enum_buffer_result_code::success) {
            if (rt_reading.hp_handle_cache_.has_next()) {
                return rt_reading.hp_handle_cache_.next();
            } else {
                rt_reading.hp_handle_cache_.result = enum_buffer_result_code::err_empty_log_buffer;
            }
        }
        log_buffer_read_handle read_handle;
        context_verify_result verify_result = context_verify_result::version_invalid;
        bool loop_finished = false;
        if (rt_cache_.mem_optimize_.is_block_marked_removed && rt_reading.cur_block_) {
            rt_reading.traverse_end_block_is_working_ = false;
        } else {
            rt_reading.traverse_end_block_is_working_ = true;
            rt_reading.traverse_end_block_ = rt_reading.cur_block_;
        }
        if (rt_reading.last_block_ == rt_reading.cur_block_) {
            assert(!rt_reading.last_block_);
        }
        // Principle 1 : If the currently processed block or buffer equals traverse_end_block_, and traverse_end_block_is_working_ is true, it means one full traversal loop has been completed.
        // Principle 2 : If, after reaching the latest version and completing a full traversal loop, no data is read, it means there is truly no data available.
        while (!loop_finished) {
            switch (rt_reading.state_) {
            case read_state::lp_buffer_reading:
                if (rt_read_from_lp_buffer(read_handle)) {
                    loop_finished = true;
                    read_handle.data_addr += sizeof(context_head);
                    read_handle.data_size -= static_cast<uint32_t>(sizeof(context_head));
                    break;
                }
                rt_reading.state_ = read_state::next_group_finding;
                rt_recycle_oversize_buffers();
                break;
            case read_state::hp_block_reading:
                rt_reading.hp_handle_cache_ = rt_reading.cur_block_->get_buffer().batch_read();
                if (enum_buffer_result_code::success == rt_reading.hp_handle_cache_.result) {
                    read_handle = rt_reading.hp_handle_cache_.next();
                    loop_finished = true;
                    break;
                }
                rt_reading.cur_block_->get_buffer().return_batch_read_chunks(rt_reading.hp_handle_cache_);
                rt_reading.state_ = read_state::next_block_finding;
                break;
            case read_state::next_block_finding: {
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(rt_reading.cur_group_);
#endif
                const auto next_block_found = rt_try_traverse_to_next_block_in_group(verify_result);
                if (!next_block_found) {
                    rt_reading.state_ = read_state::next_group_finding;
                    break;
                }
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(rt_reading.cur_block_);
#endif
                if (context_verify_result::seq_pending == verify_result && (rt_reading.version_ == version_)) {
                    rt_reading.traverse_end_block_is_working_ = false;
                } else if (rt_reading.traverse_end_block_is_working_ && rt_reading.traverse_end_block_ == rt_reading.cur_block_) {
                    if (rt_reading.version_ == version_) {
                        rt_reading.state_ = read_state::traversal_completed;
                    } else {
                        ++rt_reading.version_;
                    }
                } else if (!rt_reading.traverse_end_block_is_working_ && !rt_cache_.mem_optimize_.is_block_marked_removed) {
                    rt_reading.traverse_end_block_is_working_ = true;
                    rt_reading.traverse_end_block_ = rt_reading.cur_block_;
                }

                if (context_verify_result::valid == verify_result
                    && read_state::traversal_completed != rt_reading.state_) {
                    rt_reading.state_ = read_state::hp_block_reading;
                }
                break;
            }
            case read_state::next_group_finding:
                if (!rt_try_traverse_to_next_group()) {
                    rt_reading.state_ = read_state::lp_buffer_reading;
                    if (rt_reading.traverse_end_block_is_working_) {
                        if (rt_reading.traverse_end_block_ == nullptr) {
                            if (rt_reading.version_ == version_) {
                                rt_reading.state_ = read_state::traversal_completed;
                            } else {
                                ++rt_reading.version_;
                            }
                        }
                    } else {
                        rt_reading.traverse_end_block_is_working_ = true;
                        rt_reading.traverse_end_block_ = nullptr;
                    }
                } else {
                    rt_reading.state_ = read_state::next_block_finding;
                }
                break;
            case read_state::traversal_completed:
                if (rt_reading.cur_block_) {
                    read_handle = rt_reading.cur_block_->get_buffer().read_an_empty_chunk();
                    rt_reading.state_ = read_state::hp_block_reading;
                } else {
                    read_handle = lp_buffer_.read_an_empty_chunk();
                    rt_reading.state_ = read_state::lp_buffer_reading;
                }
                loop_finished = true;
                break;
            default:
                assert(false && "unexpected state!");
                break;
            }
        }
        return read_handle;
    }

    void log_buffer::return_read_chunk(const log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        bq::platform::thread::thread_id current_thread_id = bq::platform::thread::get_current_thread_id();
        assert(current_thread_id == read_thread_id_ && "only single thread reading is supported for log_buffer!");
#endif
        if (rt_cache_.current_reading_.hp_handle_cache_.result == enum_buffer_result_code::success) {
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(rt_cache_.current_reading_.hp_handle_cache_.verify_chunk(handle) && "log_buffer::return_read_chunk hp chunk return verify failed");
#endif
            if (!rt_cache_.current_reading_.hp_handle_cache_.has_next()) {
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(rt_cache_.current_reading_.cur_block_);
#endif
                rt_cache_.current_reading_.cur_block_->get_buffer().return_batch_read_chunks(rt_cache_.current_reading_.hp_handle_cache_);
            }
            return;
        }
        if (rt_cache_.current_reading_.cur_block_) {
            if (rt_cache_.current_reading_.hp_handle_cache_.result == enum_buffer_result_code::success) {
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(rt_cache_.current_reading_.hp_handle_cache_.verify_chunk(handle) && "log_buffer::return_read_chunk hp chunk return verify failed");
#endif
                if (!rt_cache_.current_reading_.hp_handle_cache_.has_next()) {
                    rt_cache_.current_reading_.cur_block_->get_buffer().return_batch_read_chunks(rt_cache_.current_reading_.hp_handle_cache_);
                }
                return;
            } else {
                rt_cache_.current_reading_.cur_block_->get_buffer().return_batch_read_chunks(rt_cache_.current_reading_.hp_handle_cache_);
            }

        } else if (enum_buffer_result_code::success == handle.result) {
            log_buffer_read_handle handle_cpy = handle;
            handle_cpy.data_addr -= sizeof(context_head);
            handle_cpy.data_size += (uint32_t)sizeof(context_head);
            const auto& context = *reinterpret_cast<const context_head*>(handle_cpy.data_addr);
            deregister_seq(context);
            BQ_UNLIKELY_IF(context.is_external_ref_)
            {
                rt_return_oversize_read_chunk(handle_cpy);
            }
            else
            {
                lp_buffer_.return_read_chunk(handle_cpy);
            }
        } else {
            lp_buffer_.return_read_chunk(handle);
        }
    }

#if defined(BQ_JAVA)
    log_buffer::java_buffer_info log_buffer::get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert((this->id_ == log_tls_info_.get().cur_log_buffer_id_) && "tls cur_log_buffer_ check failed");
#endif
        auto& current_buffer_info = log_tls_info_.get().get_buffer_info_directly(this);
        java_buffer_info result;
        result.buffer_array_obj_ = nullptr;
        result.offset_store_ = &current_buffer_info.java_.buffer_offset_;

        BQ_UNLIKELY_IF(current_buffer_info.oversize_target_buffer_)
        {
            if (!current_buffer_info.java_.buffer_obj_for_oversize_buffer_) {
                jobject byte_array_obj = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
                current_buffer_info.java_.buffer_obj_for_oversize_buffer_ = (jobjectArray)env->NewGlobalRef(byte_array_obj);
                auto offset_obj = bq::platform::create_new_direct_byte_buffer(env, &current_buffer_info.java_.buffer_offset_, sizeof(current_buffer_info.java_.buffer_offset_), false);
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_oversize_buffer_, 1, offset_obj);
            }
            auto& over_size_buffer_ref = current_buffer_info.oversize_target_buffer_->buffer_;
            if (current_buffer_info.java_.buffer_ref_oversize != &over_size_buffer_ref
                || current_buffer_info.java_.size_ref_oversize != over_size_buffer_ref.get_block_size() * over_size_buffer_ref.get_total_blocks_count()) {
                current_buffer_info.java_.buffer_ref_oversize = &over_size_buffer_ref;
                current_buffer_info.java_.size_ref_oversize = over_size_buffer_ref.get_block_size() * over_size_buffer_ref.get_total_blocks_count();
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_oversize_buffer_, 0, bq::platform::create_new_direct_byte_buffer(env, const_cast<uint8_t*>(over_size_buffer_ref.get_buffer_addr()), current_buffer_info.java_.size_ref_oversize, false));
            }
            result.buffer_array_obj_ = current_buffer_info.java_.buffer_obj_for_oversize_buffer_;
            result.buffer_base_addr_ = over_size_buffer_ref.get_buffer_addr();
            *result.offset_store_ = (int32_t)(handle.data_addr - result.buffer_base_addr_);
        }
        else if (current_buffer_info.cur_block_)
        {
            auto& ring_buffer = current_buffer_info.cur_block_->get_buffer();
            if (!current_buffer_info.java_.buffer_obj_for_hp_buffer_) {
                jobject byte_array_obj = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
                current_buffer_info.java_.buffer_obj_for_hp_buffer_ = (jobjectArray)env->NewGlobalRef(byte_array_obj);
                auto offset_obj = bq::platform::create_new_direct_byte_buffer(env, &current_buffer_info.java_.buffer_offset_, sizeof(current_buffer_info.java_.buffer_offset_), false);
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_hp_buffer_, 1, offset_obj);
            }
            if (current_buffer_info.java_.buffer_ref_block_ != current_buffer_info.cur_block_) {
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_hp_buffer_, 0, bq::platform::create_new_direct_byte_buffer(env, const_cast<uint8_t*>(ring_buffer.get_buffer_addr()), ring_buffer.get_block_size() * ring_buffer.get_total_blocks_count(), false));
                current_buffer_info.java_.buffer_ref_block_ = current_buffer_info.cur_block_;
            }
            result.buffer_array_obj_ = current_buffer_info.java_.buffer_obj_for_hp_buffer_;
            result.buffer_base_addr_ = ring_buffer.get_buffer_addr();
            *result.offset_store_ = (int32_t)(handle.data_addr - result.buffer_base_addr_);
        }
        else
        {
            if (!current_buffer_info.java_.buffer_obj_for_lp_buffer_) {
                jobject byte_array_obj = env->NewObjectArray(2, env->FindClass("java/nio/ByteBuffer"), nullptr);
                current_buffer_info.java_.buffer_obj_for_lp_buffer_ = (jobjectArray)env->NewGlobalRef(byte_array_obj);
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_lp_buffer_, 0, bq::platform::create_new_direct_byte_buffer(env, const_cast<uint8_t*>(lp_buffer_.get_buffer_addr()), lp_buffer_.get_block_size() * lp_buffer_.get_total_blocks_count(), false));
                env->SetObjectArrayElement(current_buffer_info.java_.buffer_obj_for_lp_buffer_, 1, bq::platform::create_new_direct_byte_buffer(env, &current_buffer_info.java_.buffer_offset_, sizeof(current_buffer_info.java_.buffer_offset_), false));
            }
            result.buffer_array_obj_ = current_buffer_info.java_.buffer_obj_for_lp_buffer_;
            result.buffer_base_addr_ = lp_buffer_.get_buffer_addr();
            *result.offset_store_ = (int32_t)(handle.data_addr - result.buffer_base_addr_);
        }
        return result;
    }
#endif

    bq::block_node_head* log_buffer::alloc_new_hp_block()
    {
        log_tls_buffer_info& tls_buffer_info = log_tls_info_.get().get_buffer_info_directly(this);
        block_misc_data misc_data;
        memset(&misc_data, 0, sizeof(misc_data));
        misc_data.context_.set_tls_info(&tls_buffer_info);
        misc_data.context_.version_ = version_;
        misc_data.context_.is_thread_finished_ = false;
        misc_data.context_.seq_ = tls_buffer_info.wt_data_.current_write_seq_++;
        auto new_node = hp_buffer_.alloc_new_block(&misc_data, sizeof(block_misc_data));
        return new_node;
    }

    log_buffer::context_verify_result log_buffer::verify_context(const context_head& context)
    {
        if (context.version_ == rt_cache_.current_reading_.version_) {
            // This is the most common scenario.
            BQ_LIKELY_IF(context.version_ == version_)
            {
                // for new data
                auto expected_seq = context.get_tls_info()->rt_data_.current_read_seq_;
                if (context.seq_ == expected_seq) {
                    return context_verify_result::valid;
                }
                assert((context.seq_ > expected_seq) && "seq_invalid only occurs when recovering");
                return context_verify_result::seq_pending;
            }
            else
            {
                // for recovering
                auto iter = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)].find(context.get_tls_info());
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert((iter != rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)].end()) && "unknowing thread found in recovering");
#endif
                auto expected_seq = iter->value();
                if (context.seq_ == expected_seq) {
                    return context_verify_result::valid;
                }
                return context.seq_ < expected_seq ? context_verify_result::seq_invalid : context_verify_result::seq_pending;
            }
        } else if (!is_version_valid(context.version_)) {
            // need discard.
            return context_verify_result::version_invalid;
        }
        return context_verify_result::version_pending;
    }

    log_buffer::context_verify_result log_buffer::verify_oversize_context(const log_buffer::context_head& parent_context, const log_buffer::context_head& oversize_context)
    {
        BQ_LIKELY_IF(parent_context.version_ == oversize_context.version_)
        {
            if (parent_context.seq_ == oversize_context.seq_) {
                return context_verify_result::valid;
            } else if (parent_context.seq_ < oversize_context.seq_) {
                return context_verify_result::seq_pending;
            } else {
                assert(parent_context.version_ != version_ && "seq_invalid only occurs when recovering");
                return context_verify_result::seq_invalid;
            }
        }
        else if (parent_context.version_ < oversize_context.version_)
        {
            BQ_UNLIKELY_IF(!is_version_valid(oversize_context.version_))
            {
                return context_verify_result::version_invalid;
            }
            else
            {
                return context_verify_result::version_pending;
            }
        }
        else
        {
            return context_verify_result::version_invalid;
        }
    }

    void log_buffer::deregister_seq(const context_head& context)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(context.version_ == rt_cache_.current_reading_.version_ && "invalid deregister_version");
#endif
        if (context.version_ == version_) {
            // for new data
            if (context.is_thread_finished_) {
                bq::util::aligned_delete(context.get_tls_info());
            } else {
                ++context.get_tls_info()->rt_data_.current_read_seq_;
            }
        } else {
            // for recovering data
            auto& recover_map = rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(version_ - 1 - context.version_)];
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert((recover_map.find(context.get_tls_info()) != recover_map.end()) && "unregister none exist seq");
#endif
            ++recover_map[context.get_tls_info()];
        }
    }

    bool log_buffer::rt_read_from_lp_buffer(log_buffer_read_handle& out_handle)
    {
        while (true) {
            auto read_handle = lp_buffer_.read_chunk();
            if (enum_buffer_result_code::success == read_handle.result) {
                const auto& context = *reinterpret_cast<const context_head*>(read_handle.data_addr);
                auto verify_result = verify_context(context);
                switch (verify_result) {
                case context_verify_result::valid:
                    BQ_LIKELY_IF(!context.is_thread_finished_)
                    {
                        BQ_LIKELY_IF(!context.is_external_ref_)
                        {
                            out_handle = read_handle;
                            return true;
                        }
                        else
                        {
                            decltype(read_handle) oversize_handle;
                            if (rt_read_oversize_chunk(read_handle, oversize_handle)) {
                                out_handle = oversize_handle;
                                return true;
                            }
                        }
                    }
                    deregister_seq(context);
                    lp_buffer_.return_read_chunk(read_handle);
                    break;
                case context_verify_result::version_invalid:
                case context_verify_result::seq_invalid:
                    lp_buffer_.return_read_chunk(read_handle);
                    break;
                case context_verify_result::version_pending:
                case context_verify_result::seq_pending:
                    lp_buffer_.discard_read_chunk(read_handle);
                    return false;
                    break;
                default:
                    break;
                }
            } else {
                lp_buffer_.return_read_chunk(read_handle);
                return false;
            }
        }
    }

    bool log_buffer::rt_try_traverse_to_next_block_in_group(context_verify_result& out_verify_result)
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
        auto& rt_reading = rt_cache_.current_reading_;
        bool is_cur_block_in_group = rt_reading.cur_group_.value().is_range_include(rt_reading.cur_block_);
        // Find new block
        block_node_head* next_block = is_cur_block_in_group
            ? rt_reading.cur_group_.value().get_data_head().used_.next(rt_reading.cur_block_)
            : rt_reading.cur_group_.value().get_data_head().used_.first();
        if (!next_block) {
            next_block = rt_reading.cur_group_.value().get_data_head().stage_.pop();
            if (next_block) {
                rt_reading.cur_group_.value().get_data_head().used_.push_after_thread_unsafe(is_cur_block_in_group ? rt_reading.cur_block_ : nullptr, next_block);
            }
        }

        if (next_block) {
            assert(next_block != rt_reading.cur_block_);
        }

        // Process traversed block
        bool block_removed = false;
#if defined(BQ_LOG_BUFFER_DEBUG)
        if (is_cur_block_in_group) {
            assert(rt_reading.cur_group_.value().get_data_head().used_.is_include(rt_reading.cur_block_));
        }
#endif
        if (is_cur_block_in_group) {
            if (mem_opt.is_block_marked_removed) {
                if (context_verify_result::valid == mem_opt.verify_result) {
                    const auto& context = rt_reading.cur_block_->get_misc_data<block_misc_data>().context_;
                    deregister_seq(context);
                }
                // remove it
                if (rt_reading.cur_group_.value().is_range_include(rt_reading.last_block_)) {
                    hp_buffer_.recycle_block_thread_unsafe(rt_reading.cur_group_, rt_reading.last_block_, rt_reading.cur_block_);
                } else {
                    hp_buffer_.recycle_block_thread_unsafe(rt_reading.cur_group_, nullptr, rt_reading.cur_block_);
                }
                block_removed = true;
            } else {
                ++mem_opt.cur_group_using_blocks_num_;
            }
        }
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(mem_opt.cur_group_using_blocks_num_ <= hp_buffer_.get_max_block_count_per_group());
#endif
        if ((!block_removed && is_cur_block_in_group) || rt_reading.cur_block_ == nullptr) {
            rt_reading.last_block_ = rt_reading.cur_block_;
        }

        // Verify and prepare for next block
        if (next_block) {
            auto& context = next_block->get_misc_data<block_misc_data>().context_;
            out_verify_result = verify_context(context);
            mem_opt.is_block_marked_removed = (out_verify_result == context_verify_result::valid && is_block_removed(next_block))
                || out_verify_result == context_verify_result::version_invalid
                || out_verify_result == context_verify_result::seq_invalid;
            mem_opt.verify_result = out_verify_result;
            if (mem_opt.left_holes_num_ > 0) {
                mark_block_need_reallocate(next_block, true);
                --mem_opt.left_holes_num_;
            }
            rt_reading.cur_block_ = next_block;
            return true;
        } else {
            mem_opt.is_block_marked_removed = false;
            mem_opt.verify_result = context_verify_result::version_invalid;
        }
        return false;
    }

    bool log_buffer::rt_try_traverse_to_next_group()
    {
        auto& mem_opt = rt_cache_.mem_optimize_;
        auto& rt_reading = rt_cache_.current_reading_;
        auto next_group = rt_reading.cur_group_
            ? hp_buffer_.next(rt_reading.cur_group_, group_list::lock_type::no_lock)
            : hp_buffer_.first(group_list::lock_type::no_lock);
        bool group_removed = false;
        if (mem_opt.cur_group_using_blocks_num_ == 0 && rt_reading.cur_group_) {
            // try lock
            group_list::iterator current_lock_iterator;
            if (rt_reading.last_group_) {
                current_lock_iterator = hp_buffer_.next(rt_reading.last_group_, group_list::lock_type::write_lock);
            } else {
                // must lock from head, because new group node is inserted after head pointer from other threads.
                while (current_lock_iterator != rt_reading.cur_group_) {
                    current_lock_iterator = static_cast<bool>(current_lock_iterator)
                        ? hp_buffer_.next(current_lock_iterator, group_list::lock_type::write_lock)
                        : hp_buffer_.first(group_list::lock_type::write_lock);
                    if (!current_lock_iterator) {
                        assert(false && "try lock removing group from head failed");
                    }
                }
            }
            assert(current_lock_iterator == rt_reading.cur_group_ && "try lock removing group failed");
            if ((!current_lock_iterator.value().get_data_head().used_.first())
                && (!current_lock_iterator.value().get_data_head().stage_.first())) {
#ifdef BQ_UNIT_TEST
                uint16_t free_blocks = 0;
                auto free_block_iter = current_lock_iterator.value().get_data_head().free_.first();
                while (free_block_iter) {
                    ++free_blocks;
                    free_block_iter = current_lock_iterator.value().get_data_head().free_.next(free_block_iter);
                }
                if (free_blocks != hp_buffer_.get_max_block_count_per_group()) {
                    printf("free_blocks != hp_buffer_.get_max_block_count_per_group() failed, free_blocks:%" PRIu16 ", max_blocks:%" PRIu16 "\n", free_blocks, hp_buffer_.get_max_block_count_per_group());
                    printf("used head:%p, stage head:%p\n", (void*)current_lock_iterator.value().get_data_head().used_.first(), (void*)current_lock_iterator.value().get_data_head().stage_.first());
                    free_block_iter = current_lock_iterator.value().get_data_head().free_.first();
                    while (free_block_iter) {
                        printf("=>%" PRIu16, current_lock_iterator.value().get_data_head().free_.get_index_by_block_head(free_block_iter));
                        free_block_iter = current_lock_iterator.value().get_data_head().free_.next(free_block_iter);
                    }
                    printf("\n");
                    fflush(stdout);
                    assert(false && "all blocks should be free when group is removed");
                }
#endif
                // remove it
                group_removed = true;
                assert(!rt_reading.cur_group_.value().is_range_include(rt_reading.last_block_));
                hp_buffer_.delete_and_unlock_thread_unsafe(current_lock_iterator);
            } else {
                // unlock
                hp_buffer_.next(current_lock_iterator, group_list::lock_type::no_lock);
            }
        }
        if (rt_reading.cur_group_ && !group_removed) {
            mem_opt.left_holes_num_ += static_cast<uint32_t>(hp_buffer_.get_max_block_count_per_group() - mem_opt.cur_group_using_blocks_num_);
        }
        mem_opt.cur_group_using_blocks_num_ = 0;
        if (!group_removed) {
            rt_reading.last_group_ = rt_reading.cur_group_;
        }
        if (!next_group) {
            mem_opt.left_holes_num_ = 0;
            rt_reading.cur_block_ = nullptr;
            rt_reading.last_block_ = nullptr;
        }
        rt_reading.cur_group_ = next_group;
        return next_group;
    }

    log_buffer_write_handle log_buffer::wt_alloc_oversize_write_chunk(uint32_t size, uint64_t current_epoch_ms)
    {
        auto& tls_buffer = log_tls_info_.get().get_buffer_info(this);
        auto& block_cache = tls_buffer.cur_block_;
        if (block_cache) {
            mark_block_removed(block_cache, true); // mark removed;
            block_cache = nullptr;
        }
        auto parent_result = lp_buffer_.alloc_write_chunk(sizeof(context_head));
        if (enum_buffer_result_code::success != parent_result.result) {
            // TODO: Even log_memory_policy::auto_expand_when_full is enabled,
            // allocation may still failed by "not enough space" error when
            // allocating oversize chunk.
            if ((config_.policy == log_memory_policy::auto_expand_when_full
                    || config_.policy == log_memory_policy::block_when_full)
                && parent_result.result == enum_buffer_result_code::err_not_enough_space) {

                parent_result.result = enum_buffer_result_code::err_wait_and_retry;
            }
            return parent_result;
        }
        auto* parent_context = reinterpret_cast<context_head*>(parent_result.data_addr);
        parent_context->version_ = version_;
        parent_context->is_thread_finished_ = false;
        parent_context->seq_ = tls_buffer.wt_data_.current_write_seq_++;
        parent_context->is_external_ref_ = true;
        parent_context->set_tls_info(&tls_buffer);

        decltype(parent_result) over_size_handle;
        // try to find in existing oversize buffer
        {
            bq::platform::scoped_spin_lock_read_crazy r_lock(temprorary_oversize_buffer_.array_lock_);
            for (auto& over_size_buffer : temprorary_oversize_buffer_.buffers_array_) {
                const auto& oversize_buffer_context = over_size_buffer->buffer_.get_misc_data<context_head>();
                if (oversize_buffer_context.version_ != parent_context->version_
                    || oversize_buffer_context.get_tls_info() != parent_context->get_tls_info()) {
                    continue;
                }
                over_size_buffer->buffer_lock_.read_lock();
                over_size_handle = over_size_buffer->buffer_.alloc_write_chunk(size + static_cast<uint32_t>(sizeof(context_head)));
                if (enum_buffer_result_code::success == over_size_handle.result) {
                    over_size_buffer->last_used_epoch_ms_ = current_epoch_ms;
                    tls_buffer.oversize_target_buffer_ = over_size_buffer.operator->();
                    break;
                }
                over_size_buffer->buffer_lock_.read_unlock();
            }
        }
        // try to alloc a new oversize buffer
        if (enum_buffer_result_code::success != over_size_handle.result) {
            uint32_t default_buffer_size = (size < (UINT32_MAX >> 1)) ? (size << 1) : UINT32_MAX;
            bq::string abs_recovery_file_path;
            if (config_.need_recovery) {
                auto new_index = current_oversize_buffer_index_.add_fetch(1, bq::platform::memory_order::relaxed);
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "_%" PRIu64 "", new_index);
                abs_recovery_file_path = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name + "/os/" + config_.log_name + tmp + ".mmap", 0);
            }
            bq::platform::scoped_spin_lock_write_crazy w_lock(temprorary_oversize_buffer_.array_lock_);
            temprorary_oversize_buffer_.buffers_array_.emplace_back(bq::make_unique<oversize_buffer_obj_def>(default_buffer_size, config_.need_recovery, abs_recovery_file_path));
            auto& new_buffer = *(temprorary_oversize_buffer_.buffers_array_.end() - 1);
            new_buffer->buffer_lock_.read_lock();
            auto& oversize_buffer_context = new_buffer->buffer_.get_misc_data<context_head>();
            oversize_buffer_context.version_ = version_;
            oversize_buffer_context.is_thread_finished_ = false;
            oversize_buffer_context.seq_ = UINT32_MAX;
            oversize_buffer_context.is_external_ref_ = true;
            oversize_buffer_context.set_tls_info(&tls_buffer);
            over_size_handle = new_buffer->buffer_.alloc_write_chunk(size + static_cast<uint32_t>(sizeof(context_head)));
            assert(enum_buffer_result_code::success == over_size_handle.result && "New created oversize buffer shouldn't fail when allocating");
            new_buffer->last_used_epoch_ms_ = current_epoch_ms;
            tls_buffer.oversize_target_buffer_ = new_buffer.operator->();
        }
        auto* oversize_context = reinterpret_cast<context_head*>(over_size_handle.data_addr);
        oversize_context->version_ = version_;
        oversize_context->is_thread_finished_ = false;
        oversize_context->seq_ = parent_context->seq_;
        oversize_context->is_external_ref_ = true;
        oversize_context->set_tls_info(&tls_buffer);
        over_size_handle.data_addr += sizeof(context_head);
        tls_buffer.oversize_parent_handle_ = parent_result;
        return over_size_handle;
    }

    void log_buffer::wt_commit_oversize_write_chunk(const log_buffer_write_handle& oversize_handle)
    {
        auto& tls_buffer_info = log_tls_info_.get().get_buffer_info_directly(this);
        tls_buffer_info.oversize_target_buffer_->buffer_.commit_write_chunk(oversize_handle);
        tls_buffer_info.oversize_target_buffer_->buffer_lock_.read_unlock();
        tls_buffer_info.oversize_target_buffer_ = nullptr;
        lp_buffer_.commit_write_chunk(tls_buffer_info.oversize_parent_handle_);
    }

    BQ_TLS_NON_POD(log_buffer_read_handle, rt_oversize_parent_handle_)
    BQ_TLS void* rt_oversize_target_buffer_;
    bool log_buffer::rt_read_oversize_chunk(const log_buffer_read_handle& parent_handle, log_buffer_read_handle& out_oversize_handle)
    {
        rt_oversize_parent_handle_.get() = parent_handle;
        const auto& parent_context = *reinterpret_cast<const context_head*>(parent_handle.data_addr);
        bq::platform::scoped_spin_lock_read_crazy r_lock(temprorary_oversize_buffer_.array_lock_);
        for (auto& buffer : temprorary_oversize_buffer_.buffers_array_) {
            const auto& oversize_buffer_context = buffer->buffer_.get_misc_data<context_head>();
            if (oversize_buffer_context.version_ != parent_context.version_
                || oversize_buffer_context.get_tls_info() != parent_context.get_tls_info()) {
                continue;
            }
            bool read_done = false;
            while (!read_done) {
                auto read_handle = buffer->buffer_.read_chunk();
                if (enum_buffer_result_code::success == read_handle.result) {
                    const auto& oversize_context = *reinterpret_cast<const context_head*>(read_handle.data_addr);
                    auto verify_result = verify_oversize_context(parent_context, oversize_context);
                    switch (verify_result) {
                    case bq::log_buffer::context_verify_result::valid:
                        rt_oversize_target_buffer_ = buffer.operator->();
                        out_oversize_handle = read_handle;
                        return true;
                        break;
                    case bq::log_buffer::context_verify_result::version_invalid:
                    case bq::log_buffer::context_verify_result::seq_invalid:
                        buffer->buffer_.return_read_chunk(read_handle);
                        break;
                    case bq::log_buffer::context_verify_result::version_pending:
                    case bq::log_buffer::context_verify_result::seq_pending:
                        buffer->buffer_.discard_read_chunk(read_handle);
                        read_done = true;
                        break;
                    default:
                        assert(false && "unexpected context verify result in rt_read_oversize_chunk");
                        break;
                    }
                } else {
                    buffer->buffer_.return_read_chunk(read_handle);
                    read_done = true;
                }
            }
        }
        assert(parent_context.version_ != version_); // data missing only occurs when recovering data.
        return false;
    }

    void log_buffer::rt_return_oversize_read_chunk(const log_buffer_read_handle& oversize_handle)
    {
        auto oversize_buffer = ((oversize_buffer_obj_def*)rt_oversize_target_buffer_);
        oversize_buffer->buffer_.return_read_chunk(oversize_handle);
        lp_buffer_.return_read_chunk(rt_oversize_parent_handle_.get());
    }

    void log_buffer::rt_recycle_oversize_buffers()
    {
        BQ_LIKELY_IF(temprorary_oversize_buffer_.buffers_array_.is_empty())
        {
            return;
        }
        auto current_epoch_ms = bq::platform::high_performance_epoch_ms();
        bool need_recycle = false;
        {
            bq::platform::scoped_try_spin_lock_read_crazy array_read_try_lock(temprorary_oversize_buffer_.array_lock_);
            if (array_read_try_lock.owns_lock()) {
                for (auto iter = temprorary_oversize_buffer_.buffers_array_.begin(); iter != temprorary_oversize_buffer_.buffers_array_.end(); ++iter) {
                    auto& oversize_buffer = *iter;
                    if (current_epoch_ms >= oversize_buffer->last_used_epoch_ms_ + OVERSIZE_BUFFER_RECYCLE_INTERVAL_MS) {
                        need_recycle = true;
                        break;
                    }
                }
            }
        }

        if (need_recycle) {
            bq::platform::scoped_try_spin_lock_write_crazy array_write_try_lock(temprorary_oversize_buffer_.array_lock_);
            if (array_write_try_lock.owns_lock()) {
                for (size_t i = temprorary_oversize_buffer_.buffers_array_.size(); i > 0; --i) {
                    auto index = i - 1;
                    auto& oversize_buffer = *temprorary_oversize_buffer_.buffers_array_[index];
                    bool is_empty = false;
                    {
                        bq::platform::scoped_try_spin_lock_write_crazy buffer_write_try_lock(oversize_buffer.buffer_lock_);

                        if (buffer_write_try_lock.owns_lock()) {
                            bool loop_finished = false;
                            while (!loop_finished) {
                                auto read_handle = oversize_buffer.buffer_.read_chunk();
                                if (read_handle.result == enum_buffer_result_code::err_empty_log_buffer) {
                                    oversize_buffer.buffer_.return_read_chunk(read_handle);
                                    is_empty = true;
                                    loop_finished = true;
                                    break;
                                } else if (read_handle.result == enum_buffer_result_code::success) {
                                    const auto& context = *reinterpret_cast<const context_head*>(read_handle.data_addr);
                                    auto verify_result = verify_context(context);
                                    switch (verify_result) {
                                    case bq::log_buffer::context_verify_result::valid:
                                    case bq::log_buffer::context_verify_result::version_pending:
                                    case bq::log_buffer::context_verify_result::seq_pending:
                                        oversize_buffer.buffer_.discard_read_chunk(read_handle);
                                        loop_finished = true;
                                        break;
                                    case bq::log_buffer::context_verify_result::version_invalid:
                                    case bq::log_buffer::context_verify_result::seq_invalid:
                                        oversize_buffer.buffer_.return_read_chunk(read_handle);
                                        break;
                                    default:
                                        assert(false && "unexpected context verify result in rt_read_oversize_chunk");
                                        break;
                                    }
                                } else {
                                    assert(false && "impossible oversize buffer read result");
                                    break;
                                }
                            }
                        }
                    }
                    if (is_empty) {
                        bq::string abs_mmap_file_path = temprorary_oversize_buffer_.buffers_array_[index]->buffer_.get_mmap_file_path();
                        temprorary_oversize_buffer_.buffers_array_.erase_replace(temprorary_oversize_buffer_.buffers_array_.begin() + static_cast<ptrdiff_t>(index));
                        if (bq::file_manager::is_file(abs_mmap_file_path)) {
                            bq::file_manager::remove_file_or_dir(abs_mmap_file_path);
                        }
                    }
                }
            }
        }
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
                context.version_++; // mark invalid
            } else if (static_cast<uint16_t>(buffer.version_ - 1 - context.version_) < MAX_RECOVERY_VERSION_RANGE) {
                auto& recovery_map = buffer.rt_cache_.current_reading_.recovery_records_[static_cast<uint16_t>(buffer.version_ - 1 - context.version_)];
                auto iter = recovery_map.find(context.get_tls_info());
                if (iter == recovery_map.end()) {
                    recovery_map[context.get_tls_info()] = context.seq_;
                } else {
                    iter->value() = bq::min_value(iter->value(), context.seq_);
                }
            }
        },
            this);
        lp_buffer_.set_thread_check_enable(true);

        auto group = hp_buffer_.first(group_list::lock_type::no_lock);
        while (group) {
            auto block = group.value().get_data_head().used_.first();
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

        // Recover oversize buffers
        bq::string over_size_memory_map_folder = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name + "/os", 0);
        if (bq::file_manager::is_dir(over_size_memory_map_folder)) {
            bq::array<bq::string> sub_names = bq::file_manager::get_sub_dirs_and_files_name(over_size_memory_map_folder);
            for (const bq::string& file_name : sub_names) {
                bq::string full_path = bq::file_manager::combine_path(over_size_memory_map_folder, file_name);
                if (!file_name.end_with(".mmap") || !file_name.begin_with(config_.log_name + "_")) {
                    bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file:%s", full_path.c_str());
                    bq::file_manager::remove_file_or_dir(full_path);
                    continue;
                }
                char* end_ptr = nullptr;
                errno = 0;
                bq::string file_name_cpy = file_name.substr(config_.log_name.size() + 1);
                uint64_t u64_value = strtoull(file_name_cpy.c_str(), &end_ptr, 10);
                if (errno == ERANGE
                    || end_ptr != file_name_cpy.c_str() + (file_name_cpy.size() - strlen(".mmap"))) {
                    bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file:%s", full_path.c_str());
                    bq::file_manager::remove_file_or_dir(full_path);
                    continue;
                }
                size_t file_size = bq::file_manager::get_file_size(full_path);
                uint32_t default_buffer_size = static_cast<uint32_t>(BQ_CACHE_LINE_SIZE);
                while (default_buffer_size < UINT32_MAX) {
                    uint32_t map_size = normal_buffer::calculate_size_of_memory(default_buffer_size);
                    size_t desired_file_size = bq::memory_map::get_min_size_of_memory_map_file(0, map_size);
                    if (desired_file_size == file_size) {
                        break;
                    }
                    default_buffer_size <<= 1;
                }
                if (default_buffer_size == UINT32_MAX) {
                    bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file:%s, invalid size:%" PRIu64, full_path.c_str(), static_cast<uint64_t>(file_size));
                    bq::file_manager::remove_file_or_dir(full_path);
                    continue;
                }
                if (u64_value > current_oversize_buffer_index_.load(bq::platform::memory_order::relaxed)) {
                    current_oversize_buffer_index_.store(u64_value, bq::platform::memory_order::seq_cst);
                }
                auto recovery_buffer = bq::make_unique<oversize_buffer_obj_def>(default_buffer_size, true, full_path);
                const auto& oversize_buffer_context = recovery_buffer->buffer_.get_misc_data<context_head>();
                if (oversize_buffer_context.seq_ != UINT32_MAX
                    || oversize_buffer_context.is_external_ref_ != true
                    || !is_version_valid(oversize_buffer_context.version_)) {
                    bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file when recovery:%s, invalid context", full_path.c_str());
                    bq::file_manager::remove_file_or_dir(full_path);
                } else {
                    temprorary_oversize_buffer_.buffers_array_.emplace_back(bq::move(recovery_buffer));
                }
            }
        }
    }

    void log_buffer::clear_recovery_data()
    {
        bq::string memory_map_folder = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name, 0);
        if (bq::file_manager::is_dir(memory_map_folder)) {
            bq::file_manager::remove_file_or_dir(memory_map_folder);
        }
    }
}
