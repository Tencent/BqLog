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
 * \file array.h
 * substitute of STL string_base<CHAR_TYPE, Allocator>, we exclude STL and libc++ to reduce the final executable and library file size
 * IMPORTANT: It is not thread-safe!!!
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include <string.h>
#include <ctype.h>
#include <wchar.h>
namespace bq {
    namespace string_tools {
        /**
         * @brief Fallback loop for compile-time constant evaluation.
         * Compilers like GCC and Clang can unroll and constant-fold this
         * when the input is a string literal.
         */
        template <typename T>
        static BQ_FUNC_RETURN_CONSTEXPR size_t constexpr_len(const T* str) {
            size_t i = 0;
            while (str[i] != 0) {
                i++;
            }
            return i;
        }

        /**
         * @brief SWAR (SIMD Within A Register) implementation for UTF-16.
         * Processes 4 characters (8 bytes) per iteration using bit manipulation
         * to detect the null terminator.
         */
        static bq_forceinline size_t u16_len_rt(const char16_t* s) {
            const char16_t* p = s;
            // Align to 8-byte boundary
            while ((reinterpret_cast<uintptr_t>(p) & 7) != 0) {
                if (*p == 0) return static_cast<size_t>(p - s);
                p++;
            }

            const uint64_t* chunk = reinterpret_cast<const uint64_t*>(p);
            const uint64_t mask = 0x8000800080008000ULL;
            const uint64_t low_bit = 0x0001000100010001ULL;

            while (true) {
                uint64_t v = *chunk;
                // Zero detection logic: (v - 0x0001) & ~v & 0x8000
                if ((v - low_bit) & ~v & mask) {
                    p = reinterpret_cast<const char16_t*>(chunk);
                    if (p[0] == 0) return static_cast<size_t>(p - s);
                    if (p[1] == 0) return static_cast<size_t>(p + 1 - s);
                    if (p[2] == 0) return static_cast<size_t>(p + 2 - s);
                    if (p[3] == 0) return static_cast<size_t>(p + 3 - s);
                }
                chunk++;
            }
        }

        /**
         * @brief SWAR implementation for UTF-32.
         * Processes 2 characters (8 bytes) per iteration.
         */
        static bq_forceinline size_t u32_len_rt(const char32_t* s) {
            const char32_t* p = s;
            while ((reinterpret_cast<uintptr_t>(p) & 7) != 0) {
                if (*p == 0) return static_cast<size_t>(p - s);
                p++;
            }

            const uint64_t* chunk = reinterpret_cast<const uint64_t*>(p);
            const uint64_t mask = 0x8000000080000000ULL;
            const uint64_t low_bit = 0x0000000100000001ULL;

            while (true) {
                uint64_t v = *chunk;
                if ((v - low_bit) & ~v & mask) {
                    p = reinterpret_cast<const char32_t*>(chunk);
                    if (p[0] == 0) return static_cast<size_t>(p - s);
                    if (p[1] == 0) return static_cast<size_t>(p + 1 - s);
                }
                chunk++;
            }
        }

        /* --- Dispatcher Templates --- */

        template <typename T, bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch {
            static BQ_FUNC_RETURN_CONSTEXPR size_t exec(const T* s) {
                return constexpr_len(s);
            }
        };

        template <bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch<char, WCHAR_SIZE_IS_16> {
            static bq_forceinline size_t exec(const char* s) {
#if BQ_GCC_CLANG_BUILTIN(__builtin_wcslen)
                return __builtin_strlen(s);
#else
                return strlen(s);
#endif
            }
        };

        template <bool WCHAR_SIZE_IS_16>
        struct string_len_dispatch<wchar_t, WCHAR_SIZE_IS_16> {
            static bq_forceinline size_t exec(const wchar_t* s) {
#if BQ_GCC_CLANG_BUILTIN(__builtin_wcslen)
                return __builtin_wcslen(s);
#else
                return wcslen(s);
#endif
            }
        };

