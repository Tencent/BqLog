/*
 * Copyright (C) 2024 Tencent.
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
 * \file hash_map_def.h
 * IMPORTANT: Do Not Include This Header File In Your Project!!!
 *
 * \author pippocao
 * \date 2022/09/10
 *
 */
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include "bq_common/misc/assert.h"
#include "bq_common/types/type_traits.h"
#include "bq_common/types/type_tools.h"

namespace bq {
    template <typename K, typename V>
    class BQ_HASH_MAP_CLS_NAME;

    template <typename K, typename V>
    struct BQ_HASH_MAP_KV_CLS_NAME {
    private:
        template <typename K_, typename V_>
        friend class BQ_HASH_MAP_CLS_NAME;
        K key_;
        V value_;

    public:
        template <typename K_, typename V_>
        BQ_HASH_MAP_KV_CLS_NAME(K_&& in_key, V_&& in_value)
            : key_(bq::forward<K_>(in_key))
            , value_(bq::forward<V_>(in_value))
        {
        }

        BQ_HASH_MAP_INLINE const K& key() const
        {
            return key_;
        }

        BQ_HASH_MAP_INLINE V& value()
        {
            return value_;
        }

        BQ_HASH_MAP_INLINE const V& value() const
        {
            return value_;
        }
    };

    template <typename K, typename V, bool C>
    class BQ_HASH_MAP_ITER_CLS_NAME {
    public:
        friend class BQ_HASH_MAP_ITER_CLS_NAME<K, V, true>;
        friend class BQ_HASH_MAP_ITER_CLS_NAME<K, V, false>;
        friend class BQ_HASH_MAP_CLS_NAME<K, V>;
        typedef K key_type;
        typedef V value_type;
        typedef typename bq::condition_type<C, const BQ_HASH_MAP_CLS_NAME<K, V>*, BQ_HASH_MAP_CLS_NAME<K, V>*>::type container_type_ptr;
        typedef typename BQ_HASH_MAP_CLS_NAME<K, V>::size_type size_type;
        typedef typename BQ_HASH_MAP_CLS_NAME<K, V>::pair_type pair_type;
        typedef typename bq::condition_type<C, const pair_type*, pair_type*>::type pair_type_ptr;
        typedef typename bq::condition_type<C, const pair_type&, pair_type&>::type pair_type_ref;
        typedef const pair_type* const_pair_type_ptr;
        typedef const pair_type& const_pair_type_ref;

    private:
        constexpr static size_type BQ_HASH_MAP_INVALID_INDEX = (size_type)-1;
        container_type_ptr parent_;
        size_type node_index_;
        size_type bucket_idx_;

    private:
        BQ_HASH_MAP_ITER_CLS_NAME() = delete;

        BQ_HASH_MAP_ITER_CLS_NAME(container_type_ptr parent, size_type index, size_type bucket_idx)
            : parent_(parent)
            , node_index_(index)
            , bucket_idx_(bucket_idx)
        {
        }

    public:
        template <bool C_>
        BQ_HASH_MAP_ITER_CLS_NAME(const BQ_HASH_MAP_ITER_CLS_NAME<K, V, C_>& rhs)
            : parent_((container_type_ptr)rhs.parent_)
            , node_index_(rhs.node_index_)
            , bucket_idx_(rhs.bucket_idx_)
        {
        }

        template <bool C_>
        BQ_HASH_MAP_ITER_CLS_NAME<K, V, C>& operator=(const BQ_HASH_MAP_ITER_CLS_NAME<K, V, C_>& rhs);

        template <typename K_, typename V_, bool C1, bool C2>
        friend bool operator==(const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, C1>& map1, const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, C2>& map2);
        template <typename K_, typename V_, bool C1, bool C2>
        friend bool operator!=(const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, C1>& map1, const BQ_HASH_MAP_ITER_CLS_NAME<K_, V_, C2>& map2);

        BQ_HASH_MAP_ITER_CLS_NAME<K, V, C>& operator++();

        BQ_HASH_MAP_ITER_CLS_NAME<K, V, C> operator++(int32_t);

        explicit operator bool() const;

        pair_type_ref operator*();

        pair_type_ptr operator->();

        const_pair_type_ref operator*() const;

        const_pair_type_ptr operator->() const;
    };

