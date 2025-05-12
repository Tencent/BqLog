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
 * It's a lock free linked list, can be used for multi-thread insert and single-thread traverse and delete.
 *
 * \author pippocao
 * \date 2025/05/12
 */
#include "bq_common/bq_common.h"

namespace bq {
    template <typename T>
    class miso_linked_list {
    private:
        template <typename U>
        struct node {
            bq::platform::atomic<node<U>*> next_;
            U data_;
            template <typename... Args>
            node(node<U>* next, Args&&... args) : data_(bq::forward<Args>(args)...) {
                next_.store_raw(next);
            }
        };
    public:
        template<typename U>
        struct iterator {
            friend class miso_linked_list<U>;
        private:
            bq::platform::atomic<node<U>*>* last_ptr_;
            node<U>* current_node_;
            iterator(bq::platform::atomic<node<U>*>& last_ptr, node<U>* node) : last_ptr_(&last_ptr), current_node_(node) {}
        public:
            bq_forceinline operator bool() const { return current_node_ != nullptr; }
            bq_forceinline U& value() { return current_node_->data_;}
        };

        using iterator_type = iterator<T>;
    public:
        template<typename... Args>
        bq_forceinline iterator_type insert(Args&&... args);
        bq_forceinline iterator_type first();
        bq_forceinline iterator_type next(iterator_type& it);
        /// <summary>
        ///  This function is not thread safe, should be called in single thread.
        /// </summary>
        /// <param name="it">iterator you want to delete</param>
        /// <returns>the next iterator</returns>
        bq_forceinline iterator_type remove(iterator_type& it);
    private:
        bq::platform::atomic<node<T>*> head_ = nullptr;
    };

    template <typename T>
    template <typename... Args>
    bq_forceinline typename miso_linked_list<T>::iterator_type miso_linked_list<T>::insert(Args&&... args)
    {
        node<T>* expected_next = head_.load_raw();
        node<T>* new_node = new node<T>(expected_next, bq::forward<Args>(args)...);
        while (!head_.compare_exchange_strong(expected_next, new_node, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
            new_node->next_.store_raw(expected_next);
        }
        return iterator_type(head_, new_node);
    }

    template <typename T>
    bq_forceinline typename miso_linked_list<T>::iterator_type miso_linked_list<T>::first()
    {
        return iterator_type(head_, head_.load_acquire());
    }

    template <typename T>
    bq_forceinline typename miso_linked_list<T>::iterator_type miso_linked_list<T>::next(miso_linked_list<T>::iterator_type& it)
    {
        if (!it) {
            return it;
        }
        return iterator_type(it.current_node_->next_, it.current_node_->next_.load_raw());
    }

    template <typename T>
    bq_forceinline typename miso_linked_list<T>::iterator_type miso_linked_list<T>::remove(miso_linked_list<T>::iterator_type& it)
    {
        if (!it) {
            return it;
        }
        node<T>* next = it.current_node_->next_.load_raw();
        bq::platform::atomic<node<T>*>* last_ptr = it.last_ptr_;
        if (&head_ == last_ptr) {
            node<T>* expected_head = it.current_node_;
            if (head_.compare_exchange_strong(expected_head, next, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                delete it.current_node_;
                return iterator_type(head_, next);
            }
            last_ptr = &expected_head->next_;
            while (last_ptr->load_raw() != it.current_node_) {
                last_ptr = &last_ptr->load_raw()->next_;
            }
        } 
        last_ptr->store_raw(next);
        delete it.current_node_;
        return iterator_type(*last_ptr, next);
    }
}