        template <>
        struct string_len_dispatch<char16_t, true> {
            static bq_forceinline size_t exec(const char16_t* s) {
                return string_len_dispatch<wchar_t, true>::exec(reinterpret_cast<const wchar_t*>(s));
            }
        };
        template <>
        struct string_len_dispatch<char16_t, false> {
            static bq_forceinline size_t exec(const char16_t* s) {
                return u16_len_rt(s);
            }
        };

        template <>
        struct string_len_dispatch<char32_t, true> {
            static bq_forceinline size_t exec(const char32_t* s) {
                return u32_len_rt(s);
            }
        };
        template <>
        struct string_len_dispatch<char32_t, false> {
            static bq_forceinline size_t exec(const char32_t* s) {
                return string_len_dispatch<wchar_t, false>::exec(reinterpret_cast<const wchar_t*>(s));
            }
        };

        /**
         * @brief Unified entry point for string length calculation.
         * * Performance characteristics:
         * 1. Compile-time: Literal strings result in a zero-cost constant.
         * 2. Runtime (char/wchar_t): Dispatches to compiler built-ins (SIMD optimized).
         * 3. Runtime (u16/u32): Uses 64-bit SWAR to process multiple units per cycle.
         */
        template <typename CHAR_TYPE>
        BQ_FUNC_RETURN_CONSTEXPR size_t string_len_ptr(const CHAR_TYPE* str) {
            if (!str){
                return 0;
            }
#if BQ_GCC_CLANG_BUILTIN(__builtin_constant_p)
            BQ_CONSTEXPR_IF(__builtin_constant_p(*str))
            {
                return constexpr_len(str);
            }else{
#endif
                return string_len_dispatch<CHAR_TYPE, sizeof(char16_t) == sizeof(wchar_t)>::exec(str);
#if defined(BQ_GCC) || defined(BQ_CLANG)
            }
#endif
        }
        template <typename CHAR_TYPE, size_t N>
        BQ_FUNC_RETURN_CONSTEXPR size_t string_len_array(const CHAR_TYPE(&str)[N]) {
            BQ_LIKELY_IF(str[N - 1] == '\0') {
                return N - 1; //constexpr_len(str);
            }
            else {
                return N;
            }
        }
        template <typename STR_TYPE>
        bq_forceinline bq::enable_if_t<bq::is_pointer<STR_TYPE>::value, size_t> string_len(const STR_TYPE& str) {
            return string_len_ptr(str);
        }

        template <typename STR_TYPE>
        bq_forceinline bq::enable_if_t<bq::is_array<STR_TYPE>::value, size_t> string_len(const STR_TYPE& str) {
            return string_len_array(str);
        }


        template <typename CHAR_TYPE>
        inline const CHAR_TYPE* find_char(const CHAR_TYPE* str, CHAR_TYPE c)
        {
            if (!str) {
                return nullptr;
            }
            for (; *str; ++str) {
                if (*str == c) {
                    return str;
                }
            }
            return nullptr;
        }

        template <typename CHAR_TYPE>
        inline const CHAR_TYPE* find_str(const CHAR_TYPE* str1, const CHAR_TYPE* str2)
        {
            if (!str2) {
                return nullptr;
            }
            auto len2 = string_len(str2);
            if (0 == len2) {
            }
            auto len1 = string_len(str1);
            const CHAR_TYPE* begin_pos = find_char(str1, *str2);
            while (begin_pos && begin_pos + len2 <= str1 + len1) {
                if (memcmp(begin_pos, str2, len2 * sizeof(CHAR_TYPE)) == 0) {
                    return begin_pos;
                }
                begin_pos = find_char(begin_pos + 1, *str2);
            }
            return nullptr;
        }

        template <>
        inline const char* find_str(const char* str1, const char* str2)
        {
            if (str1 == nullptr || str2 == nullptr) {
                return nullptr;
            }
            return strstr(str1, str2);
        }
    }


    template <typename T, typename = void>
    struct __has_value_type : bq::false_type { };

    template <typename T>
    struct __has_value_type<T, bq::void_t<typename T::value_type>> : bq::true_type { };

