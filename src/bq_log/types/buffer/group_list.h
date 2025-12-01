#pragma once
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
 * \class bq::group_list
 *
 * A singly linked list, where each item is referred to as a "Group,"
 * containing a pool of group_list::group::group_node siso_buffer instances. Each siso_buffer serves a single thread
 * , aimed at reducing inter-thread data storms caused by the MESI protocol, thereby enhancing performance.
 *
 * \author pippocao
 * \date 2024/12/17
 */
#if defined(BQ_JAVA)
#include <jni.h>
#endif
#include "bq_common/bq_common.h"
#include "bq_common/platform/io/memory_map.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/block_list.h"
#include "bq_log/types/buffer/memory_pool.h"
#include "bq_log/types/buffer/normal_buffer.h"

namespace bq {
    BQ_PACK_BEGIN
    struct alignas(BQ_CACHE_LINE_SIZE) group_data_head {
        block_list used_;
        block_list free_;
        block_list stage_;
        group_data_head(uint16_t max_blocks_count, uint8_t* group_data_addr, size_t group_data_size, bool is_memory_recovery);
    } BQ_PACK_END static_assert(sizeof(group_data_head) == sizeof(block_list) * 3, "invalid bq::group_data_head size");

    class group_node : public bq::memory_pool_obj_base<group_node, true> {
    public:
        struct pointer_type {
            bq::platform::spin_lock_rw_crazy lock_;
            group_node* node_ = nullptr; // don't worry about ABA issue, it would not be appear in this case. (lock free op only do push new nodes)

            bq_forceinline bool is_empty() const { return node_ == nullptr; }
        };

    private:
        size_t get_group_meta_size(const log_buffer_config& config);
        size_t get_group_data_size(const log_buffer_config& config, uint16_t max_block_count_per_group);
        create_memory_map_result create_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group, uint64_t index);
        bool try_recover_from_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group);
        void init_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group);
        void init_memory(const log_buffer_config& config, uint16_t max_block_count_per_group);
    private:
        pointer_type next_;
        bq::unique_ptr<bq::normal_buffer> buffer_entity_;
        group_data_head* head_ptr_ = nullptr;
        uint64_t in_pool_epoch_ms_ = 0;
        class group_list* parent_list_ = nullptr;

    public:
        group_node(class group_list* parent_list, uint16_t max_block_count_per_group, uint64_t index);
        ~group_node();

        bq_forceinline pointer_type& get_next_ptr() { return next_; }
        bq_forceinline group_data_head& get_data_head()
        {
            BQ_SUPPRESS_NULL_DEREF_BEGIN();
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(head_ptr_ && "head_ptr_ is null, group_node has not been initialized");
#endif
            return *head_ptr_;
            BQ_SUPPRESS_NULL_DEREF_END();
        }
        bq_forceinline uint64_t get_in_pool_epoch_ms() const { return in_pool_epoch_ms_; }
        bq_forceinline void set_in_pool_epoch_ms(uint64_t epoch_ms) { in_pool_epoch_ms_ = epoch_ms; }
        bq_forceinline bool is_range_include(const block_node_head* block) const
        {
            return head_ptr_->used_.is_range_include(block);
        }
#if defined(BQ_UNIT_TEST)
        bq_forceinline create_memory_map_result get_memory_map_status() const
        {
            return buffer_entity_->get_mmap_result();
        }
