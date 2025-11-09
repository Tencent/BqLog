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
 * \file array_impl.h
 * IMPORTANT: Do Not Include This Header File In Your Project!!!
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bq_common/misc/assert.h"
#include "bq_common/types/type_tools.h"
#include "bq_common/types/array_def.h"

namespace bq {
    /*===========================================================BQ_ARRAY_ITER_CLS_NAME==============================================================*/
    template <typename T, typename ARRAY>
    template <typename V>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::BQ_ARRAY_ITER_CLS_NAME(V* ptr, const ARRAY* array_ptr /*= nullptr*/)
    {
        data_ = ptr;
        array_data_ptr_ = array_ptr ? array_ptr->data_ : nullptr;
    }

    template <typename T, typename ARRAY>
    template <typename V, typename V_ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::BQ_ARRAY_ITER_CLS_NAME(const BQ_ARRAY_ITER_CLS_NAME<V, V_ARRAY>& rhs)
    {
        data_ = rhs.data_;
        array_data_ptr_ = rhs.array_data_ptr_;
    }

    template <typename T, typename ARRAY>
    template <typename V, typename V_ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator=(const BQ_ARRAY_ITER_CLS_NAME<V, V_ARRAY>& rhs)
    {
        data_ = rhs.data_;
        array_data_ptr_ = rhs.array_data_ptr_;
        return *this;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator==(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ == array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator!=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ != array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator<(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ < array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator<=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ <= array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator>(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ > array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE bool operator>=(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not compare two bq::array_iterator generated from different bq::array");
        return array1.data_ >= array2.data_;
    }

    template <typename T1, typename T2, typename V_ARRAY1, typename V_ARRAY2>
    BQ_ARRAY_INLINE typename BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>::difference_type operator-(const BQ_ARRAY_ITER_CLS_NAME<T1, V_ARRAY1>& array1, const BQ_ARRAY_ITER_CLS_NAME<T2, V_ARRAY2>& array2)
    {
        assert((array1.array_data_ptr_ == array2.array_data_ptr_) && "you can not sub two bq::array_iterator generated from different bq::array");
        return array1.data_ - array2.data_;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator++()
    {
        data_ += 1;
        return *this;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator++(int32_t)
    {
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> tmp = *this;
        operator++();
        return tmp;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator--()
    {
        data_ -= 1;
        return *this;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator--(int32_t)
    {
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> tmp = *this;
        operator--();
        return tmp;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator+=(difference_type value)
    {
        data_ += value;
        return *this;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator+(difference_type value)
    {
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> result = *this;
        result += value;
        return result;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator-=(difference_type value)
    {
        data_ -= value;
        return *this;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator-(difference_type value)
    {
        BQ_ARRAY_ITER_CLS_NAME<T, ARRAY> result = *this;
        result -= value;
        return result;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE typename BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::value_type& BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator*() const
    {
        return *data_;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE typename BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::value_type* BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator->() const
    {
        return data_;
    }

    template <typename T, typename ARRAY>
    BQ_ARRAY_INLINE BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::operator typename BQ_ARRAY_ITER_CLS_NAME<T, ARRAY>::value_type*() const
    {
        return data_;
    }

    /*===========================================================BQ_ARRAY_CLS_NAME==============================================================*/
    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME()
        : data_(nullptr)
        , size_(0)
        , capacity_(0)
    {
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::~BQ_ARRAY_CLS_NAME()
    {
        reset();
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename First, typename Second, typename... Rest>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME(First&& first, Second&& second, Rest&&... rest)
        : data_(nullptr)
        , size_(0)
        , capacity_(sizeof...(rest) + 2)
    {
        set_capacity(capacity_, true);
        inner_args_insert(bq::forward<First>(first), bq::forward<Second>(second), bq::forward<Rest>(rest)...);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME(const T& rhs)
        : data_(nullptr)
        , size_(0)
        , capacity_(0)
    {
        insert(end(), rhs);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME(T&& rhs) noexcept
        : data_(nullptr)
        , size_(0)
        , capacity_(0)
    {
        insert(end(), bq::move(rhs));
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME(const BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>& rhs)
        : data_(nullptr)
        , size_(0)
        , capacity_(rhs.size())
    {
        set_capacity(capacity_, true);
        insert_batch(begin(), rhs.begin(), rhs.size_);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::BQ_ARRAY_CLS_NAME(BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>&& rhs) noexcept
        : size_(rhs.size_)
        , capacity_(rhs.capacity())
    {
        data_ = rhs.data_;
        rhs.data_ = nullptr;
        rhs.size_ = 0;
        rhs.capacity_ = 0;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size() const
    {
        return size_;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::capacity() const
    {
        return capacity_;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE bool BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::is_empty() const
    {
        return size_ == 0;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename IDX_TYPE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type& BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::operator[](IDX_TYPE idx)
    {
#ifndef NDEBUG
        assert(static_cast<size_type>(idx) < size_);
#endif
        return data_[static_cast<size_type>(idx)];
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename IDX_TYPE>
    BQ_ARRAY_INLINE const typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type& BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::operator[](IDX_TYPE idx) const
    {
#ifndef NDEBUG
        assert(static_cast<size_type>(idx) < size_);
#endif
        return data_[static_cast<size_type>(idx)];
    }

    template <typename T1, typename T2, typename Allocator1, typename Allocator2, size_t S1, size_t S2>
    BQ_ARRAY_INLINE bool operator==(const BQ_ARRAY_CLS_NAME<T1, Allocator1, S1>& array1, const BQ_ARRAY_CLS_NAME<T2, Allocator2, S2>& array2)
    {
        if (array1.size() != array2.size()) {
            return false;
        }
        for (typename BQ_ARRAY_CLS_NAME<T1, Allocator1, S1>::size_type i = 0; i < array1.size(); ++i) {
            if (array1[i] != array2[i]) {
                return false;
            }
        }
        return true;
    }

    template <typename T1, typename T2, typename Allocator1, typename Allocator2, size_t S1, size_t S2>
    BQ_ARRAY_INLINE bool operator!=(const BQ_ARRAY_CLS_NAME<T1, Allocator1, S1>& array1, const BQ_ARRAY_CLS_NAME<T2, Allocator2, S2>& array2)
    {
        return !(array1 == array2);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>& BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::operator=(const BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>& rhs)
    {
        clear();
        insert_batch(begin(), rhs.begin(), rhs.size_);
        return *this;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>& BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::operator=(BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>&& rhs) noexcept
    {
        reset();
        data_ = rhs.data_;
        size_ = rhs.size_;
        capacity_ = rhs.capacity_;
        rhs.data_ = nullptr;
        rhs.size_ = 0;
        rhs.capacity_ = 0;
        return *this;
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_move_constructible<T>::value, void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_mem_copy)(Allocator&, T* dest, T* src, size_t count)
    {
        constexpr size_t item_size = sizeof(T);
        // simply copy memory for non-trivial constructible type to optimize performance.
        if (dest && src && count) {
            BQ_SUPPRESS_NULL_DEREF_BEGIN();
            memcpy((void*)dest, (void*)src, item_size * count);
            BQ_SUPPRESS_NULL_DEREF_END();
        }
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<!(bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_move_constructible<T>::value), void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_mem_copy)(Allocator& allocator, T* dest, T* src, size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            allocator.construct(dest + i, bq::move(src[i]));
            allocator.destroy(src + i);
        }
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_move_constructible<T>::value, void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_forward_move)(Allocator&, T* dest, T* src, size_t move_count)
    {
        // simply copy memory for non-trivial constructible type to optimize performance.
        memmove(dest, src, move_count * sizeof(T));
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<!(bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_move_constructible<T>::value), void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_forward_move)(Allocator& allocator, T* dest, T* src, size_t move_count)
    {
        size_t diff = static_cast<size_t>(dest - src);
        for (size_t i = move_count; i > 0; --i) {
            size_t index = i - 1;
            if (index + diff < move_count) {
                *(dest + index) = bq::move(*(src + index));
            } else {
                auto ptr = (dest + index);
                allocator.construct(ptr, bq::move(*(src + index)));
            }
        }
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_copy_assignable<T>::value, void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_backward_copy)(Allocator&, T* dest, const T* src, size_t count, size_t constructed_count)
    {
        (void)constructed_count;
        // simply copy memory for non-trivial constructible type to optimize performance.
        if (dest && src && count) {
            BQ_SUPPRESS_NULL_DEREF_BEGIN();
            memcpy(dest, src, count * sizeof(T));
            BQ_SUPPRESS_NULL_DEREF_END();
        }
    }

    template <typename T, typename Allocator>
    BQ_ARRAY_INLINE typename bq::enable_if<!(bq::is_trivially_copy_constructible<T>::value && bq::is_trivially_copy_assignable<T>::value), void>::type
    BQ_ARRAY_INLINE_MACRO(_inner_backward_copy)(Allocator& allocator, T* dest, const T* src, size_t count, size_t constructed_count)
    {
        for (size_t i = 0; i < count; ++i) {
            if (i < constructed_count) {
                *(dest + i) = *(src + i);
            } else {
                allocator.construct(dest + i, bq::forward<const T>(*(src + i)));
            }
        }
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename... V>
    void bq::BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::insert(iterator dest_it, V&&... args)
    {
        assert(dest_it >= begin() && dest_it <= end() && "dest_it param where_it must between begin() and end() iterator!");
        constexpr size_type count = 1;
        auto move_count = end() - dest_it;
        size_type new_size = size_ + count;
        auto diff = dest_it - begin();
        set_capacity(new_size);
        dest_it = begin() + diff;
        memmove(static_cast<void*>(end() + static_cast<difference_type>(count)), static_cast<void*>(end()), TAIL_BUFFER_SIZE * sizeof(value_type));
        BQ_ARRAY_INLINE_MACRO(_inner_forward_move)<value_type, allocator_type>(allocator_, dest_it + static_cast<difference_type>(count), dest_it, static_cast<size_type>(move_count));
        if (move_count > 0) {
            allocator_.destroy(static_cast<value_type*>(dest_it));
        }
        allocator_.construct(static_cast<value_type*>(dest_it), bq::forward<V>(args)...);
        size_ = new_size;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::insert_batch(iterator dest_it, const_iterator src_it, size_type count)
    {
        if (count == 0) {
            return;
        }
        assert(dest_it >= begin() && dest_it <= end() && "dest_it param where_it must between begin() and end() iterator!");
        auto move_count = (end() - dest_it);
        const size_type new_size = size_ + count;
        auto diff = dest_it - begin();
        set_capacity(new_size);
        dest_it = begin() + diff;
        if (static_cast<void*>(end())) {
            memmove(static_cast<void*>(end() + static_cast<difference_type>(count)), static_cast<void*>(end()), TAIL_BUFFER_SIZE * sizeof(value_type));
        }
        BQ_ARRAY_INLINE_MACRO(_inner_forward_move)<value_type, allocator_type>(allocator_, dest_it + static_cast<difference_type>(count), dest_it, static_cast<size_type>(move_count));
        BQ_ARRAY_INLINE_MACRO(_inner_backward_copy)<value_type, allocator_type>(allocator_, dest_it, src_it, count, static_cast<size_type>(move_count));
        size_ = new_size;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename... V>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::push_back(V&&... args)
    {
        insert(end(), bq::forward<V>(args)...);
        return size_;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename... V>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::emplace(iterator where_it, V&&... args)
    {
        return insert(where_it, bq::forward<V>(args)...);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename... V>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::emplace_back(V&&... args)
    {
        return push_back(bq::forward<V>(args)...);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename U>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_pod<U>::value>::type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::fill_uninitialized(typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::size_type count)
    {
        if (size() + count + TAIL_BUFFER_SIZE > capacity()) {
            set_capacity(size() + count);
        }
        size_ += count;
        if (data_ && TAIL_BUFFER_SIZE > 0) {
            memset(data_ + size_, 0, TAIL_BUFFER_SIZE * sizeof(value_type));
        }
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::pop_back()
    {
        if (size_ <= 0) {
            return;
        }
        allocator_.destroy((value_type*)end() - 1);
        --size_;
    }
    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::pop_back(size_type count)
    {
        assert(size_ >= count && "BQ_ARRAY_CLS_NAME size less than pop count");
        size_ -= count;
        allocator_.destroy(data_ + size_, count);
    }

    template <typename T>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_trivially_move_assignable<T>::value && bq::is_trivially_copy_assignable<T>::value, void>::type BQ_ARRAY_INLINE_MACRO(_inner_backward_move)(T* _dest, T* _src, size_t count)
    {
        if (_dest != _src && count > 0) {
            memmove(_dest, _src, sizeof(T) * count);
        }
    }

    template <typename T>
    BQ_ARRAY_INLINE typename bq::enable_if<!(bq::is_trivially_move_assignable<T>::value && bq::is_trivially_copy_assignable<T>::value), void>::type BQ_ARRAY_INLINE_MACRO(_inner_backward_move)(T* _dest, T* _src, size_t count)
    {
        if (_dest != _src) {
            for (size_t i = 0; i < count; ++i) {
                _dest[i] = bq::move(_src[i]);
            }
        }
    }

    // We adopt the mode of move assignment first, which is different from the C++11 standard documentation,
    // but is same with the MSVC STL behavior. The element that may be destructed is not necessarily the one being deleted,
    // but is always the element at the end of the BQ_ARRAY_CLS_NAME.
    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::erase(iterator where_it, size_type count /* = 1 */)
    {
        if (count == 0) {
            return;
        }
        assert(where_it >= begin() && where_it < end() && "erase param where_it must between begin() and end() iterator!");
        size_type left_count_from_where_to_tail = (size_type)(end() - where_it);
        size_type real_remove_count = bq::min_ref(left_count_from_where_to_tail, count);
        if (real_remove_count == 0) {
            return;
        }
        BQ_ARRAY_INLINE_MACRO(_inner_backward_move)<typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type>(where_it, where_it + static_cast<difference_type>(real_remove_count), static_cast<size_type>(end() - (where_it + static_cast<difference_type>(real_remove_count))));
        allocator_.destroy(data_ + size_ - static_cast<difference_type>(real_remove_count), real_remove_count);
        memmove(static_cast<void*>(end().operator->() - static_cast<difference_type>(real_remove_count)), static_cast<void*>(end().operator->()), TAIL_BUFFER_SIZE * sizeof(value_type));
        size_ -= real_remove_count;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::erase_replace(iterator where_it)
    {
        assert(where_it >= begin() && where_it < end() && "erase_replace param where_it must between begin() and end() iterator!");
        constexpr size_type real_remove_count = 1;
        BQ_ARRAY_INLINE_MACRO(_inner_backward_move)<typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type>(where_it, end() - 1, real_remove_count);
        allocator_.destroy(static_cast<value_type*>(end()) - 1);
        memmove(static_cast<void*>(end().operator->() - static_cast<difference_type>(real_remove_count)), static_cast<void*>(end().operator->()), TAIL_BUFFER_SIZE * sizeof(value_type));
        --size_;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::begin()
    {
        return iterator(data_, this);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::const_iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::begin() const
    {
        return const_iterator(data_, this);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::end()
    {
        return iterator(data_ + size_, this);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::const_iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::end() const
    {
        return const_iterator(data_ + size_, this);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE bool BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::set_capacity(size_type new_capacity, bool force_reset /* = false*/)
    {
        if (new_capacity < size_) {
            return false;
        }
        if (new_capacity > 0) {
            new_capacity += TAIL_BUFFER_SIZE;
        }
        if (!force_reset) {
            if (new_capacity <= capacity_) {
                return false;
            }
            capacity_ = bq::max_value(bq::roundup_pow_of_two(new_capacity), (size_type)2);
        } else {
            capacity_ = new_capacity;
        }
        value_type* new_data = nullptr;
        if (capacity_ > 0) {
            size_type item_size = sizeof(value_type);
            new_data = allocator_.allocate(capacity_);
            if (!new_data) {
                assert(false && "malloc memory failed, size:");
                return false;
            }
            if (data_) {
                BQ_ARRAY_INLINE_MACRO(_inner_mem_copy)<value_type, allocator_type>(allocator_, new_data, data_, size_);
                BQ_SUPPRESS_NULL_DEREF_BEGIN();
                memcpy((void*)(new_data + size_), (void*)(data_ + size_), item_size * TAIL_BUFFER_SIZE);
                BQ_SUPPRESS_NULL_DEREF_END();
            } else {
                memset((void*)new_data, 0, TAIL_BUFFER_SIZE * sizeof(value_type));
            }
        }
        if (data_) {
            allocator_.deallocate(data_, capacity_);
        }
        data_ = new_data;
        return true;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::find(const typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type& value, bool reverse_find /* = false*/)
    {
        if (reverse_find) {
            for (auto i = static_cast<difference_type>(size()); i >= 1; --i) {
                if (data_[i - 1] == value) {
                    return begin() + i - 1;
                }
            }
        } else {
            for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
                if (data_[i] == value) {
                    return begin() + i;
                }
            }
        }
        return end();
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::const_iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::find(const typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::value_type& value, bool reverse_find /* = false*/) const
    {
        if (reverse_find) {
            for (auto i = static_cast<difference_type>(size()); i >= 1; --i) {
                if (data_[i - 1] == value) {
                    return begin() + i - 1;
                }
            }
        } else {
            for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
                if (data_[i] == value) {
                    return begin() + i;
                }
            }
        }
        return end();
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename Predicate>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::find_if(Predicate predicate, bool reverse_find /* = false*/)
    {
        if (reverse_find) {
            for (auto i = static_cast<difference_type>(size()); i >= 1; --i) {
                if (predicate(data_[i - 1])) {
                    return begin() + i - 1;
                }
            }
        } else {
            for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
                if (predicate(data_[i])) {
                    return begin() + i;
                }
            }
        }
        return end();
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename Predicate>
    BQ_ARRAY_INLINE typename BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::const_iterator BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::find_if(Predicate predicate, bool reverse_find /* = false*/) const
    {
        if (reverse_find) {
            for (auto i = static_cast<difference_type>(size()); i >= 1; --i) {
                if (predicate(data_[i - 1])) {
                    return begin() + i - 1;
                }
            }
        } else {
            for (difference_type i = 0; i < static_cast<difference_type>(size()); ++i) {
                if (predicate(data_[i])) {
                    return begin() + i;
                }
            }
        }
        return end();
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::shrink()
    {
        set_capacity(size_, true);
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    BQ_ARRAY_INLINE void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::clear()
    {
        if (size_ > 0) {
            allocator_.destroy(data_, size_);
        }
        size_ = 0;
        if (data_) {
            memset((void*)data_, 0, TAIL_BUFFER_SIZE * sizeof(value_type));
        }
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename U>
    BQ_ARRAY_INLINE typename bq::enable_if<bq::is_pod<U>::value>::type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::reset()
    {
        if (capacity_ > 0) {
            allocator_.deallocate(data_, capacity_);
            data_ = nullptr;
        }
        size_ = 0;
        capacity_ = 0;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename U>
    BQ_ARRAY_INLINE typename bq::enable_if<!bq::is_pod<U>::value>::type BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::reset()
    {
        if (capacity_ > 0) {
            allocator_.destroy(data_, size_);
            allocator_.deallocate(data_, capacity_);
            data_ = nullptr;
        }
        size_ = 0;
        capacity_ = 0;
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename First>
    void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::inner_args_insert(First&& first)
    {
        insert(end(), bq::forward<First>(first));
    }

    template <typename T, typename Allocator, size_t TAIL_BUFFER_SIZE>
    template <typename First, typename... Rest>
    void BQ_ARRAY_CLS_NAME<T, Allocator, TAIL_BUFFER_SIZE>::inner_args_insert(First&& first, Rest&&... rest)
    {
        insert(end(), bq::forward<First>(first));
        inner_args_insert(bq::forward<Rest>(rest)...);
    }
}