    template <typename T, typename bq::enable_if_t<(bq::string::is_std_string_compatible<T>::value || bq::u16string::is_std_string_compatible<T>::value || bq::u32string::is_std_string_compatible<T>::value), int32_t> = 0>
    inline const typename T::value_type* __bq_string_compatible_class_get_data(const T& str)
    {
        return str.c_str();
    }

    template <typename T, typename bq::enable_if_t<(bq::string::is_std_string_view_compatible<T>::value || bq::u16string::is_std_string_view_compatible<T>::value || bq::u32string::is_std_string_view_compatible<T>::value), int64_t> = 0>
    inline const typename T::value_type* __bq_string_compatible_class_get_data(const T& str)
    {
        return str.data();
    }
}
namespace bq {
    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base()
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base(typename string_base<CHAR_TYPE, Allocator>::size_type init_capacity)
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>::set_capacity(init_capacity);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base(const string_base<CHAR_TYPE, Allocator>& rhs)
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>(rhs)
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base(string_base<CHAR_TYPE, Allocator>&& rhs) noexcept
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>(bq::forward<string_base<CHAR_TYPE, Allocator>>(rhs))
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base(const typename string_base<CHAR_TYPE, Allocator>::char_type* str)
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        size_t str_length = str ? string_tools::string_len(str) : 0;
        if (str_length == 0) {
            return;
        }
        insert_batch(begin(), const_iterator(str, nullptr), str_length);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>::string_base(const typename string_base<CHAR_TYPE, Allocator>::char_type* str, size_t char_count)
        : array<typename string_base<CHAR_TYPE, Allocator>::char_type, Allocator, 1>()
    {
        if (char_count == 0) {
            return;
        }
        insert_batch(begin(), const_iterator(str, nullptr), char_count);
    }