#endif
    };

    class group_list {
    public:
        enum class lock_type {
            no_lock,
            read_lock,
            write_lock
        };
        struct iterator {
            friend class group_list;

        private:
            group_node::pointer_type* last_pointer_;
            group_node* value_;
            lock_type last_lock_type_;

        public:
            iterator()
                : last_pointer_(nullptr)
                , value_(nullptr)
                , last_lock_type_(lock_type::no_lock)
            {
            }
            bq_forceinline operator bool() const { return (bool)value_; }
            bq_forceinline group_node& value() { return *value_; }
            bq_forceinline const group_node& value() const { return *value_; }
            bq_forceinline bool operator==(const group_list::iterator& rhs) const { return value_ == rhs.value_; }
            bq_forceinline bool operator!=(const group_list::iterator& rhs) const { return value_ != rhs.value_; }
        };
        static constexpr uint64_t GROUP_NODE_GC_LIFE_TIME_MS = 1000; // If a group node has not been used for 1 seconds, it will be deleted. Otherwise it can stay in the memory pool.
    public:
        group_list(const log_buffer_config& config, uint16_t max_block_count_per_group);

        ~group_list();

        block_node_head* alloc_new_block(const void* misc_data_src, size_t misc_data_size);

        void recycle_block_thread_unsafe(iterator group, block_node_head* prev_block, block_node_head* recycle_block);

        bq_forceinline uint16_t get_max_block_count_per_group() const { return max_block_count_per_group_; }

#if defined(BQ_UNIT_TEST)
        bq_forceinline int32_t get_groups_count() const { return groups_count_.load_seq_cst(); }
#endif

        void garbage_collect();

        size_t get_garbage_count();

        bq_forceinline iterator first(const lock_type type)
        {
            iterator result;
            result.last_pointer_ = &head_;
            result.last_lock_type_ = type;
            switch (type) {
            case bq::group_list::lock_type::no_lock:
                break;
            case bq::group_list::lock_type::read_lock:
                head_.lock_.read_lock();
                break;
            case bq::group_list::lock_type::write_lock:
                head_.lock_.write_lock();
                break;
            default:
                break;
            }
            result.value_ = head_.node_;
            if (!result.value_) {
                switch (type) {
                case bq::group_list::lock_type::no_lock:
                    break;
                case bq::group_list::lock_type::read_lock:
                    head_.lock_.read_unlock();
                    break;
                case bq::group_list::lock_type::write_lock:
                    head_.lock_.write_unlock();
                    break;
                default:
                    break;
                }
            }
            return result;
        }

        bq_forceinline iterator next(iterator& current, lock_type type)
        {
            switch (type) {
            case bq::group_list::lock_type::no_lock:
                break;
            case bq::group_list::lock_type::read_lock:
                current.value_->get_next_ptr().lock_.read_lock();
                break;
            case bq::group_list::lock_type::write_lock:
                current.value_->get_next_ptr().lock_.write_lock();
                break;
            default:
                break;
            }
            iterator result;
            result.last_pointer_ = &current.value_->get_next_ptr();
            result.value_ = current.value_->get_next_ptr().node_;
            result.last_lock_type_ = type;
            if (!result.value_) {
                switch (type) {
                case bq::group_list::lock_type::no_lock:
                    break;
                case bq::group_list::lock_type::read_lock:
                    current.value_->get_next_ptr().lock_.read_unlock();
                    break;
                case bq::group_list::lock_type::write_lock:
                    current.value_->get_next_ptr().lock_.write_unlock();
                    break;
                default:
                    break;
                }
            }

            switch (current.last_lock_type_) {
            case bq::group_list::lock_type::no_lock:
                break;
            case bq::group_list::lock_type::read_lock:
                current.last_pointer_->lock_.read_unlock();
                break;
            case bq::group_list::lock_type::write_lock:
                current.last_pointer_->lock_.write_unlock();
                break;
            default:
                break;
            }
            return result;
        }

        bq_forceinline void delete_and_unlock_thread_unsafe(iterator& current)
        {
            //Make sure no other thread can traverse to next node
            current.value_->get_next_ptr().lock_.write_lock();
            group_node* next_node = current.value().get_next_ptr().node_;
            current.value_->get_next_ptr().lock_.write_unlock();

            //Now we can safely remove current node from the list
            current.last_pointer_->node_ = next_node;

            switch (current.last_lock_type_) {
            case bq::group_list::lock_type::no_lock:
                break;
            case bq::group_list::lock_type::read_lock:
                current.last_pointer_->lock_.read_unlock();
                break;
            case bq::group_list::lock_type::write_lock:
                current.last_pointer_->lock_.write_unlock();
                break;
            default:
                break;
            }
            current.value().set_in_pool_epoch_ms(bq::platform::high_performance_epoch_ms());
            pool_.push(&current.value());
#if defined(BQ_UNIT_TEST)
            groups_count_.fetch_add_seq_cst(-1);
#endif
        }

        bq_forceinline const log_buffer_config& get_config() const { return config_; }

    private:
        const log_buffer_config& config_;
        uint16_t max_block_count_per_group_;
#if defined(BQ_UNIT_TEST)
        bq::platform::atomic<int32_t> groups_count_ = 0;
#endif
        alignas(BQ_CACHE_LINE_SIZE) bq::platform::atomic<uint64_t> current_group_index_;
        alignas(BQ_CACHE_LINE_SIZE) group_node::pointer_type head_;
        alignas(BQ_CACHE_LINE_SIZE) memory_pool<group_node> pool_;
    };

}
