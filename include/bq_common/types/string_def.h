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
 * \file string_def.h
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */
#include "bq_common/types/array.h"
#include "bq_common/types/type_tools.h"

namespace bq {
    template <typename CHAR_TYPE, typename Allocator = bq::default_allocator<CHAR_TYPE>>
    class BQ_STRING_CLS_NAME : public array<CHAR_TYPE, Allocator, 1> {
    public:
        typedef array<CHAR_TYPE, Allocator, 1> container_type;
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
        typedef typename container_type::difference_type difference_type;
        typedef typename container_type::iterator iterator;
        typedef typename container_type::const_iterator const_iterator;
        typedef typename container_type::allocator_type allocator_type;
        using container_type::capacity_;
        using container_type::data_;
        using container_type::size_;

        static constexpr typename array<CHAR_TYPE, Allocator, 1>::size_type npos = static_cast<typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type>(-1);
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
        BQ_STRING_CLS_NAME();

        explicit BQ_STRING_CLS_NAME(typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type init_capacity);

        BQ_STRING_CLS_NAME(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs);

        BQ_STRING_CLS_NAME(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& rhs) noexcept;

        BQ_STRING_CLS_NAME(const char_type* str);

        BQ_STRING_CLS_NAME(const char_type* str, size_t char_count);

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::value, void>>
        BQ_STRING_CLS_NAME(const S& rhs);

        BQ_STRING_CLS_NAME& operator=(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs);

        BQ_STRING_CLS_NAME& operator=(BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>&& rhs) noexcept;

        BQ_STRING_CLS_NAME& operator=(const char_type* str);

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::value>>
        BQ_STRING_CLS_NAME& operator=(const S& rhs);

        BQ_STRING_CLS_NAME operator+(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs) const;

        BQ_STRING_CLS_NAME operator+(const char* rhs) const;

        BQ_STRING_CLS_NAME& operator+=(const BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& rhs);

        bool operator==(const char_type* str) const;

        bool operator==(const BQ_STRING_CLS_NAME& str) const;

        bool operator!=(const char_type* str) const;

        bool operator!=(const BQ_STRING_CLS_NAME& str) const;

        template <typename S, typename = bq::enable_if_t<(is_std_string_compatible<S>::value || is_std_string_view_compatible<S>::value) && !bq::is_same<S, BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::value>>
        operator S() const;

        typename bq::generic_type_hash_calculator<typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type hash_code() const;

        typename bq::generic_type_hash_calculator<typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>>::hash_value_type hash_code_ignore_case() const;

        const char_type* c_str() const;

        bool is_empty() const;

        BQ_STRING_CLS_NAME substr(typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type pos, typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type count = npos) const;

        typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type find(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const;

        typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type find(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str, typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type search_start_pos) const;

        bool begin_with(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const;

        bool end_with(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const;

        typename BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::size_type find_last(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const;

        BQ_STRING_CLS_NAME replace(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& from, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& to) const;

        bool equals_ignore_case(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str) const;

        bq::array<typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>> split(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& delimiter, bool include_empty_string = false) const;

        BQ_STRING_CLS_NAME trim() const;

    private:
        static uint64_t get_hash(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str);

        static uint64_t get_hash_ignore_case(const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str);

        const char_type* empty_str() const;
    };

    template <typename CHAR_TYPE, typename Allocator>
    bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator> operator+(const typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2);

    template <typename CHAR_TYPE, typename Allocator>
    bool operator==(const typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2);

    template <typename CHAR_TYPE, typename Allocator>
    bool operator!=(const typename bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>::char_type* str1, const bq::BQ_STRING_CLS_NAME<CHAR_TYPE, Allocator>& str2);

    typedef BQ_STRING_CLS_NAME<char> BQ_STRING_INLINE_MACRO(string);
    typedef BQ_STRING_CLS_NAME<char16_t> BQ_STRING_INLINE_MACRO(u16string);
    typedef BQ_STRING_CLS_NAME<char32_t> BQ_STRING_INLINE_MACRO(u32string);
}