    template <typename K, typename V>
    class BQ_HASH_MAP_CLS_NAME {
    public:
        typedef typename bq::remove_reference<K>::type key_type;
        typedef typename bq::remove_reference<V>::type value_type;
        typedef const key_type& const_key_type_ref;
        typedef uint32_t size_type;
        typedef BQ_HASH_MAP_ITER_CLS_NAME<key_type, value_type, false> iterator;
        typedef BQ_HASH_MAP_ITER_CLS_NAME<key_type, value_type, true> const_iterator;
        typedef typename bq::condition_type<
            bq::is_pointer<key_type>::value,
            typename bq::pointer_type_hash_calculator<key_type>,
            typename bq::condition_type<
                bq::is_basic_types<key_type>::value,
                typename bq::number_type_hash_calculator<key_type>,
                typename bq::generic_type_hash_calculator<key_type>>::type>::type hash_calculate_type;
        typedef BQ_HASH_MAP_KV_CLS_NAME<key_type, value_type> pair_type;
        friend class BQ_HASH_MAP_ITER_CLS_NAME<key_type, value_type, true>;
        friend class BQ_HASH_MAP_ITER_CLS_NAME<key_type, value_type, false>;

    protected:
        struct value_node {
            BQ_HASH_MAP_CLS_NAME<K, V>::size_type prev;
            BQ_HASH_MAP_CLS_NAME<K, V>::size_type next;
            BQ_HASH_MAP_CLS_NAME<K, V>::size_type bucket_idx;
            typename iterator::pair_type entry;
        };

        template <typename T>
        struct value_node_buffer_head {
            typedef T value_type;
            typedef BQ_HASH_MAP_CLS_NAME<K, V>::size_type size_type;
            T* data_;
            size_type size_;
            value_node_buffer_head()
                : data_(nullptr)
                , size_(0)
            {
            }
            BQ_HASH_MAP_CLS_NAME<K, V>::size_type size() const { return size_; }
            T& operator[](BQ_HASH_MAP_CLS_NAME<K, V>::size_type idx)
            {
                assert(idx < size_);
                return data_[idx];
            }
            const T& operator[](BQ_HASH_MAP_CLS_NAME<K, V>::size_type idx) const
            {
                assert(idx < size_);
                return data_[idx];
            }
        };

        typedef value_node node_type;
        value_node_buffer_head<size_type> buckets_;
        // we need manually control the destructive behaviour of every node item. bq::array didn't meets our needs because it has its own destructive logic.
        value_node_buffer_head<node_type> nodes_;

        constexpr static size_type BQ_HASH_MAP_INVALID_INDEX = (size_type)-1;
        size_type size_;
        size_type head_;
        size_type tail_;
        size_type free_;
        float expand_rate_ = 1.5; // bucket expand rate

    public:
        BQ_HASH_MAP_CLS_NAME(size_type init_bucket_size = 0);

        ~BQ_HASH_MAP_CLS_NAME();

        BQ_HASH_MAP_CLS_NAME(const BQ_HASH_MAP_CLS_NAME<K, V>& rhs);

        BQ_HASH_MAP_CLS_NAME(BQ_HASH_MAP_CLS_NAME<K, V>&& rhs);

        size_type size() const;

        size_type buckets_size() const;

        size_type nodes_size() const;

        bool is_empty() const;

        value_type& operator[](const_key_type_ref key);

        iterator find(const_key_type_ref key);

        const_iterator find(const_key_type_ref key) const;

        iterator add(const_key_type_ref key);

        template <typename V_>
        iterator add(const_key_type_ref key, V_&& value);

        BQ_HASH_MAP_CLS_NAME<K, V>& operator=(const BQ_HASH_MAP_CLS_NAME<K, V>& rhs);

        BQ_HASH_MAP_CLS_NAME<K, V>& operator=(BQ_HASH_MAP_CLS_NAME<K, V>&& rhs);

        bool erase(iterator where_it);

        bool erase(const_key_type_ref key);

        iterator begin();

        const_iterator begin() const;

        iterator end();

        const_iterator end() const;

        bool iterator_legal_check(const const_iterator& iter) const;

        void set_expand_rate(float rate);

        float get_expand_rate() const;

        void clear();

        void reset();

    protected:
        void construct_value(iterator& iter);

        template <typename V_>
        void construct_value(iterator& iter, V_&& arg);

        void destruct_value(iterator& iter);

        template <typename K_>
        void construct_key(iterator& iter, K_&& key);

        void destruct_key(iterator& iter);

        void destruct(iterator& iter);

        size_type get_bucket_index_by_key(const_key_type_ref key) const;

        size_type get_bucket_index_by_hash(typename hash_calculate_type::hash_value_type hash_code) const;

        size_type get_prev_node_index(size_type node_index) const;

        size_type get_next_node_index(size_type node_index) const;

        void expand_buckets(size_type size);

        void expand_nodes(size_type size);

        BQ_HASH_MAP_KV_CLS_NAME<size_type, size_type> find_index_and_bucket_idx_by_key(const_key_type_ref key) const;

        // key should not exist before call this function
        iterator alloc_node_by_key(const_key_type_ref key);
    };

}
