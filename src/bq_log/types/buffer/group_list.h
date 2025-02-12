#pragma once
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
 * \class bq::group_list
 *
 * A singly linked list, where each item is referred to as a "Group,"
 * containing a pool of group_list::group::group_node siso_buffer instances. Each siso_buffer serves a single thread
 * , aimed at reducing inter-thread data storms caused by the MESI protocol, thereby enhancing performance.
 *
 * \author pippocao
 * \date 2024/12/17
 */
#include <stddef.h>
#if BQ_JAVA
#include <jni.h>
#endif
#include "bq_common/bq_common.h"
#include "bq_common/platform/io/memory_map.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/block_list.h"

namespace bq {
    BQ_STRUCT_PACK(struct group_data_head {
        block_list used_;
        block_list free_;
        block_list stage_;
        group_data_head(uint16_t max_blocks_count, uint8_t * group_data_addr, size_t group_data_size, bool is_memory_mapped);
    });
    static_assert(sizeof(group_data_head) == sizeof(block_list) * 3, "invalid bq::group_data_head size");

    class group_node {
    public:
        struct pointer_type {
            bq::platform::spin_lock_rw_crazy lock_;
            group_node* node_ = nullptr; // don't worry about ABA issue, it would not be appear in this case. (lock free op only do push new nodes)

            bq_forceinline bool is_empty() const { return node_ == nullptr; }
        };
        static constexpr uint32_t blocks_per_group = 16;
    private:
        size_t get_group_meta_size(const log_buffer_config& config);
        size_t get_group_data_size(const log_buffer_config& config, uint16_t max_block_count_per_group);
        create_memory_map_result create_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group, uint32_t index);
        bool try_recover_from_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group);
        void init_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group);
        void init_memory(const log_buffer_config& config, uint16_t max_block_count_per_group);
    private:
        pointer_type next_;
        bq::file_handle memory_map_file_;
        bq::memory_map_handle memory_map_handle_;
        uint8_t* node_data_ = nullptr;
        group_data_head* head_ptr_ = nullptr;
#if BQ_JAVA
        jobject java_buffer_obj_ = nullptr;
#endif
    public:
        group_node(const log_buffer_config& config, uint16_t max_block_count_per_group, uint32_t index);
        ~group_node();

        bq_forceinline pointer_type& get_next_ptr() { return next_; }
        bq_forceinline group_data_head& get_data() { return *head_ptr_;}
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
            bq_forceinline operator bool() const { return value_;}
            bq_forceinline group_node& value() { return *value_; }
            bq_forceinline const group_node& value() const { return *value_; }
            bq_forceinline bool operator==(const group_list::iterator& rhs) const { return value_ == rhs.value_;}
        };
    public:
        group_list(const log_buffer_config& config, uint16_t max_block_count_per_group);

        ~group_list();

        block_node_head* alloc_new_block();

        bq_forceinline uint16_t get_max_block_count_per_group() const { return max_block_count_per_group_;}

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

        bq_forceinline void delete_and_unlock_node_thread_unsafe(iterator& current)
        {
            current.last_pointer_->node_ = current.value().get_next_ptr().node_;

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
            delete &current.value();
        }

    private:
        const log_buffer_config& config_;
        uint16_t max_block_count_per_group_;
        bq::platform::atomic<uint32_t> current_group_index_;
        char padding_0[CACHE_LINE_SIZE];
        group_node::pointer_type head_;
        char padding_1[CACHE_LINE_SIZE];
    };

}
