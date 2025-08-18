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
 * \file hash_map_impl.h
 * IMPORTANT: Do Not Include This Header File In Your Project!!!
 *
 * \author pippocao
 * \date 2022/09/10
 *
 */
#include <string.h>
#include "bq_common/misc/assert.h"
#include "bq_common/types/type_tools.h"
#include "bq_common/types/hash_map_def.h"

namespace bq {
    /*===========================================================BQ_HASH_MAP_ITER_CLS_NAME==============================================================*/
    template <typename K, typename V, typename Allocator, bool C>
    template <bool C_>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>& BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator=(const BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C_>& rhs)
    {
        parent_ = rhs.parent_;
        node_index_ = rhs.node_index_;
        bucket_idx_ = rhs.bucket_idx_;
        return *this;
    }

    template <typename K_, typename V_, typename Allocator1, typename Allocator2, bool C1, bool C2>
    BQ_HASH_MAP_INLINE bool operator==(const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator1, C1>& map1, const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator2, C2>& map2)
    {
        if (map1.node_index_ == BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator1, C1>::BQ_HASH_MAP_INVALID_INDEX
            && map2.node_index_ == BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator2, C2>::BQ_HASH_MAP_INVALID_INDEX) {
            return true;
        }
        return (map1.node_index_ == map2.node_index_)
            && (map1.parent_ == map2.parent_)
            && (map1.bucket_idx_ == map2.bucket_idx_);
    }

