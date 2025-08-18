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
 * \class bq::memory_pool
 *
 * This is a Last-In-First-Out (LIFO) object pool designed to maximize the retention of objects in L1 and L2 caches,
 * while also providing an LRU (Least Recently Used) mechanism that allows for customizable eviction conditions.
 *
 * \author pippocao
 * \date 2024/3/20
 */
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"

namespace bq {
    template <typename T>
    class memory_pool;

    template <typename T, bool Align>
    class memory_pool_obj_base;

    template <typename T>
    class alignas(CACHE_LINE_SIZE) memory_pool_obj_base<T, true> {
    public:
        static constexpr bool is_aligned = true;

    private:
        friend memory_pool<T>;
        T* memory_pool_next_;
        T* memory_pool_prev_;
        char memory_pool_padding_[CACHE_LINE_SIZE - 2 * sizeof(T*)];
        // Note: No constructor to initialize pointers, managed solely by memory_pool for performance
    };
    static_assert(sizeof(memory_pool_obj_base<char, true>) == CACHE_LINE_SIZE, "invalid memory_pool_obj_base struct size");
    static_assert(bq::is_pod<memory_pool_obj_base<char, true>>::value, "invalid memory_pool_obj_base struct type");

    template <typename T>
    class memory_pool_obj_base<T, false> {
    public:
        static constexpr bool is_aligned = false;

    private:
        friend memory_pool<T>;
        T* memory_pool_next_;
        T* memory_pool_prev_;
        // Note: No constructor to initialize pointers, managed solely by memory_pool for performance
    };
    static_assert(sizeof(memory_pool_obj_base<char, false>) == (2 * sizeof(void*)), "invalid memory_pool_obj_base struct size");
    static_assert(bq::is_pod<memory_pool_obj_base<char, false>>::value, "invalid memory_pool_obj_base struct type");

    template <typename T>
    class memory_pool {
    private:
        bq::platform::spin_lock lock_;
        T* head_;
        T* tail_;

        static_assert(bq::is_base_of<memory_pool_obj_base<T, T::is_aligned>, T>::value,
            "T must inherit from memory_pool_obj_base");

    public:
        memory_pool()
            : head_(nullptr)
            , tail_(nullptr)
        {
        }

        /// <summary>
        /// Pushes an object into the pool (LIFO: adds to the head).
        /// </summary>
        /// <param name="object">Pointer to the object to be added; ignored if nullptr.</param>
        /// <returns></returns>
        bq_forceinline void push(T* object)
        {
            if (!object) {
                return;
            }
            bq::platform::scoped_spin_lock lock(lock_);
            object->memory_pool_next_ = head_;
            object->memory_pool_prev_ = nullptr;
            if (head_) {
                head_->memory_pool_prev_ = object;
            }
            head_ = object;
            if (!tail_) {
                tail_ = object;
            }
        }

        /// <summary>
        /// Pops an object from the pool (LIFO: removes from the head).
        /// </summary>
        /// <returns>Pointer to the removed object, or nullptr if the pool is empty.</returns>
        bq_forceinline T* pop()
        {
            bq::platform::scoped_spin_lock lock(lock_);
            if (!head_) {
                return nullptr;
            }
            T* old_head = head_;
            head_ = old_head->memory_pool_next_;
            if (head_) {
                head_->memory_pool_prev_ = nullptr;
            } else {
                tail_ = nullptr;
            }
            return old_head;
        }

        /// <summary>
        /// Evicts the least recently used object (LRU: removes from the tail) if it meets the condition.
        /// Only the tail is checked; other objects are ignored if the tail doesn't qualify.
        /// </summary>
        /// <param name="evaluate">Function pointer evaluating if the tail should be evicted (true to remove).
        ///                        *Example : [](const T* obj) { return obj->some_condition(); }
        /// </param>
        /// <param name="user_data">Datas passed to the evaluate function.
        /// </param>
        /// <returns>Pointer to the evicted object, or nullptr if empty or tail doesn't meet the condition.</returns>
        bq_forceinline T* evict(bool (*evaluate)(const T*, void* user_data), void* user_data)
        {
            bq::platform::scoped_spin_lock lock(lock_);
            if (!tail_) {
                return nullptr;
            }
            if (!evaluate(tail_, user_data)) {
                return nullptr;
            }
            T* old_tail = tail_;
            tail_ = old_tail->memory_pool_prev_;
            if (tail_) {
                tail_->memory_pool_next_ = nullptr;
            } else {
                head_ = nullptr;
            }
            return old_tail;
        }

        /// <summary>
        /// get current objects count in pool
        /// </summary>
        /// <returns></returns>
        bq_forceinline size_t size()
        {
            bq::platform::scoped_spin_lock lock(lock_);
            size_t result = 0;
            T* obj = head_;
            while (obj) {
                ++result;
                obj = obj->memory_pool_next_;
            }
            return result;
        }
    };
}