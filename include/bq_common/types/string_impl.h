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
 * \file string_impl.h
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include <string.h>
#include <ctype.h>
#include "bq_common/misc/assert.h"
#include "bq_common/types/type_tools.h"
#include "bq_common/types/string_def.h"
#include "bq_common/types/string_tools.h"
namespace bq {
    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME()
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type init_capacity)
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>::set_capacity(init_capacity);
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs)
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>(rhs)
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& rhs) noexcept
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>(bq::forward<BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>(rhs))
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str)
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        size_t str_length = str ? string_tools::string_len(str) : 0;
        if (str_length == 0) {
            return;
        }
        insert_batch(begin(), const_iterator(str, nullptr), str_length);
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str, size_t char_count)
        : array<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        if (char_count == 0) {
            return;
        }
        insert_batch(begin(), const_iterator(str, nullptr), char_count);
    }

    template <typename CHAR_TYPE, typename Allocator>
    template <typename S, typename>
    inline bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::BQ_STRING_CLS_NAME(const S& rhs)
        : BQ_STRING_CLS_NAME(rhs.size() > 0 ? string_tools::__bq_string_compatible_class_get_data(rhs) : nullptr, rhs.size())
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator=(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs)
    {
        clear();
        if (rhs.size() != 0) {
            insert_batch(begin(), rhs.begin(), rhs.size());
        }
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator=(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& rhs) noexcept
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

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator=(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str)
    {
        clear();
        size_t str_length = str ? string_tools::string_len(str) : 0;
        if (str_length != 0) {
            insert_batch(begin(), const_iterator(str, nullptr), str_length);
        }
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    template <typename S, typename>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator=(const S& rhs)
    {
        this->operator=(string_tools::__bq_string_compatible_class_get_data(rhs));
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator+=(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs)
    {
        size_t origin_size = size();
        size_t total_length = origin_size + rhs.size();
        if (total_length == 0) {
            return *this;
        }
        insert_batch(end(), rhs.begin(), rhs.size());
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator+(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs) const
    {
        BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> result;
        size_t origin_size = size();
        size_t total_length = origin_size + rhs.size();
        if (total_length == 0) {
            return result;
        }
        result.set_capacity(total_length);
        result.insert_batch(result.end(), begin(), size());
        result.insert_batch(result.end(), rhs.begin(), rhs.size());
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator+(const char* rhs) const
    {
        BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> result;
        size_t origin_size = size();
        size_t rhs_size = strlen(rhs);
        size_t total_length = origin_size + rhs_size;
        if (total_length == 0) {
            return result;
        }
        result.set_capacity(total_length);
        result.insert_batch(result.end(), begin(), size());
        result.insert_batch(result.end(), rhs, rhs_size);
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> operator+(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& lhs, const char* rhs) noexcept
    {
        lhs.insert_batch(lhs.end(), rhs, strlen(rhs));
        return move(lhs);
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> operator+(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& lhs, const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs) noexcept
    {
        lhs.insert_batch(lhs.end(), rhs.begin(), rhs.size());
        return move(lhs);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator==(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str) const
    {
        size_t compare_size = str ? string_tools::string_len(str) : 0;
        if (compare_size != size()) {
            return false;
        }
        if (compare_size == 0) {
            return true;
        }
        int32_t compare = memcmp(c_str(), str, size() * sizeof(CHAR_TYPE));
        if (compare != 0) {
            return false;
        }
        return str[size()] == '\0';
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator==(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() != str.size()) {
            return false;
        }
        if (size() == 0) {
            return true;
        }
        return (memcmp(c_str(), str.c_str(), size() * sizeof(CHAR_TYPE)) == 0);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator!=(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str) const
    {
        return !operator==(str);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator!=(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        return !operator==(str);
    }

    template <typename CHAR_TYPE, typename Allocator>
    template <typename S, typename>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::operator S() const
    {
        return c_str();
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename bq::generic_type_hash_calculator<typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::hash_code() const
    {
        return static_cast<typename bq::generic_type_hash_calculator<bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type>(get_hash(*this));
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename bq::generic_type_hash_calculator<typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::hash_code_ignore_case() const
    {
        return static_cast<typename bq::generic_type_hash_calculator<bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type>(get_hash_ignore_case(*this));
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::c_str() const

    {
        if (size() == 0) {
            return empty_str();
        }
        return begin().operator->();
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::is_empty() const
    {
        return size() == 0;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::substr(typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type pos, typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type count /* = npos*/) const
    {
        assert(pos < size() && "BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> substr pos error, exceed the BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> length!");
        BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> result;
        if (count == npos || pos + count > size()) {
            count = size() - pos;
        }
        if (count == 0) {
            return result;
        }
        result.set_capacity(count, true);
        result.insert_batch(result.begin(), begin() + static_cast<difference_type>(pos), count);
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::find(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() == 0) {
            return BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::npos;
        }
        const char_type* found_ptr = string_tools::find_str(c_str(), str.c_str());
        if (!found_ptr) {
            return BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::npos;
        }
        return (size_type)(found_ptr - begin().operator->());
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::find(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str, typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type search_start_pos) const
    {
        if (size() <= search_start_pos) {
            return BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::npos;
        }
        const char_type* found_ptr = string_tools::find_str(c_str() + search_start_pos, str.c_str());
        if (!found_ptr) {
            return BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::npos;
        }
        return (size_type)(found_ptr - begin().operator->());
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::begin_with(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() < str.size()) {
            return false;
        }
        if ((nullptr != c_str()) && (nullptr != str.c_str())) {
            return memcmp(c_str(), str.c_str(), str.size() * sizeof(CHAR_TYPE)) == 0;
        }
        return false;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::end_with(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() < str.size()) {
            return false;
        }
        auto offset = static_cast<ptrdiff_t>(size() - str.size());
        const char_type* s1 = c_str() + offset;
        const char_type* s2 = str.c_str();
        if (s1 && s2) {
            return memcmp(s1, s2, str.size() * sizeof(CHAR_TYPE)) == 0;
        }
        return false;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::find_last(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() == 0 || str.size() == 0 || size() < str.size()) {
            return BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::npos;
        }
        const size_type str_len = str.size();
        size_type pos = size() - str_len;
        while (pos > 0) {
            if (memcmp(c_str() + pos, str.c_str(), str_len * sizeof(CHAR_TYPE)) == 0) {
                return pos;
            }
            --pos;
        }
        return npos;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::replace(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& from, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& to) const
    {
        assert(!from.is_empty() && "replace from BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> must no be empty!");
        bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> result;

        result.set_capacity(capacity()); // it accelerate for most of time
        size_type pos = 0;
        while (pos < size()) {
            size_t found_pos = find(from, pos);
            if (found_pos != npos) {
                result.insert_batch(result.end(), begin() + static_cast<difference_type>(pos), found_pos - pos);
                result.insert_batch(result.end(), to.begin(), to.size());
                pos = found_pos + from.size();
            } else {
                result.insert_batch(result.end(), begin() + static_cast<difference_type>(pos), size() - pos);
                pos = size();
            }
        }
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::equals_ignore_case(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const
    {
        if (size() != str.size()) {
            return false;
        }
        size_type this_size = this->size();
        for (size_type i = 0; i < this_size; ++i) {
            if (operator[](i) != str[i] && toupper(operator[](i)) != toupper(str[i])) {
                return false;
            }
        }
        return true;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bq::array<bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::split(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& delimiter, bool include_empty_string) const
    {
        bq::array<bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>> result;
        size_type last_offset = 0;
        while (last_offset <= size()) {
            size_type new_offset = find(delimiter, last_offset);
            if (new_offset == npos) {
                // No more delimiters found, add the remaining string (could be empty if at end)
                bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> last_str = (last_offset < size()) ? substr(last_offset) : BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>();
                if (include_empty_string || !last_str.is_empty())
                    result.emplace_back(last_str);
                break;
            }
            else {
                // Found a delimiter, add the substring before it
                bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> last_str = substr(last_offset, new_offset - last_offset);
                if (include_empty_string || !last_str.is_empty())
                    result.emplace_back(last_str);
                last_offset = new_offset + delimiter.size();
            }
        }
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::trim() const
    {
        size_type start_pos = 0;
        while (start_pos < size()) {
            if (!isspace((int32_t)(uint8_t)(*(c_str() + start_pos)))) {
                break;
            }
            ++start_pos;
        }
        if (start_pos >= size()) {
            return empty_str();
        }
        size_type end_pos = size();
        while (end_pos > 0) {
            --end_pos;
            if (!isspace((int32_t)(uint8_t)(*(c_str() + end_pos)))) {
                break;
            }
        }
        return substr(start_pos, end_pos - start_pos + 1);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline uint64_t BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::get_hash(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str)
    {
        //Slower than bq::util::get_hash_64 and produces a different hash value, but suitable for header-only files.
        uint64_t hash = 0;
        for (size_type i = 0; i < str.size(); i++) {
            hash *= 1099511628211ull;
            hash ^= static_cast<uint64_t>(str[i]);
        }
        return hash;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE uint64_t BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::get_hash_ignore_case(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str)
    {
        uint64_t hash = 0;
        for (size_type i = 0; i < str.size(); i++) {
            hash *= 1099511628211ull;
            hash ^= static_cast<uint64_t>(toupper((int32_t)str[i]));
        }
        return hash;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::empty_str() const
    {
        return string_tools::inner_empty_str_def<CHAR_TYPE>::value();
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> operator+(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2)
    {
        bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> result;
        result.set_capacity(string_tools::string_len(str1) + str2.size());
        result += str1;
        result += str2;
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE bool operator==(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2)
    {
        return str2 == str1;
    }

    template <typename CHAR_TYPE, typename Allocator>
    BQ_STRING_INLINE bool operator!=(const typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2)
    {
        return str2 != str1;
    }
}