    template <typename CHAR_TYPE, typename Allocator>
    template <typename S, typename>
    inline bq::string_base<CHAR_TYPE, Allocator>::string_base(const S& rhs)
        : string_base(rhs.size() > 0 ? __bq_string_compatible_class_get_data(rhs) : nullptr, rhs.size())
    {
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>& string_base<CHAR_TYPE, Allocator>::operator=(const string_base<CHAR_TYPE, Allocator>& rhs)
    {
        clear();
        if (rhs.size() != 0) {
            insert_batch(begin(), rhs.begin(), rhs.size());
        }
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>& string_base<CHAR_TYPE, Allocator>::operator=(string_base<CHAR_TYPE, Allocator>&& rhs) noexcept
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
    inline string_base<CHAR_TYPE, Allocator>& string_base<CHAR_TYPE, Allocator>::operator=(const typename string_base<CHAR_TYPE, Allocator>::char_type* str)
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
    inline string_base<CHAR_TYPE, Allocator>& string_base<CHAR_TYPE, Allocator>::operator=(const S& rhs)
    {
        this->operator=(__bq_string_compatible_class_get_data(rhs));
        return *this;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator>& string_base<CHAR_TYPE, Allocator>::operator+=(const string_base<CHAR_TYPE, Allocator>& rhs)
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
    inline string_base<CHAR_TYPE, Allocator> string_base<CHAR_TYPE, Allocator>::operator+(const string_base<CHAR_TYPE, Allocator>& rhs) const
    {
        string_base<CHAR_TYPE, Allocator> result;
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
    inline string_base<CHAR_TYPE, Allocator> string_base<CHAR_TYPE, Allocator>::operator+(const char* rhs) const
    {
        string_base<CHAR_TYPE, Allocator> result;
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
    inline string_base<CHAR_TYPE, Allocator> operator+(string_base<CHAR_TYPE, Allocator>&& lhs, const char* rhs) noexcept
    {
        lhs.insert_batch(lhs.end(), rhs, strlen(rhs));
        return move(lhs);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator> operator+(string_base<CHAR_TYPE, Allocator>&& lhs, const string_base<CHAR_TYPE, Allocator>& rhs) noexcept
    {
        lhs.insert_batch(lhs.end(), rhs.begin(), rhs.size());
        return move(lhs);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool string_base<CHAR_TYPE, Allocator>::operator==(const typename string_base<CHAR_TYPE, Allocator>::char_type* str) const
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
    inline bool string_base<CHAR_TYPE, Allocator>::operator==(const string_base<CHAR_TYPE, Allocator>& str) const
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
    inline bool string_base<CHAR_TYPE, Allocator>::operator!=(const typename string_base<CHAR_TYPE, Allocator>::char_type* str) const
    {
        return !operator==(str);
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool string_base<CHAR_TYPE, Allocator>::operator!=(const string_base<CHAR_TYPE, Allocator>& str) const
    {
        return !operator==(str);
    }

    template <typename CHAR_TYPE, typename Allocator>
    template <typename S, typename>
    inline string_base<CHAR_TYPE, Allocator>::operator S() const
    {
        return c_str();
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename bq::generic_type_hash_calculator<typename bq::string_base<CHAR_TYPE, Allocator>>::hash_value_type string_base<CHAR_TYPE, Allocator>::hash_code() const
    {
        return static_cast<typename bq::generic_type_hash_calculator<bq::string_base<CHAR_TYPE, Allocator>>::hash_value_type>(get_hash(*this));
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename bq::generic_type_hash_calculator<typename bq::string_base<CHAR_TYPE, Allocator>>::hash_value_type string_base<CHAR_TYPE, Allocator>::hash_code_ignore_case() const
    {
        return static_cast<typename bq::generic_type_hash_calculator<bq::string_base<CHAR_TYPE, Allocator>>::hash_value_type>(get_hash_ignore_case(*this));
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline const typename string_base<CHAR_TYPE, Allocator>::char_type* string_base<CHAR_TYPE, Allocator>::c_str() const

    {
        if (size() == 0) {
            return empty_str();
        }
        return begin().operator->();
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool string_base<CHAR_TYPE, Allocator>::is_empty() const
    {
        return size() == 0;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator> string_base<CHAR_TYPE, Allocator>::substr(typename string_base<CHAR_TYPE, Allocator>::size_type pos, typename string_base<CHAR_TYPE, Allocator>::size_type count /* = npos*/) const
    {
        assert(pos < size() && "string_base<CHAR_TYPE, Allocator> substr pos error, exceed the string_base<CHAR_TYPE, Allocator> length!");
        string_base<CHAR_TYPE, Allocator> result;
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
    inline typename string_base<CHAR_TYPE, Allocator>::size_type string_base<CHAR_TYPE, Allocator>::find(const bq::string_base<CHAR_TYPE, Allocator>& str) const
    {
        if (size() == 0) {
            return string_base<CHAR_TYPE, Allocator>::npos;
        }
        const char_type* found_ptr = string_tools::find_str(c_str(), str.c_str());
        if (!found_ptr) {
            return string_base<CHAR_TYPE, Allocator>::npos;
        }
        return (size_type)(found_ptr - begin().operator->());
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline typename string_base<CHAR_TYPE, Allocator>::size_type string_base<CHAR_TYPE, Allocator>::find(const bq::string_base<CHAR_TYPE, Allocator>& str, typename string_base<CHAR_TYPE, Allocator>::size_type search_start_pos) const
    {
        if (size() <= search_start_pos) {
            return string_base<CHAR_TYPE, Allocator>::npos;
        }
        const char_type* found_ptr = string_tools::find_str(c_str() + search_start_pos, str.c_str());
        if (!found_ptr) {
            return string_base<CHAR_TYPE, Allocator>::npos;
        }
        return (size_type)(found_ptr - begin().operator->());
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool string_base<CHAR_TYPE, Allocator>::begin_with(const bq::string_base<CHAR_TYPE, Allocator>& str) const
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
    inline bool string_base<CHAR_TYPE, Allocator>::end_with(const bq::string_base<CHAR_TYPE, Allocator>& str) const
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
    inline typename string_base<CHAR_TYPE, Allocator>::size_type string_base<CHAR_TYPE, Allocator>::find_last(const bq::string_base<CHAR_TYPE, Allocator>& str) const
    {
        if (size() == 0 || str.size() == 0 || size() < str.size()) {
            return string_base<CHAR_TYPE, Allocator>::npos;
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
    inline string_base<CHAR_TYPE, Allocator> string_base<CHAR_TYPE, Allocator>::replace(const bq::string_base<CHAR_TYPE, Allocator>& from, const bq::string_base<CHAR_TYPE, Allocator>& to) const
    {
        assert(!from.is_empty() && "replace from string_base<CHAR_TYPE, Allocator> must no be empty!");
        bq::string_base<CHAR_TYPE, Allocator> result;

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
    inline bool string_base<CHAR_TYPE, Allocator>::equals_ignore_case(const bq::string_base<CHAR_TYPE, Allocator>& str) const
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
    inline bq::array<bq::string_base<CHAR_TYPE, Allocator>> string_base<CHAR_TYPE, Allocator>::split(const bq::string_base<CHAR_TYPE, Allocator>& delimiter) const
    {
        bq::array<bq::string_base<CHAR_TYPE, Allocator>> result;
        size_type last_offset = 0;
        while (last_offset < size()) {
            size_type new_offset = find(delimiter, last_offset);
            if (new_offset == npos) {
                bq::string_base<CHAR_TYPE, Allocator> lastStr = substr(last_offset);
                if (!lastStr.is_empty()) {
                    result.emplace_back(lastStr);
                }
                break;
            } else {
                bq::string_base<CHAR_TYPE, Allocator> lastStr = substr(last_offset, new_offset - last_offset);
                if (!lastStr.is_empty()) {
                    result.emplace_back(lastStr);
                }
                last_offset = new_offset + delimiter.size();
            }
        }
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline string_base<CHAR_TYPE, Allocator> string_base<CHAR_TYPE, Allocator>::trim() const
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
    inline uint64_t string_base<CHAR_TYPE, Allocator>::get_hash(const bq::string_base<CHAR_TYPE, Allocator>& str)
    {
        uint64_t hash = 0;
        for (size_type i = 0; i < str.size(); i++) {
            hash *= 1099511628211ull;
            hash ^= static_cast<uint64_t>(str[i]);
        }
        return hash;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline uint64_t string_base<CHAR_TYPE, Allocator>::get_hash_ignore_case(const bq::string_base<CHAR_TYPE, Allocator>& str)
    {
        uint64_t hash = 0;
        for (size_type i = 0; i < str.size(); i++) {
            hash *= 1099511628211ull;
            hash ^= static_cast<uint64_t>(toupper((int32_t)str[i]));
        }
        return hash;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline const typename string_base<CHAR_TYPE, Allocator>::char_type* string_base<CHAR_TYPE, Allocator>::empty_str() const
    {
        assert(false && "unimplemented CHAR_TYPE!");
        return nullptr;
    }

    template <>
    inline const typename string_base<char>::char_type* string_base<char>::empty_str() const
    {
        return "";
    }

    template <>
    inline const typename string_base<char16_t>::char_type* string_base<char16_t>::empty_str() const
    {
        return u"";
    }

    template <>
    inline const typename string_base<char32_t>::char_type* string_base<char32_t>::empty_str() const
    {
        return U"";
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bq::string_base<CHAR_TYPE, Allocator> operator+(const typename string_base<CHAR_TYPE, Allocator>::char_type* str1, const bq::string_base<CHAR_TYPE, Allocator>& str2)
    {
        bq::string_base<CHAR_TYPE, Allocator> result;
        result.set_capacity(string_tools::string_len(str1) + str2.size());
        result += str1;
        result += str2;
        return result;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool operator==(const typename string_base<CHAR_TYPE, Allocator>::char_type* str1, const bq::string_base<CHAR_TYPE, Allocator>& str2)
    {
        return str2 == str1;
    }

    template <typename CHAR_TYPE, typename Allocator>
    inline bool operator!=(const typename string_base<CHAR_TYPE, Allocator>::char_type* str1, const bq::string_base<CHAR_TYPE, Allocator>& str2)
    {
        return str2 != str1;
    }
}
