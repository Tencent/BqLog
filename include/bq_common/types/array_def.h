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
 * \file array_def.h
 * IMPORTANT: Do Not Include This Header File In Your Project!!!
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include <stdint.h>
#include <stddef.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/type_traits.h"

namespace bq {
    template <typename T, typename ARRAY>
    class BQ_ARRAY_ITER_CLS_NAME {
    public:
        template <typename V, typename V_ARRAY>
        friend class BQ_ARRAY_ITER_CLS_NAME;
        template <typename V, size_t CB>
        friend class BQ_ARRAY_CLS_NAME;
        typedef typename bq::remove_reference<T>::type value_type;
        typedef ptrdiff_t diff_type;
        typedef typename ARRAY::size_type size_type;

    private:
        value_type* data_;
        const typename ARRAY::value_type* array_data_ptr_; // when BQ_ARRAY_CLS_NAME is expanded, departed iterators will be invalid, so we need this to do validation.

    public:
        template <typename V>
        BQ_ARRAY_ITER_CLS_NAME(V* ptr, const ARRAY* array_ptr = nullptr);

        template <typename V, typename V_ARRAY>
        BQ_ARRAY_ITER_CLS_NAME(const BQ_ARRAY_ITER_CLS_NAME<V, V_ARRAY>& rhs);

        template <typename V, typename V_ARRAY>
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator=(const BQ_ARRAY_ITER_CLS_NAME<V, V_ARRAY>& rhs);

        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator==(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator!=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator<(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator<=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator>(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend bool operator>=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);
        template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
        friend typename BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>::diff_type operator-(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2);

        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator++();
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator++(int32_t);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator--();
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator--(int32_t);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator+=(int32_t value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator+=(size_type value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator+(int32_t value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator+(size_type value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator-=(int32_t value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& operator-=(size_type value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator-(int32_t value);
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> operator-(size_type value);

        value_type& operator*() const;

        value_type* operator->() const;

        operator value_type*() const;
    };

    template <typename T, size_t TAIL_BUFFER_SIZE = 0>
    class BQ_ARRAY_CLS_NAME {
    public:
        using value_type = typename bq::decay<T>::type;
        using size_type = size_t;
        using iterator = BQ_ARRAY_ITER_CLS_NAME<value_type, BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>>;
        using const_iterator = BQ_ARRAY_ITER_CLS_NAME<const value_type, BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>>;

        template <typename V, size_t TAIL_BUFFER_SIZE_V>
        friend class BQ_ARRAY_CLS_NAME;
        friend class BQ_ARRAY_ITER_CLS_NAME<value_type, BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>>;
        friend class BQ_ARRAY_ITER_CLS_NAME<const value_type, BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>>;

    protected:
        value_type* data_;
        size_type size_;
        size_type capacity_;

    public:
        BQ_ARRAY_CLS_NAME();

        ~BQ_ARRAY_CLS_NAME();

        template <typename First, typename Second, typename... Rest>
        BQ_ARRAY_CLS_NAME(First&& first, Second&& second, Rest&&... rest);

        BQ_ARRAY_CLS_NAME(const T& rhs);

        BQ_ARRAY_CLS_NAME(T&& rhs) noexcept;

        BQ_ARRAY_CLS_NAME(const BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>& rhs);

        BQ_ARRAY_CLS_NAME(BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>&& rhs) noexcept;

        size_type size() const;

        size_type capacity() const;

        bool is_empty() const;

        template <typename IDX_TYPE>
        value_type& operator[](IDX_TYPE idx);

        template <typename IDX_TYPE>
        const value_type& operator[](IDX_TYPE idx) const;

        template <typename T1, typename T2, size_t S1, size_t S2>
        friend bool operator==(const BQ_ARRAY_CLS_NAME<T1, S1>& array1, const BQ_ARRAY_CLS_NAME<T2, S2>& array2);

        template <typename T1, typename T2, size_t S1, size_t S2>
        friend bool operator!=(const BQ_ARRAY_CLS_NAME<T1, S1>& array1, const BQ_ARRAY_CLS_NAME<T2, S2>& array2);

        BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>& operator=(const BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>& rhs);

        BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>& operator=(BQ_ARRAY_CLS_NAME<T, TAIL_BUFFER_SIZE>&& rhs) noexcept;

        template <typename... V>
        void insert(iterator dest_it, V&&... args);

        void insert_batch(iterator dest_it, const_iterator src_it, size_type count);

        template <typename... V>
        size_type push_back(V&&... args);

        template <typename... V>
        size_type emplace(iterator where_it, V&&... args);

        template <typename... V>
        size_type emplace_back(V&&... args);

        template <typename U = T>
        typename bq::enable_if<bq::is_pod<U>::value>::type fill_uninitialized(size_type count);

        void pop_back();

        void pop_back(size_type count);

        void erase(iterator where_it, size_type count = 1);

        // remove element and move last element to here.
        void erase_replace(iterator where_it);

        iterator begin();

        const_iterator begin() const;

        iterator end();

        const_iterator end() const;

        void shrink();

        void clear();

        template <typename U = T>
        typename bq::enable_if<bq::is_pod<U>::value>::type reset();

        template <typename U = T>
        typename bq::enable_if<!bq::is_pod<U>::value>::type reset();

        bool set_capacity(size_type new_capacity, bool force_reset = false);

        iterator find(const value_type& value, bool reverse_find = false);

        const_iterator find(const value_type& value, bool reverse_find = false) const;

        template <typename Predicate>
        iterator find_if(Predicate predicate, bool reverse_find = false);

        template <typename Predicate>
        const_iterator find_if(Predicate predicate, bool reverse_find = false) const;

    protected:
        template <typename... V>
        void construct(iterator iter, V&&... args);

        void destruct(iterator iter);

        template <typename First>
        void inner_args_insert(First&& first);

        template <typename First, typename... Rest>
        void inner_args_insert(First&& first, Rest&&... rest);
    };

}
