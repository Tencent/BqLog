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
 * \class bq::block_list
 *
 * A lock free singly linked list.
 * 
 * \author pippocao
 * \date 2024/12/06
 */
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_types.h"
#include "bq_log/types/buffer/high_performance/siso_ring_buffer.h"

namespace bq {
    class block_node_head {
    public:
        friend class block_list;
        union pointer_type {
            BQ_STRUCT_PACK(struct
            {
                uint16_t index_;
                uint16_t aba_mark_;
            });
            uint32_t ordinary_value_;

            pointer_type();
            ~pointer_type();
            bq_forceinline pointer_type(const pointer_type& rhs) : ordinary_value_(rhs.ordinary_value_) {}
            bq_forceinline pointer_type& operator=(const pointer_type& rhs) {ordinary_value_ == rhs.ordinary_value_; return *this;}

            bq_forceinline bool is_empty() const { return index_ == (uint16_t)-1; }
        };
        static_assert(offsetof(pointer_type, ordinary_value_) == offsetof(pointer_type, index_), "invalid alignment of bq::block_node_head::pointer_type");
        static_assert((size_t)(offsetof(pointer_type, aba_mark_) - offsetof(pointer_type, index_)) == sizeof(uint16_t), "invalid alignment of bq::block_node_head::pointer_type");

    private:
        BQ_STRUCT_PACK(struct
        {
            union {
                BQ_STRUCT_PACK(struct
                {
                    // These members are modified in different threads which will leads to "False Share".
                    // So we must modify them in a low frequency
                    pointer_type next_;
                    char padding_inner0_[8 - sizeof(next_)];
                    // reserved data. can be cast to any struct, all the bytes will be set to 0 in constructor.
                    char misc_data_[56];
                });
                uint8_t padding_0[CACHE_LINE_SIZE];
            };
            siso_ring_buffer buffer_;
        });
        static_assert(offsetof(block_node_head, next_) == 0, "invalid alignment of bq::block_node_head");
        static_assert((offsetof(block_node_head, misc_data_) % 8 == 0), "invalid alignment of bq::block_node_head");
        static_assert((offsetof(block_node_head, buffer_) == CACHE_LINE_SIZE), "invalid alignment of bq::block_node_head");
    public:
        /// <summary>
        /// constructor
        /// </summary>
        /// <param name="buffer">buffer for siso_ring_buffer</param>
        /// <param name="buffer_size">buffer size for siso_ring_buffer</param>
        /// <param name="is_memory_mapped">is buffer memory mapped</param>
        block_node_head(void* buffer, size_t buffer_size, bool is_memory_mapped);

        template<typename T>
        bq_forceinline T& get_misc_data()
        {
            static_assert(sizeof(T) <= sizeof(misc_data_), "size of T too large");
            return *((T*)(void*)misc_data_);
        }

        void reset_misc_data();

        bq_forceinline siso_ring_buffer& get_buffer() {return buffer_;}

        static ptrdiff_t get_buffer_data_offset();
    };


    class block_list {
        friend class group_data_head;
    private:
        union {
            struct {
                block_node_head::pointer_type head_;
                uint16_t offset_;
                uint16_t max_blocks_count_;
                size_t buffer_size_per_block_;
            };
            uint8_t padding_[CACHE_LINE_SIZE];
        };
    private:
        void reset(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        bool try_recover_from_memory_map(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        void construct_blocks();
        bq_forceinline block_node_head& get_block_head_by_index(uint16_t index) const
        {
            return *(block_node_head*)((uint8_t*)this + (ptrdiff_t)offset_ + (ptrdiff_t)(buffer_size_per_block_ * index));
        }
    public:
        block_list(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size, bool is_memory_mapped);

        ~block_list();

        bq_forceinline block_node_head* first() const
        {
            if (head_.is_empty()) {
                return nullptr;
            }
            return &get_block_head_by_index(head_.index_);
        }

        bq_forceinline block_node_head* next(const block_node_head* current) const
        {
            if (current->next_.is_empty()) {
                return nullptr;
            }
            return &get_block_head_by_index(current->next_.index_);
        }

        bq_forceinline block_node_head* pop()
        {
            while (true) {
                block_node_head::pointer_type head_cpy = head_;
                if (head_cpy.is_empty()) {
                    return nullptr;
                }
                block_node_head* first_node = &get_block_head_by_index(head_cpy.index_);
                uint32_t head_copy_expected_value = head_cpy.ordinary_value_;
                head_cpy.ordinary_value_ = first_node->next_.ordinary_value_;
                ++head_cpy.aba_mark_;
                if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.ordinary_value_, uint32_t).compare_exchange_strong(head_copy_expected_value, head_cpy.ordinary_value_, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                    return first_node;
                }
            }
        }

        bq_forceinline void push(block_node_head* new_block_node)
        {
            new_block_node->next_ = head_;
            while (true) {
                block_node_head::pointer_type head_cpy = head_;
                uint32_t head_copy_expected_value = head_cpy.ordinary_value_;
                head_cpy.index_ = (uint16_t)(((const uint8_t*)new_block_node - ((const uint8_t*)this + (ptrdiff_t)offset_)) / buffer_size_per_block_);
                ++head_cpy.aba_mark_;
                if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.ordinary_value_, uint32_t).compare_exchange_strong(head_copy_expected_value, head_cpy.ordinary_value_, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                    break;
                }
            }
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">nullptr means push after head</param>
        /// <param name="new_block_node"></param>
        /// <returns></returns>
        bq_forceinline void push_after_thread_unsafe(block_node_head* prev_block_node, block_node_head* new_block_node)
        {
            auto& prev_pointer = (prev_block_node ? prev_block_node->next_ : head_);
            new_block_node->next_ = prev_pointer;
            prev_pointer.index_ = (uint16_t)(((const uint8_t*)new_block_node - ((const uint8_t*)this + (ptrdiff_t)offset_)) / buffer_size_per_block_);
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">prev node of `remove_block_node`, nullptr means remove_block_node is head node</param>
        /// <param name="remove_block_node"></param>
        /// <returns></returns>
        bq_forceinline void remove_thread_unsafe(block_node_head* prev_block_node, block_node_head* remove_block_node)
        {
            uint16_t remove_index = (uint16_t)(((const uint8_t*)remove_block_node - ((const uint8_t*)this + (ptrdiff_t)offset_)) / buffer_size_per_block_);
            if (!prev_block_node) {
                assert(head_.index_ == remove_index && "remove assert failed!");
                head_ = remove_block_node->next_;
            } else {
                assert(prev_block_node->next_.index_ == remove_index);
                prev_block_node->next_ = remove_block_node->next_;
            }
        }
    };
    static_assert(sizeof(block_list) == CACHE_LINE_SIZE, "invalid block_list size");
}