    template <typename K_, typename V_, typename Allocator1, typename Allocator2, bool C1, bool C2>
    BQ_HASH_MAP_INLINE bool operator!=(const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator1, C1>& map1, const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, Allocator2, C2>& map2)
    {
        return !(map1 == map2);
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>& BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator++()
    {
        node_index_ = parent_->get_next_node_index(node_index_);
        bucket_idx_ = (node_index_ == BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::BQ_HASH_MAP_INVALID_INDEX) ? node_index_ : parent_->nodes_[node_index_].bucket_idx;
        return *this;
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C> BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator++(int32_t)
    {
        BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C> next_iter = *this;
        operator++();
        return next_iter;
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator bool() const
    {
        return parent_->iterator_legal_check(*this);
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::pair_type_ref BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator*()
    {
        return parent_->nodes_[node_index_].entry;
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::pair_type_ptr BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator->()
    {
        return &parent_->nodes_[node_index_].entry;
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::const_pair_type_ref BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator*() const
    {
        return parent_->nodes_[node_index_].entry;
    }

    template <typename K, typename V, typename Allocator, bool C>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::const_pair_type_ptr BQ_HASH_MAP_ITER_CLS_NAME<K, V, Allocator, C>::operator->() const
    {
        return &parent_->nodes_[node_index_].entry;
    }

    /*===========================================================BQ_HASH_MAP_CLS_NAME==============================================================*/
    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::BQ_HASH_MAP_CLS_NAME(size_type init_bucket_size /* = 0 */)
    {
        size_ = 0;
        head_ = BQ_HASH_MAP_INVALID_INDEX;
        tail_ = BQ_HASH_MAP_INVALID_INDEX;
        free_ = BQ_HASH_MAP_INVALID_INDEX;
        expand_buckets(init_bucket_size);
        expand_nodes(buckets_size());
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::~BQ_HASH_MAP_CLS_NAME()
    {
        reset();
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::BQ_HASH_MAP_CLS_NAME(const BQ_HASH_MAP_CLS_NAME<K, V, Allocator>& rhs)
        : buckets_(decltype(buckets_)())
        , nodes_(decltype(nodes_)())
    {
        size_ = 0;
        head_ = BQ_HASH_MAP_INVALID_INDEX;
        tail_ = BQ_HASH_MAP_INVALID_INDEX;
        free_ = BQ_HASH_MAP_INVALID_INDEX;
        expand_buckets(rhs.buckets_size());
        expand_nodes(rhs.nodes_size());
        for (BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::const_iterator iter = rhs.begin(); iter != rhs.end(); ++iter) {
            add(iter->key(), iter->value());
        }
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::BQ_HASH_MAP_CLS_NAME(BQ_HASH_MAP_CLS_NAME<K, V, Allocator>&& rhs)
    {
        size_ = rhs.size_;
        head_ = rhs.head_;
        tail_ = rhs.tail_;
        free_ = rhs.free_;
        buckets_.data_ = rhs.buckets_.data_;
        buckets_.size_ = rhs.buckets_.size_;
        nodes_.data_ = rhs.nodes_.data_;
        nodes_.size_ = rhs.nodes_.size_;

        rhs.size_ = 0;
        rhs.head_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.tail_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.free_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.buckets_.data_ = nullptr;
        rhs.buckets_.size_ = 0;
        rhs.nodes_.data_ = nullptr;
        rhs.nodes_.size_ = 0;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size() const
    {
        return size_;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::buckets_size() const
    {
        return static_cast<size_type>(buckets_.size());
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::nodes_size() const
    {
        return static_cast<size_type>(nodes_.size());
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE bool BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::is_empty() const
    {
        return size_ == 0;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::value_type& BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::operator[](const_key_type_ref key)
    {
        auto iter = find(key);
        if (!iter) {
            return add(key)->value();
        }
        return iter->value();
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::find(const_key_type_ref key)
    {
        auto node_bucket_indices_pair = find_index_and_bucket_idx_by_key(key);
        return iterator(this, node_bucket_indices_pair.key(), node_bucket_indices_pair.value());
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::const_iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::find(const_key_type_ref key) const
    {
        auto node_bucket_indices_pair = find_index_and_bucket_idx_by_key(key);
        return const_iterator(this, node_bucket_indices_pair.key(), node_bucket_indices_pair.value());
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::add(const_key_type_ref key)
    {
        iterator iter = alloc_node_by_key(key);
        construct_value(iter);
        ++size_;
        return iter;
    }

    template <typename K, typename V, typename Allocator>
    template <typename V_>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::add(const_key_type_ref key, V_&& value)
    {
        iterator iter = alloc_node_by_key(key);
        construct_value(iter, bq::forward<V_>(value));
        ++size_;
        return iter;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>& BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::operator=(const BQ_HASH_MAP_CLS_NAME<K, V, Allocator>& rhs)
    {
        clear();
        expand_buckets(rhs.buckets_size());
        expand_nodes(rhs.nodes_size());
        for (BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::const_iterator iter = rhs.begin(); iter != rhs.end(); ++iter) {
            add(iter->key(), iter->value());
        }
        return *this;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_CLS_NAME<K, V, Allocator>& BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::operator=(BQ_HASH_MAP_CLS_NAME<K, V, Allocator>&& rhs)
    {
        reset();

        size_ = rhs.size_;
        head_ = rhs.head_;
        tail_ = rhs.tail_;
        free_ = rhs.free_;
        buckets_.data_ = rhs.buckets_.data_;
        buckets_.size_ = rhs.buckets_.size_;
        nodes_.data_ = rhs.nodes_.data_;
        nodes_.size_ = rhs.nodes_.size_;

        rhs.size_ = 0;
        rhs.head_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.tail_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.free_ = BQ_HASH_MAP_INVALID_INDEX;
        rhs.buckets_.data_ = nullptr;
        rhs.buckets_.size_ = 0;
        rhs.nodes_.data_ = nullptr;
        rhs.nodes_.size_ = 0;

        return *this;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE bool BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::erase(iterator where_it)
    {
        if (!iterator_legal_check(where_it)) {
            return false;
        }

        // nodes
        auto& cur_node = nodes_[where_it.node_index_];
        size_type prev_index = cur_node.prev;
        size_type next_index = cur_node.next;
        if (prev_index != BQ_HASH_MAP_INVALID_INDEX) {
            nodes_[prev_index].next = cur_node.next;
        } else {
            head_ = cur_node.next;
        }
        if (next_index != BQ_HASH_MAP_INVALID_INDEX) {
            nodes_[next_index].prev = cur_node.prev;
        } else {
            tail_ = cur_node.prev;
        }

        // buckets
        size_type bucket_index = where_it.bucket_idx_;
        if (where_it.node_index_ == buckets_[bucket_index]) {
            if (next_index != BQ_HASH_MAP_INVALID_INDEX
                && nodes_[next_index].bucket_idx == bucket_index) {
                buckets_[bucket_index] = next_index;
            } else {
                buckets_[bucket_index] = BQ_HASH_MAP_INVALID_INDEX;
            }
        }

        // return to free list
        cur_node.next = free_;
        free_ = where_it.node_index_;
        --size_;

        destruct(where_it);
        return true;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE bool BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::erase(const_key_type_ref key)
    {
        auto iter = find(key);
        if (!iter) {
            return false;
        }
        return erase(iter);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::begin()
    {
        if (head_ != BQ_HASH_MAP_INVALID_INDEX) {
            return iterator(this, head_, nodes_[head_].bucket_idx);
        }
        return end();
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::const_iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::begin() const
    {
        if (head_ != BQ_HASH_MAP_INVALID_INDEX) {
            return const_iterator(this, head_, nodes_[head_].bucket_idx);
        }
        return end();
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::end()
    {
        return iterator(this, BQ_HASH_MAP_INVALID_INDEX, BQ_HASH_MAP_INVALID_INDEX);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::const_iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::end() const
    {
        return const_iterator(this, BQ_HASH_MAP_INVALID_INDEX, BQ_HASH_MAP_INVALID_INDEX);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE bool BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator_legal_check(const const_iterator& iter) const
    {
        if (iter.node_index_ == BQ_HASH_MAP_INVALID_INDEX) {
            return false;
        }
        if (iter.parent_ != this) {
            return false;
        }
        if (iter.node_index_ >= this->nodes_.size()) {
            return false;
        }
        if (this->nodes_[iter.node_index_].bucket_idx != iter.bucket_idx_) {
            return false;
        }
        return true;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::set_expand_rate(float rate)
    {
        expand_rate_ = rate;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE float BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::get_expand_rate() const
    {
        return expand_rate_;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::clear()
    {
        for (auto iter = begin(); iter != end(); ++iter) {
            destruct(iter);
        }
        if (buckets_size() > 0) {
            memset(&buckets_[0], 0xFF, buckets_size() * sizeof(typename decltype(buckets_)::value_type));
        }
        if (nodes_size() > 0) {
            free_ = 0;
            for (typename decltype(nodes_)::size_type i = 0; i < nodes_.size(); ++i) {
                nodes_[i].prev = BQ_HASH_MAP_INVALID_INDEX;
                nodes_[i].next = static_cast<size_type>(i) + 1;
            }
            nodes_[nodes_.size() - 1].next = BQ_HASH_MAP_INVALID_INDEX;
        } else {
            free_ = BQ_HASH_MAP_INVALID_INDEX;
        }
        size_ = 0;
        tail_ = BQ_HASH_MAP_INVALID_INDEX;
        head_ = BQ_HASH_MAP_INVALID_INDEX;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::reset()
    {
        for (auto iter = begin(); iter != end(); ++iter) {
            destruct(iter);
        }
        if (buckets_.data_) {
            buckets_allocator_.deallocate(buckets_.data_, buckets_.size_);
            buckets_.data_ = nullptr;
            buckets_.size_ = 0;
        }
        if (nodes_.data_) {
            nodes_allocator_.deallocate(nodes_.data_, nodes_.size_);
            nodes_.data_ = nullptr;
            nodes_.size_ = 0;
        }
        size_ = 0;
        head_ = BQ_HASH_MAP_INVALID_INDEX;
        tail_ = BQ_HASH_MAP_INVALID_INDEX;
        free_ = BQ_HASH_MAP_INVALID_INDEX;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::construct_value(iterator& iter)
    {
        new (&(iter->value()), bq::enum_new_dummy::dummy) V();
    }

    template <typename K, typename V, typename Allocator>
    template <typename V_>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::construct_value(iterator& iter, V_&& arg)
    {
        new (&(iter->value()), bq::enum_new_dummy::dummy) V(bq::forward<V_>(arg));
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::destruct_value(iterator& iter)
    {
        bq::object_destructor<V>::destruct(&iter->value());
    }

    template <typename K, typename V, typename Allocator>
    template <typename K_>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::construct_key(iterator& iter, K_&& key)
    {
        new (&iter->key_, bq::enum_new_dummy::dummy) K(bq::forward<K_>(key));
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::destruct_key(iterator& iter)
    {
        bq::object_destructor<K>::destruct(&iter->key_);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::destruct(iterator& iter)
    {
        destruct_key(iter);
        destruct_value(iter);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::get_bucket_index_by_key(const_key_type_ref key) const
    {
        auto hash_code = hash_calculate_type::hash_code(key);
        return get_bucket_index_by_hash(hash_code);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::get_bucket_index_by_hash(typename hash_calculate_type::hash_value_type hash_code) const
    {
        return static_cast<size_type>(hash_code) & (buckets_size() - 1);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::get_prev_node_index(typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type node_index) const
    {
        if (node_index == BQ_HASH_MAP_INVALID_INDEX) {
            return BQ_HASH_MAP_INVALID_INDEX;
        }
        return nodes_[node_index].prev;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::get_next_node_index(typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type node_index) const
    {
        if (node_index == BQ_HASH_MAP_INVALID_INDEX) {
            return BQ_HASH_MAP_INVALID_INDEX;
        }
        return nodes_[node_index].next;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::expand_buckets(typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type size)
    {
        if (size <= buckets_size()) {
            return;
        }
        size_type capacity = bq::roundup_pow_of_two(size);
        if (buckets_.size() > 0) {
            buckets_allocator_.deallocate(buckets_.data_, buckets_.size_);
        }
        buckets_.data_ = buckets_allocator_.allocate(capacity);
        buckets_.size_ = capacity;
        memset(&buckets_[0], 0xFF, capacity * sizeof(typename decltype(buckets_)::value_type));

        if (buckets_size() > 0) {
#ifndef NDEBUG
            size_type cnt = 0;
#endif
            size_type index = head_;
            while (index != BQ_HASH_MAP_INVALID_INDEX) {
#ifndef NDEBUG
                cnt++;
#endif
                size_type cur_index = index;
                auto& cur_node = nodes_[cur_index];
                index = cur_node.next;

                size_type bucket_idx = get_bucket_index_by_key(cur_node.entry.key());
                cur_node.bucket_idx = bucket_idx;
                size_type& bucket_value_ref = buckets_[bucket_idx];
                if (bucket_value_ref == BQ_HASH_MAP_INVALID_INDEX) {
                    bucket_value_ref = cur_index;
                } else if (cur_node.prev != bucket_value_ref) {
                    size_type old_prev = cur_node.prev;
                    size_type old_next = cur_node.next;

                    size_type new_prev = bucket_value_ref;
                    size_type new_next = nodes_[new_prev].next;

                    if (old_prev != BQ_HASH_MAP_INVALID_INDEX) {
                        nodes_[old_prev].next = old_next;
                    }
                    if (old_next != BQ_HASH_MAP_INVALID_INDEX) {
                        nodes_[old_next].prev = old_prev;
                    }
                    cur_node.prev = new_prev;
                    cur_node.next = new_next;
                    nodes_[new_prev].next = cur_index;
                    if (new_next != BQ_HASH_MAP_INVALID_INDEX) {
                        nodes_[new_next].prev = cur_index;
                    }
                    if (tail_ == cur_index) {
                        tail_ = old_prev;
                    }
                    if (cur_node.prev == cur_node.next) {
                        tail_ = BQ_HASH_MAP_INVALID_INDEX;
                    }
                }
            }
#ifndef NDEBUG
            assert(buckets_size() == capacity);
            assert(cnt == this->size());
            if (head_ != BQ_HASH_MAP_INVALID_INDEX) {
                assert(nodes_[head_].prev == BQ_HASH_MAP_INVALID_INDEX);
            }
            if (tail_ != BQ_HASH_MAP_INVALID_INDEX) {
                assert(nodes_[tail_].next == BQ_HASH_MAP_INVALID_INDEX);
            }
#endif
        }
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE void BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::expand_nodes(typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type size)
    {
        size_type old_space = static_cast<size_type>(nodes_.size());
        if (size <= old_space) {
            return;
        }
        BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type new_space = bq::roundup_pow_of_two(size);
        using alloc_type = typename decltype(nodes_)::value_type;
        alloc_type* new_data = nodes_allocator_.allocate(new_space);
        for (size_type i = 0; i < old_space; ++i) {
            new_data[i].bucket_idx = nodes_[i].bucket_idx;
            new_data[i].prev = nodes_[i].prev;
            new_data[i].next = nodes_[i].next;
        }
        for (size_type i = new_space; i > old_space; --i) {
            new_data[i - 1].prev = BQ_HASH_MAP_INVALID_INDEX;
            new_data[i - 1].next = free_;
            free_ = i - 1;
        }
        for (auto iter = begin(); iter != end(); ++iter) {
            new (&(new_data[iter.node_index_].entry.key_), bq::enum_new_dummy::dummy) key_type(bq::move(iter.operator*().key_));
            new (&(new_data[iter.node_index_].entry.value_), bq::enum_new_dummy::dummy) value_type(bq::move(iter.operator*().value_));
            destruct(iter);
        }
        if (nodes_.data_) {
            nodes_allocator_.deallocate(nodes_.data_, nodes_.size_);
        }
        nodes_.data_ = new_data;
        nodes_.size_ = new_space;
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE BQ_HASH_MAP_KV_CLS_NAME<typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type, typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::size_type> BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::find_index_and_bucket_idx_by_key(const_key_type_ref key) const
    {
        size_type index = BQ_HASH_MAP_INVALID_INDEX;
        size_type bucket_idx = BQ_HASH_MAP_INVALID_INDEX;
        if (buckets_size() > 0) {
            bucket_idx = get_bucket_index_by_key(key);
            size_type valid_index = buckets_[bucket_idx];
            while (valid_index != BQ_HASH_MAP_INVALID_INDEX) {
                auto& node = nodes_[valid_index];
                if (node.bucket_idx != bucket_idx) {
                    valid_index = BQ_HASH_MAP_INVALID_INDEX;
                    break;
                }
                if (node.entry.key() == key) {
                    index = valid_index;
                    break;
                }
                valid_index = node.next;
            }
        }
        return BQ_HASH_MAP_KV_CLS_NAME<size_type, size_type>(index, bucket_idx);
    }

    template <typename K, typename V, typename Allocator>
    BQ_HASH_MAP_INLINE typename BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::iterator BQ_HASH_MAP_CLS_NAME<K, V, Allocator>::alloc_node_by_key(const_key_type_ref key)
    {
        uint32_t expected_bucket_size = static_cast<uint32_t>((static_cast<float>((size() + 1)) * expand_rate_)) + 1;
        if (expected_bucket_size > buckets_size()) {
            expand_buckets(expected_bucket_size);
        }
        // This must be called after expand_buckets because it will reform the buckets and nodes.
        auto node_bucket_indices_pair = find_index_and_bucket_idx_by_key(key);
        assert(node_bucket_indices_pair.key() == BQ_HASH_MAP_INVALID_INDEX && "key already exist");
        auto bucket_idx = node_bucket_indices_pair.value();
        if (free_ == BQ_HASH_MAP_INVALID_INDEX) {
            expand_nodes(nodes_size() + 1);
        }

        size_type& target_bucket_value = buckets_[bucket_idx];
        size_type next_node_index = target_bucket_value;
        size_type prev_node_index = (target_bucket_value == BQ_HASH_MAP_INVALID_INDEX) ? tail_ : nodes_[target_bucket_value].prev;
        assert(free_ != BQ_HASH_MAP_INVALID_INDEX);

        size_type new_index = free_;
        free_ = nodes_[new_index].next;
        auto& new_node = nodes_[new_index];
        target_bucket_value = new_index;
        new_node.next = next_node_index;
        new_node.prev = prev_node_index;
        new_node.bucket_idx = bucket_idx;
        if (prev_node_index != BQ_HASH_MAP_INVALID_INDEX) {
            auto& prev_node = nodes_[prev_node_index];
            prev_node.next = new_index;
        } else {
            head_ = new_index;
        }
        if (next_node_index != BQ_HASH_MAP_INVALID_INDEX) {
            auto& next_node = nodes_[next_node_index];
            next_node.prev = new_index;
        } else {
            tail_ = new_index;
        }

        iterator alloc_iter(this, new_index, bucket_idx);
        construct_key(alloc_iter, key);
        return alloc_iter;
    }
}
