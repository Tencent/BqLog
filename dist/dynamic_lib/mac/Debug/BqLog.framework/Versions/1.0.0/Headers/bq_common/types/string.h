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
 * \file array.h
 * substitute of STL string_base, we exclude STL and libc++ to reduce the final executable and library file size
 * IMPORTANT: It is not thread-safe!!!
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include "bq_common/types/array.h"
#include "bq_common/types/type_tools.h"

namespace bq {
    template <typename CHAR_TYPE>
    class string_base : public array<CHAR_TYPE, 1> {
    public:
        typedef array<CHAR_TYPE, 1> container_type;
        using container_type::begin;
        using container_type::capacity;
        using container_type::clear;
        using container_type::end;
        using container_type::insert;
        using container_type::insert_batch;
        using container_type::reset;
        using container_type::size;
        using container_type::operator[];
        typedef typename container_type::size_type size_type;
        typedef typename container_type::iterator iterator;
        typedef typename container_type::const_iterator const_iterator;
        using container_type::capacity_;
        using container_type::data_;
        using container_type::size_;

        static constexpr typename array<CHAR_TYPE, 1>::size_type npos = static_cast<typename string_base<CHAR_TYPE>::size_type>(-1);
        typedef typename container_type::value_type char_type;

    private:
        template <typename T>
        struct is_compatible_helper {
        private:
            // for basic_string
            template <typename U, typename V = void>
            struct c_str_func_sfinae : public bq::false_type {
            };
            template <typename U>
            struct c_str_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq::declval<const U>().c_str()), const typename U::value_type*>::value>> : public bq::true_type {
            };

            // for string_view
            template <typename U, typename V = void>
            struct data_func_sfinae : public bq::false_type {
            };
            template <typename U>
            struct data_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq::declval<const U>().data()), const typename U::value_type*>::value>> : public bq::true_type {
            };

            template <typename U, typename V = void>
            struct size_func_sfinae : public bq::false_type {
            };
            template <typename U>
            struct size_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq::declval<const U>().size()), typename U::size_type>::value>> : public bq::true_type {
            };

            template <typename U, typename V = void>
            struct size_sfinae {

                static constexpr size_t value = 0;
            };
            template <typename U>
            struct size_sfinae<U, bq::enable_if_t<sizeof(typename U::value_type) == 1 || sizeof(typename U::value_type) == 2 || sizeof(typename U::value_type) == 4>> {
                static constexpr size_t value = sizeof(typename U::value_type);
            };

        public:
            static constexpr bool is_std_string = c_str_func_sfinae<bq::decay_t<T>>::value && size_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool is_string_view = !is_std_string && data_func_sfinae<bq::decay_t<T>>::value && size_func_sfinae<bq::decay_t<T>>::value;

            static constexpr size_t char_size = size_sfinae<bq::decay_t<T>>::value;
            static constexpr bool is_utf8 = (is_std_string || is_string_view) && (char_size == 1);
            static constexpr bool is_utf16 = (is_std_string || is_string_view) && (char_size == 2);
            static constexpr bool is_utf32 = (is_std_string || is_string_view) && (char_size == 4);
        };

    public:
        // To determine whether a type is compatible with a bq string type.
        // For example:
        // bq::string is compatible with std::string,
        // bq::u16string is compatible with std::u16string,
        // and bq::u32string is compatible with std::u32string.
        template <typename T>
        struct is_std_string_compatible : public bq::bool_type<is_compatible_helper<bq::decay_t<T>>::is_std_string
                                              && is_compatible_helper<bq::decay_t<T>>::char_size == sizeof(char_type)> {
        };
        template <typename T>
        struct is_std_string_view_compatible : public bq::bool_type<is_compatible_helper<bq::decay_t<T>>::is_string_view
                                                   && is_compatible_helper<bq::decay_t<T>>::char_size == sizeof(char_type)> {
        };

        struct std_string_dummy_tag_type { };
        struct std_string_view_dummy_tag_type { };

    public:
        string_base();

        explicit string_base(typename string_base<CHAR_TYPE>::size_type capacity);

        string_base(const string_base<CHAR_TYPE>& rhs);

        string_base(string_base<CHAR_TYPE>&& rhs) noexcept;

        string_base(const char_type* str);
        
        string_base(const char_type* str, size_t char_count);

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, string_base<CHAR_TYPE>>::value, void>>
        string_base(const S& rhs);

        string_base& operator=(const string_base<CHAR_TYPE>& rhs);

        string_base& operator=(string_base<CHAR_TYPE>&& rhs) noexcept;

        string_base& operator=(const char_type* str);

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, string_base<CHAR_TYPE>>::value>>
        string_base& operator=(const S& rhs);

        string_base operator+(const string_base<CHAR_TYPE>& rhs) const;

        string_base operator+(const char* rhs) const;

        string_base& operator+=(const string_base<CHAR_TYPE>& rhs);

        bool operator==(const char_type* str) const;

        bool operator==(const string_base& str) const;

        bool operator!=(const char_type* str) const;

        bool operator!=(const string_base& str) const;

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, string_base<CHAR_TYPE>>::value>>
        operator S() const;

        typename bq::generic_type_hash_calculator<typename bq::string_base<CHAR_TYPE>>::hash_value_type hash_code() const;

        typename bq::generic_type_hash_calculator<typename bq::string_base<CHAR_TYPE>>::hash_value_type hash_code_ignore_case() const;

        const char_type* c_str() const;

        bool is_empty() const;

        string_base substr(typename string_base<CHAR_TYPE>::size_type pos, typename string_base<CHAR_TYPE>::size_type count = npos) const;

        typename string_base<CHAR_TYPE>::size_type find(const bq::string_base<CHAR_TYPE>& str) const;

        typename string_base<CHAR_TYPE>::size_type find(const bq::string_base<CHAR_TYPE>& str, typename string_base<CHAR_TYPE>::size_type search_start_pos) const;

        bool begin_with(const bq::string_base<CHAR_TYPE>& str) const;

        bool end_with(const bq::string_base<CHAR_TYPE>& str) const;

        typename string_base<CHAR_TYPE>::size_type find_last(const bq::string_base<CHAR_TYPE>& str) const;

        string_base replace(const bq::string_base<CHAR_TYPE>& from, const bq::string_base<CHAR_TYPE>& to) const;

        bool equals_ignore_case(const bq::string_base<CHAR_TYPE>& str) const;

        bq::array<typename bq::string_base<CHAR_TYPE>> split(const bq::string_base<CHAR_TYPE>& delimiter) const;

        string_base trim() const;

    private:
        static uint64_t get_hash(const bq::string_base<CHAR_TYPE>& str);

        static uint64_t get_hash_ignore_case(const bq::string_base<CHAR_TYPE>& str);

        const char_type* empty_str() const;
    };

    template <typename CHAR_TYPE>
    bq::string_base<CHAR_TYPE> operator+(const typename bq::string_base<CHAR_TYPE>::char_type* str1, const bq::string_base<CHAR_TYPE>& str2);

    template <typename CHAR_TYPE>
    bool operator==(const typename bq::string_base<CHAR_TYPE>::char_type* str1, const bq::string_base<CHAR_TYPE>& str2);

    template <typename CHAR_TYPE>
    bool operator!=(const typename bq::string_base<CHAR_TYPE>::char_type* str1, const bq::string_base<CHAR_TYPE>& str2);

    typedef string_base<char> string;
    typedef string_base<char16_t> u16string;
    typedef string_base<char32_t> u32string;
}

#include "bq_common/types/string_impl.h"
