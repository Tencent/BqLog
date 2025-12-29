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
 *
 * \tools
 *
 * \author pippocao
 * \date 2022.08.03
 */
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "bq_common/bq_common_public_include.h"
#include "bq_log/misc/bq_log_def.h"

namespace bq {
    namespace tools {
        template <typename T>
        struct _is_utf8_c_style_string : public bq::bool_type<bq::is_same<char*, bq::decay_t<T>>::value
                                             || bq::is_same<const char*, bq::decay_t<T>>::value> {
        };

        template <typename T>
        struct _is_utf16_c_style_string : public bq::bool_type<
                                              bq::is_same<char16_t*, bq::decay_t<T>>::value
                                              || bq::is_same<const char16_t*, bq::decay_t<T>>::value
                                              || ((bq::is_same<wchar_t*, bq::decay_t<T>>::value
                                                      || bq::is_same<const wchar_t*, bq::decay_t<T>>::value)
                                                  && sizeof(wchar_t) == 2)> {
        };

        template <typename T>
        struct _is_utf32_c_style_string : public bq::bool_type<
                                              bq::is_same<char32_t*, bq::decay_t<T>>::value
                                              || bq::is_same<const char32_t*, bq::decay_t<T>>::value
                                              || ((bq::is_same<wchar_t*, bq::decay_t<T>>::value
                                                      || bq::is_same<const wchar_t*, bq::decay_t<T>>::value)
                                                  && sizeof(wchar_t) == 4)> {
        };

        template <typename T>
        struct _is_c_style_string : public bq::bool_type<_is_utf8_c_style_string<T>::value
                                        || _is_utf16_c_style_string<T>::value
                                        || _is_utf32_c_style_string<T>::value> {
        };

        template <typename T>
        struct _is_std_string_view_like : public bq::bool_type<bq::string::is_std_string_view_compatible<T>::value || bq::u16string::is_std_string_view_compatible<T>::value || bq::u32string::is_std_string_view_compatible<T>::value> { };

        template <typename T>
        struct _is_bq_string_like : public bq::bool_type<bq::string::is_std_string_compatible<bq::decay_t<T>>::value
                                        || bq::u16string::is_std_string_compatible<bq::decay_t<T>>::value
                                        || bq::u32string::is_std_string_compatible<bq::decay_t<T>>::value
                                        || bq::is_same<bq::decay_t<T>, bq::string>::value
                                        || bq::is_same<bq::decay_t<T>, bq::string>::value
                                        || bq::is_same<bq::decay_t<T>, bq::string>::value> { };

        template <typename T>
        struct _is_bq_log_supported_string_type : public bq::bool_type<_is_c_style_string<T>::value || _is_std_string_view_like<T>::value || _is_bq_string_like<T>::value> { };

        template <typename T>
        struct _custom_type_helper {
        private:
            // member function version
            template <typename U, typename V = void>
            struct format_size_member_func_sfinae : public bq::false_type {
            };
            template <typename U>
            struct format_size_member_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq::declval<const U>().bq_log_format_str_size()), size_t>::value>> : public bq::true_type {
            };

            template <typename U, typename V = void>
            struct format_c_str_member_func_sfinae : public bq::false_type {
                typedef void char_type;
            };
            template <typename U>
            struct format_c_str_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_format_str_chars())>, const char*>::value>> : public bq::true_type {
                typedef char char_type;
            };
            template <typename U>
            struct format_c_str_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_format_str_chars())>, const char16_t*>::value>> : public bq::true_type {
                typedef char16_t char_type;
            };
            template <typename U>
            struct format_c_str_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_format_str_chars())>, const char32_t*>::value>> : public bq::true_type {
                typedef char32_t char_type;
            };
            template <typename U>
            struct format_c_str_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_format_str_chars())>, const wchar_t*>::value>> : public bq::true_type {
                typedef wchar_t char_type;
            };
            template <typename U, typename V = void>
            struct format_custom_member_func_sfinae : public bq::false_type {
                typedef void char_type;
            };
            template <typename U>
            struct format_custom_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_custom_format(bq::declval<char*>(), bq::declval<size_t>()))>, void>::value>> : public bq::true_type {
                typedef char char_type;
            };
            template <typename U>
            struct format_custom_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_custom_format(bq::declval<char16_t*>(), bq::declval<size_t>()))>, void>::value>> : public bq::true_type {
                typedef char16_t char_type;
            };
            template <typename U>
            struct format_custom_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_custom_format(bq::declval<char32_t*>(), bq::declval<size_t>()))>, void>::value>> : public bq::true_type {
                typedef char32_t char_type;
            };
            template <typename U>
            struct format_custom_member_func_sfinae<U, bq::enable_if_t<bq::is_same<bq::decay_t<decltype(bq::declval<const U>().bq_log_custom_format(bq::declval<wchar_t*>(), bq::declval<size_t>()))>, void>::value>> : public bq::true_type {
                typedef wchar_t char_type;
            };

            // global function version
            template <typename U, typename V = void>
            struct format_size_global_func_sfinae : public bq::false_type {
            };
            template <typename U>
            struct format_size_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_format_str_size(bq::declval<const U&>())), size_t>::value>> : public bq::true_type {
            };

            template <typename U, typename V = void>
            struct format_c_str_global_func_sfinae : public bq::false_type {
                typedef void char_type;
                static constexpr size_t char_size = 0;
            };
            template <typename U>
            struct format_c_str_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_format_str_chars(bq::declval<const U&>())), const char*>::value>> : public bq::true_type {
                typedef char char_type;
            };
            template <typename U>
            struct format_c_str_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_format_str_chars(bq::declval<const U&>())), const char16_t*>::value>> : public bq::true_type {
                typedef char16_t char_type;
            };
            template <typename U>
            struct format_c_str_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_format_str_chars(bq::declval<const U&>())), const char32_t*>::value>> : public bq::true_type {
                typedef char32_t char_type;
            };
            template <typename U>
            struct format_c_str_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_format_str_chars(bq::declval<const U&>())), const wchar_t*>::value>> : public bq::true_type {
                typedef wchar_t char_type;
            };
            template <typename U, typename V = void>
            struct format_custom_global_func_sfinae : public bq::false_type {
                typedef void char_type;
            };
            template <typename U>
            struct format_custom_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_custom_format(bq::declval<const U&>(), bq::declval<char*>(), bq::declval<size_t>())), void>::value>> : public bq::true_type {
                typedef char char_type;
            };
            template <typename U>
            struct format_custom_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_custom_format(bq::declval<const U&>(), bq::declval<char16_t*>(), bq::declval<size_t>())), void>::value>> : public bq::true_type {
                typedef char16_t char_type;
            };
            template <typename U>
            struct format_custom_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_custom_format(bq::declval<const U&>(), bq::declval<char32_t*>(), bq::declval<size_t>())), void>::value>> : public bq::true_type {
                typedef char32_t char_type;
            };
            template <typename U>
            struct format_custom_global_func_sfinae<U, bq::enable_if_t<bq::is_same<decltype(bq_log_custom_format(bq::declval<const U&>(), bq::declval<wchar_t*>(), bq::declval<size_t>())), void>::value>> : public bq::true_type {
                typedef wchar_t char_type;
            };

        public:
            static constexpr bool has_member_size_func = format_size_member_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool has_global_size_func = !has_member_size_func && format_size_global_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool has_member_custom_format_func = format_custom_member_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool has_global_custom_format_func = !has_member_custom_format_func && format_custom_global_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool has_member_c_str_func = (!has_member_custom_format_func) && (!has_global_custom_format_func) && format_c_str_member_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool has_global_c_str_func = (!has_member_custom_format_func) && (!has_global_custom_format_func) && (!has_member_c_str_func) && format_c_str_global_func_sfinae<bq::decay_t<T>>::value;
            static constexpr bool is_valid = (has_member_size_func || has_global_size_func) && (has_member_c_str_func || has_global_c_str_func || has_member_custom_format_func || has_global_custom_format_func);
            using char_type = bq::condition_type_t<
                has_member_c_str_func,
                typename format_c_str_member_func_sfinae<bq::decay_t<T>>::char_type,
                bq::condition_type_t<has_global_c_str_func, typename format_c_str_global_func_sfinae<bq::decay_t<T>>::char_type,
                    bq::condition_type_t<has_member_custom_format_func, typename format_custom_member_func_sfinae<bq::decay_t<T>>::char_type,
                        bq::condition_type_t<has_global_custom_format_func, typename format_custom_global_func_sfinae<bq::decay_t<T>>::char_type,
                            char>>>>;
        };

        template <typename STR>
        struct _is_bq_log_format_type {
            static constexpr bool value = _is_bq_log_supported_string_type<STR>::value
                || _custom_type_helper<STR>::is_valid;
        };

        enum class _serialize_func_type {
            utf8_string,
            utf16_string,
            utf32_string,
            custom_type,
            null_pointer,
            pointer,
            pod,
            others
        };

        enum class _string_type {
            c_style_type,
            array_type,
            cls_type
        };

        template <typename T>
        constexpr _serialize_func_type _get_serialize_func_type()
        {
            return bq::condition_value<_custom_type_helper<T>::is_valid, _serialize_func_type, _serialize_func_type::custom_type,
                bq::condition_value<bq::tools::_is_utf8_c_style_string<T>::value || bq::string::is_std_string_compatible<T>::value || bq::string::is_std_string_view_compatible<T>::value, _serialize_func_type, _serialize_func_type::utf8_string,
                    bq::condition_value<bq::tools::_is_utf16_c_style_string<T>::value || bq::u16string::is_std_string_compatible<T>::value || bq::u16string::is_std_string_view_compatible<T>::value, _serialize_func_type, _serialize_func_type::utf16_string,
                        bq::condition_value<bq::tools::_is_utf32_c_style_string<T>::value || bq::u32string::is_std_string_compatible<T>::value || bq::u32string::is_std_string_view_compatible<T>::value, _serialize_func_type, _serialize_func_type::utf32_string,
                            bq::condition_value<bq::is_null_pointer<T>::value, _serialize_func_type, _serialize_func_type::null_pointer,
                                bq::condition_value<bq::is_pointer<bq::decay_t<T>>::value, _serialize_func_type, _serialize_func_type::pointer,
                                    bq::condition_value<bq::is_pod<bq::decay_t<T>>::value, _serialize_func_type, _serialize_func_type::pod,
                                        _serialize_func_type::others>::value>::value>::value>::value>::value>::value>::value;
        }

        template <typename T, _serialize_func_type TYPE>
        struct _is_serialize_func_type {
            constexpr static bool value = (_get_serialize_func_type<T>() == TYPE);
        };

        template <typename T>
        constexpr log_arg_type_enum _get_log_param_type_enum()
        {
            return bq::condition_value < (_get_serialize_func_type<T>() == _serialize_func_type::custom_type) && bq::is_same<typename _custom_type_helper<T>::char_type, char>::value, log_arg_type_enum, log_arg_type_enum::string_utf8_type,
                   bq::condition_value < (_get_serialize_func_type<T>() == _serialize_func_type::custom_type) && !bq::is_same<typename _custom_type_helper<T>::char_type, char>::value, log_arg_type_enum, log_arg_type_enum::string_utf16_type,
                   bq::condition_value<(_get_serialize_func_type<T>() == _serialize_func_type::utf8_string), log_arg_type_enum, log_arg_type_enum::string_utf8_type,
                       bq::condition_value<_get_serialize_func_type<T>() == _serialize_func_type::utf16_string || _get_serialize_func_type<T>() == _serialize_func_type::utf32_string, log_arg_type_enum, log_arg_type_enum::string_utf16_type, // all the string exclude utf8 will be encoded into utf16.
                           bq::condition_value<bq::is_null_pointer<T>::value, log_arg_type_enum, log_arg_type_enum::null_type,
                               bq::condition_value<bq::is_pointer<T>::value, log_arg_type_enum, log_arg_type_enum::pointer_type,
                                   bq::condition_value<bq::is_same<T, bool>::value, log_arg_type_enum, log_arg_type_enum::bool_type,
                                       bq::condition_value<bq::is_same<T, char>::value, log_arg_type_enum, log_arg_type_enum::char_type,
                                           bq::condition_value<bq::is_same<T, char16_t>::value, log_arg_type_enum, log_arg_type_enum::char16_type,
                                               bq::condition_value<bq::is_same<T, wchar_t>::value && sizeof(wchar_t) == 2, log_arg_type_enum, log_arg_type_enum::char16_type,
                                                   bq::condition_value<bq::is_same<T, wchar_t>::value && sizeof(wchar_t) == 4, log_arg_type_enum, log_arg_type_enum::char32_type,
                                                       bq::condition_value<bq::is_same<T, char32_t>::value, log_arg_type_enum, log_arg_type_enum::char32_type,
                                                           bq::condition_value<bq::is_same<T, int8_t>::value, log_arg_type_enum, log_arg_type_enum::int8_type,
                                                               bq::condition_value<bq::is_same<T, uint8_t>::value, log_arg_type_enum, log_arg_type_enum::uint8_type,
                                                                   bq::condition_value<bq::is_same<T, int16_t>::value, log_arg_type_enum, log_arg_type_enum::int16_type,
                                                                       bq::condition_value<bq::is_same<T, uint16_t>::value, log_arg_type_enum, log_arg_type_enum::uint16_type,
                                                                           bq::condition_value<bq::is_same<T, int32_t>::value, log_arg_type_enum, log_arg_type_enum::int32_type,
                                                                               bq::condition_value<bq::is_same<T, uint32_t>::value, log_arg_type_enum, log_arg_type_enum::uint32_type,
                                                                                   bq::condition_value<bq::is_same<T, int64_t>::value, log_arg_type_enum, log_arg_type_enum::int64_type,
                                                                                       bq::condition_value<bq::is_same<T, uint64_t>::value, log_arg_type_enum, log_arg_type_enum::uint64_type,
                                                                                           bq::condition_value<bq::is_same<T, float>::value, log_arg_type_enum, log_arg_type_enum::float_type,
                                                                                               bq::condition_value<bq::is_same<T, double>::value, log_arg_type_enum, log_arg_type_enum::double_type,
                                                                                                   bq::condition_value<(bq::is_pod<T>::value && sizeof(T) == 1), log_arg_type_enum, log_arg_type_enum::int8_type,
                                                                                                       bq::condition_value<(bq::is_pod<T>::value && sizeof(T) == 2), log_arg_type_enum, log_arg_type_enum::int16_type,
                                                                                                           bq::condition_value<(bq::is_pod<T>::value && sizeof(T) == 4), log_arg_type_enum, log_arg_type_enum::int32_type,
                                                                                                               bq::condition_value<(bq::is_pod<T>::value && sizeof(T) == 8), log_arg_type_enum, log_arg_type_enum::int64_type,
                                                                                                                   log_arg_type_enum::unsupported_type>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value>::value
                       > ::value > ::value;
        }

        template <typename FIRST, typename... REST>
        struct is_params_valid_type_helper {
            constexpr static bool value = (_get_log_param_type_enum<FIRST>() != log_arg_type_enum::unsupported_type) && is_params_valid_type_helper<REST...>::value;
        };

        template <typename FIRST>
        struct is_params_valid_type_helper<FIRST> {
            constexpr static bool value = _get_log_param_type_enum<FIRST>() != log_arg_type_enum::unsupported_type;
        };

        template <typename T>
        struct is_type_constexpr_size {
            static constexpr bool value = !(_is_serialize_func_type<T, _serialize_func_type::custom_type>::value
                || _is_serialize_func_type<T, _serialize_func_type::utf8_string>::value
                || _is_serialize_func_type<T, _serialize_func_type::utf16_string>::value
                || _is_serialize_func_type<T, _serialize_func_type::utf32_string>::value);
        };

        template <bool INCLUDE_TYPE_INFO>
        bq_forceinline bq::enable_if_t<INCLUDE_TYPE_INFO, void> _type_copy_type_info(uint8_t* data_addr, log_arg_type_enum type_value)
        {
            *data_addr = static_cast<uint8_t>(type_value);
        }

        template <bool INCLUDE_TYPE_INFO>
        bq_forceinline bq::enable_if_t<!INCLUDE_TYPE_INFO, void> _type_copy_type_info(uint8_t* data_addr, log_arg_type_enum type_value)
        {
            (void)data_addr;
            (void)type_value;
        }
        //================================================string helpers begin==========================================
        // we can distinguish c style string ,literal string and char array at compiling time or runtime now.
        template <typename CHAR_TYPE>
        static bq_forceinline size_t _get_utf32_c_style_string_storage_size_impl(const CHAR_TYPE* str, size_t max_character_count)
        {
            static_assert(sizeof(CHAR_TYPE) == 4, "_get_utf32_c_style_string_storage_size_impl only for utf32 characters");
            size_t str_len = 0;
            for (size_t i = 0; i < max_character_count; ++i) {
                CHAR_TYPE c = str[i];
                if (c == 0) {
                    break;
                }
                if (str[i] <= 0xFFFF) {
                    ++str_len;
                } else {
                    str_len += 2;
                }
            }
            return str_len * sizeof(char16_t);
        }

        template <typename CHAR_TYPE, size_t CHAR_SIZE>
        struct _serialize_str_helper_by_encode_impl {
            template <size_t N>
            static bq_forceinline size_t get_array_string_storage_size(const CHAR_TYPE (&str)[N])
            {
                return ((str[N - 1] == '\0') ? (N - 1) : N) * sizeof(CHAR_TYPE);
            }

            template <typename T>
            static bq_forceinline size_t get_class_string_storage_size(const T& str)
            {
                return str.size() * sizeof(CHAR_TYPE);
            }

            static bq_forceinline size_t get_c_style_string_storage_size(const CHAR_TYPE* str)
            {
                return bq::string_tools::string_len(str) * sizeof(CHAR_TYPE);
            }

            static bq_forceinline size_t get_c_style_string_storage_size(const CHAR_TYPE* str, size_t char_count)
            {
                (void)str;
                return char_count * sizeof(CHAR_TYPE);
            }

            static bq_forceinline void c_style_string_copy(void* tar, const CHAR_TYPE* src, size_t target_buffer_size)
            {
                if (target_buffer_size > 0) {
                    memcpy(tar, src, target_buffer_size);
                }
            }
        };

        template <typename CHAR_TYPE>
        struct _serialize_str_helper_by_encode_impl<CHAR_TYPE, 4> {
            template <size_t N>
            static bq_forceinline size_t get_array_string_storage_size(const CHAR_TYPE (&str)[N])
            {
                return _get_utf32_c_style_string_storage_size_impl(str, N);
            }

            template <typename T>
            static bq_forceinline bq::enable_if_t<_is_bq_string_like<T>::value, size_t> get_class_string_storage_size(const T& str)
            {
                auto size = str.size();
                if (size == 0) {
                    return 0;
                }
                return _get_utf32_c_style_string_storage_size_impl(str.c_str(), size);
            }

            template <typename T>
            static bq_forceinline bq::enable_if_t<_is_std_string_view_like<T>::value, size_t> get_class_string_storage_size(const T& str)
            {
                auto size = str.size();
                if (size == 0) {
                    return 0;
                }
                return _get_utf32_c_style_string_storage_size_impl(str.data(), size);
            }

            static bq_forceinline size_t get_c_style_string_storage_size(const CHAR_TYPE* str)
            {
                return _get_utf32_c_style_string_storage_size_impl(str, SIZE_MAX);
            }

            static bq_forceinline size_t get_c_style_string_storage_size(const CHAR_TYPE* str, size_t char_count)
            {
                return _get_utf32_c_style_string_storage_size_impl(str, char_count);
            }

            static bq_forceinline void c_style_string_copy(void* tar, const CHAR_TYPE* src, size_t target_buffer_size)
            {
                size_t target_char_size = target_buffer_size >> 1;
                size_t cursor = 0;
                char16_t* target_buffer = (char16_t*)tar;
                for (size_t i = 0; cursor < target_char_size; ++i) {
                    CHAR_TYPE c = src[i];
                    if (c <= 0xFFFF) {
                        target_buffer[cursor++] = static_cast<char16_t>(c);
                    } else {
                        c -= 0x10000;
                        target_buffer[cursor++] = static_cast<char16_t>((c >> 10) + 0xD800);
                        target_buffer[cursor++] = static_cast<char16_t>((c & 0x3FF) + 0xDC00);
                    }
                }
            }
        };

        template <typename CHAR_TYPE>
        struct _serialize_str_helper_by_encode : public _serialize_str_helper_by_encode_impl<CHAR_TYPE, sizeof(CHAR_TYPE)> {
        };

        template <bool INCLUDE_TYPE_INFO, _string_type STR_TYPE>
        struct _serialize_str_helper_by_type {
        };
        template <bool INCLUDE_TYPE_INFO>
        struct _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, _string_type::array_type> {
            template <typename CHAR_TYPE, size_t N>
            static bq_forceinline size_t get_storage_data_size(const CHAR_TYPE (&str)[N])
            {
                static_assert(sizeof(CHAR_TYPE) == 1 || sizeof(CHAR_TYPE) == 2 || sizeof(CHAR_TYPE) == 4, "invalid char type!");
                static_assert(N > 0, "char array dimension must not be zero");
                size_t str_data_len = _serialize_str_helper_by_encode<CHAR_TYPE>::get_array_string_storage_size(str);
                return str_data_len;
            }
            template <typename CHAR_TYPE, size_t N>
            static bq_forceinline void type_copy(const CHAR_TYPE (&value)[N], uint8_t* data_addr, size_t data_size)
            {
                static_assert(sizeof(CHAR_TYPE) == 1 || sizeof(CHAR_TYPE) == 2 || sizeof(CHAR_TYPE) == 4, "invalid char type!");
                _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<CHAR_TYPE*>());
                data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
                // potential risk, data_size may bigger than max uint32_t
                uint32_t string_size = (uint32_t)(data_size - bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4 + sizeof(uint32_t), sizeof(uint32_t)>::value);
                *((uint32_t*)(data_addr)) = string_size;
                data_addr += sizeof(uint32_t);
                _serialize_str_helper_by_encode<CHAR_TYPE>::c_style_string_copy(data_addr, value, string_size);
            }
        };
        template <bool INCLUDE_TYPE_INFO>
        struct _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, _string_type::c_style_type> {
            template <typename CHAR_TYPE>
            static bq_forceinline size_t get_storage_data_size(const CHAR_TYPE* str)
            {
                static_assert(sizeof(CHAR_TYPE) == 1 || sizeof(CHAR_TYPE) == 2 || sizeof(CHAR_TYPE) == 4, "invalid char type!");
                if (!str) {
                    return _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, _string_type::array_type>::get_storage_data_size("null");
                }
                size_t str_data_len = _serialize_str_helper_by_encode<CHAR_TYPE>::get_c_style_string_storage_size(str);
                return str_data_len;
            }

            template <typename CHAR_TYPE>
            static bq_forceinline void type_copy(const CHAR_TYPE* value, uint8_t* data_addr, size_t data_size)
            {
                static_assert(sizeof(CHAR_TYPE) == 1 || sizeof(CHAR_TYPE) == 2 || sizeof(CHAR_TYPE) == 4, "invalid char type!");
                if (!value) {
                    _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, _string_type::array_type>::type_copy("null", data_addr, data_size);
                } else {
                    _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<CHAR_TYPE*>());
                    data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
                    // potential risk, data_size may bigger than max uint32_t
                    uint32_t string_size = (uint32_t)(data_size - bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4 + sizeof(uint32_t), sizeof(uint32_t)>::value);
                    *((uint32_t*)(data_addr)) = string_size;
                    data_addr += sizeof(uint32_t);
                    _serialize_str_helper_by_encode<CHAR_TYPE>::c_style_string_copy(data_addr, value, string_size);
                }
            }
        };
        template <bool INCLUDE_TYPE_INFO>
        struct _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, _string_type::cls_type> {
            template <typename STRING_TYPE>
            static bq_forceinline size_t get_storage_data_size(const STRING_TYPE& str)
            {
                static_assert(sizeof(typename STRING_TYPE::value_type) == 1 || sizeof(typename STRING_TYPE::value_type) == 2 || sizeof(typename STRING_TYPE::value_type) == 4, "invalid char type!");
                size_t str_data_len = _serialize_str_helper_by_encode<typename STRING_TYPE::value_type>::get_class_string_storage_size(str);
                return str_data_len;
            }

            template <typename STRING_TYPE>
            static bq_forceinline bq::enable_if_t<_is_bq_string_like<STRING_TYPE>::value, void> type_copy(const STRING_TYPE& str, uint8_t* data_addr, size_t data_size)
            {
                static_assert(sizeof(typename STRING_TYPE::value_type) == 1 || sizeof(typename STRING_TYPE::value_type) == 2 || sizeof(typename STRING_TYPE::value_type) == 4, "invalid char type!");
                _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<STRING_TYPE>());
                data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
                // potential risk, data_size may bigger than max uint32_t
                uint32_t string_size = (uint32_t)(data_size - bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4 + sizeof(uint32_t), sizeof(uint32_t)>::value);
                *((uint32_t*)(data_addr)) = string_size;
                data_addr += sizeof(uint32_t);
                if (string_size > 0) {
                    _serialize_str_helper_by_encode<typename STRING_TYPE::value_type>::c_style_string_copy(data_addr, str.c_str(), string_size);
                }
            }

            template <typename STRING_TYPE>
            static bq_forceinline bq::enable_if_t<_is_std_string_view_like<STRING_TYPE>::value, void> type_copy(const STRING_TYPE& str, uint8_t* data_addr, size_t data_size)
            {
                static_assert(sizeof(typename STRING_TYPE::value_type) == 1 || sizeof(typename STRING_TYPE::value_type) == 2 || sizeof(typename STRING_TYPE::value_type) == 4, "invalid char type!");
                _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<STRING_TYPE>());
                data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
                // potential risk, data_size may bigger than max uint32_t
                uint32_t string_size = (uint32_t)(data_size - bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4 + sizeof(uint32_t), sizeof(uint32_t)>::value);
                *((uint32_t*)(data_addr)) = string_size;
                data_addr += sizeof(uint32_t);
                if (string_size > 0) {
                    _serialize_str_helper_by_encode<typename STRING_TYPE::value_type>::c_style_string_copy(data_addr, str.data(), string_size);
                }
            }
        };

        //================================================string helpers end============================================

        //================================================get storage size begin========================================
        template <bool INCLUDE_TYPE_INFO>
        bq_forceinline size_t get_string_field_total_storage_size(size_t str_storage_size)
        {
            return bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value
                + sizeof(uint32_t)
                + str_storage_size;
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_member_size_func, size_t>
        _get_storage_data_size(const T& arg)
        {
            return get_string_field_total_storage_size<INCLUDE_TYPE_INFO>(sizeof(typename _custom_type_helper<T>::char_type) * arg.bq_log_format_str_size());
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_global_size_func, size_t>
        _get_storage_data_size(const T& arg)
        {
            return get_string_field_total_storage_size<INCLUDE_TYPE_INFO>(sizeof(typename _custom_type_helper<T>::char_type) * bq_log_format_str_size(arg));
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_bq_log_supported_string_type<T>::value, size_t>
        _get_storage_data_size(const T& str)
        {
            constexpr _string_type type = bq::condition_value<(_is_bq_string_like<T>::value || _is_std_string_view_like<T>::value),
                _string_type,
                _string_type::cls_type,
                bq::condition_value<bq::is_array<T>::value, _string_type,
                    _string_type::array_type, _string_type::c_style_type>::value>::value;
            return get_string_field_total_storage_size<INCLUDE_TYPE_INFO>(_serialize_str_helper_by_type<INCLUDE_TYPE_INFO, type>::get_storage_data_size(str));
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline constexpr bq::enable_if_t<!is_type_constexpr_size<T>::value, size_t>
        _get_storage_data_size_constexpr()
        {
            // dummy
            return (size_t)-1;
        }

        // nullptr get_storage_data_size
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline constexpr bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::null_pointer>::value, size_t>
        _get_storage_data_size_constexpr()
        {
            return bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
        }

        // pointer get_storage_data_size
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline constexpr bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::pointer>::value, size_t>
        _get_storage_data_size_constexpr()
        {
            return sizeof(uint64_t) + bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
        }

        // pod get_storage_data_size
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline constexpr bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::pod>::value, size_t>
        _get_storage_data_size_constexpr()
        {
            static_assert(bq::is_pod<bq::decay_t<T>>::value
                    && (!bq::is_pointer<bq::decay_t<T>>::value),
                "POD test failed");
            return sizeof(T)
                + bq::condition_value<INCLUDE_TYPE_INFO, size_t,
                    bq::condition_value<sizeof(T) <= 2, size_t, 2, 4>::value, 0>::value;
        }

        // other type get_storage_data_size
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline constexpr bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::others>::value, size_t>
        _get_storage_data_size_constexpr()
        {
            static_assert(_get_serialize_func_type<T>() != _serialize_func_type::others, "unsupported log arg type");
            return 0;
        }
        //================================================get storage size end========================================

        //=====================================================================size_seq begin=======================================================================================
        /**
         * This is similar to std::tuple<size_t>, but it allows mixing constexpr elements with runtime elements.
         */
        template <size_t H, size_t V, bool C>
        struct size_seq_element {
            static constexpr bool is_constexpr = C;
            constexpr size_t get_value() const
            {
                return V;
            }
            constexpr size_t get_aligned_value() const
            {
                return align_4(V);
            }
        };

        template <size_t H, size_t V>
        struct EBCO size_seq_element<H, V, false> {
            static constexpr bool is_constexpr = false;
            size_t value;
            size_t get_value() const
            {
                return value;
            }
            constexpr size_t get_aligned_value() const
            {
                return align_4(value);
            }
        };

        template <bool INCLUDE_TYPE_INFO, typename... TYPES>
        struct EBCO size_seq {
            size_t get_total() const
            {
                return 0;
            }
        };

        template <bool INCLUDE_TYPE_INFO, size_t H, size_t V, bool C, typename... TYPES>
        struct EBCO size_seq_impl : public size_seq_element<H, V, C>, public size_seq<INCLUDE_TYPE_INFO, TYPES...> {
            using element_type = size_seq_element<H, V, C>;
        };

        template <bool INCLUDE_TYPE_INFO, typename FIRST, typename... REST>
        struct EBCO size_seq<INCLUDE_TYPE_INFO, FIRST, REST...> : public size_seq_impl<INCLUDE_TYPE_INFO, sizeof...(REST) + 1,
                                                                      is_type_constexpr_size<FIRST>::value ? _get_storage_data_size_constexpr<INCLUDE_TYPE_INFO, FIRST>() : 0,
                                                                      is_type_constexpr_size<FIRST>::value ? true : false,
                                                                      REST...> {
            using next_type = size_seq<INCLUDE_TYPE_INFO, REST...>;
            using impl_type = size_seq_impl<INCLUDE_TYPE_INFO,
                sizeof...(REST) + 1,
                is_type_constexpr_size<FIRST>::value ? _get_storage_data_size_constexpr<INCLUDE_TYPE_INFO, FIRST>() : 0,
                is_type_constexpr_size<FIRST>::value ? true : false,
                REST...>;
            using element_type = typename impl_type::element_type;
            next_type& get_next()
            {
                return (*this);
            }
            const next_type& get_next() const
            {
                return (*this);
            }
            element_type& get_element()
            {
                return (*this);
            }
            const element_type& get_element() const
            {
                return (*this);
            }
            size_t get_total() const
            {
                return get_element().get_aligned_value() + get_next().get_total();
            }
        };

        template <typename T, bool INCLUDE_TYPE_INFO, typename... TYPES>
        void fill_size_seq_impl(size_seq<INCLUDE_TYPE_INFO, TYPES...>& seq, const T& value, const bq::false_type& is_constexpr)
        {
            (void)is_constexpr;
            seq.get_element().value = _get_storage_data_size<INCLUDE_TYPE_INFO>(value);
        }

        template <typename T, bool INCLUDE_TYPE_INFO, typename... TYPES>
        void fill_size_seq_impl(size_seq<INCLUDE_TYPE_INFO, TYPES...>& seq, const T& value, const bq::true_type& is_constexpr)
        {
            (void)seq;
            (void)value;
            (void)is_constexpr;
        }

        template <bool INCLUDE_TYPE_INFO, typename FIRST>
        bq::enable_if_t<_get_serialize_func_type<FIRST>() != _serialize_func_type::others, void> fill_size_seq(size_seq<INCLUDE_TYPE_INFO, FIRST>& seq, const FIRST& first)
        {
            using element_type = typename bq::remove_reference_t<decltype(seq)>::element_type;
            constexpr bool is_constexpr = element_type::is_constexpr;
            fill_size_seq_impl(seq, first, bq::bool_type<is_constexpr> {});
        }

        template <bool INCLUDE_TYPE_INFO, typename FIRST, typename... REST>
        bq::enable_if_t<_get_serialize_func_type<FIRST>() != _serialize_func_type::others, void> fill_size_seq(size_seq<INCLUDE_TYPE_INFO, FIRST, REST...>& seq, const FIRST& first, const REST&... rest)
        {
            using element_type = typename bq::remove_reference_t<decltype(seq)>::element_type;
            constexpr bool is_constexpr = element_type::is_constexpr;
            fill_size_seq_impl(seq, first, bq::bool_type<is_constexpr> {});
            fill_size_seq(seq.get_next(), rest...);
        }

        template <bool INCLUDE_TYPE_INFO, typename... PARAMS>
        bq::enable_if_t<is_params_valid_type_helper<PARAMS...>::value, size_seq<INCLUDE_TYPE_INFO, PARAMS...>> make_size_seq(const PARAMS&... params)
        {
            size_seq<INCLUDE_TYPE_INFO, PARAMS...> init_seq;
            fill_size_seq(init_seq, params...);
            return init_seq;
        }

        template <bool INCLUDE_TYPE_INFO, typename CHAR_TYPE>
        size_seq<INCLUDE_TYPE_INFO, const CHAR_TYPE*> make_single_string_size_seq(size_t known_data_size)
        {
            size_seq<INCLUDE_TYPE_INFO, const CHAR_TYPE*> init_seq;
            init_seq.get_element().value = get_string_field_total_storage_size<INCLUDE_TYPE_INFO>(known_data_size);
            return init_seq;
        }
        //=====================================================================size_seq end=========================================================================================

        //================================================type copy begin========================================
        // custom type copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_member_custom_format_func>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<T>());
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
            data_size -= (bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value + sizeof(uint32_t));
            *(uint32_t*)data_addr = (uint32_t)data_size;
            data_addr += sizeof(uint32_t);
            value.bq_log_custom_format(reinterpret_cast<typename _custom_type_helper<T>::char_type*>(data_addr), data_size);
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_global_custom_format_func>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<T>());
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
            data_size -= (bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value + sizeof(uint32_t));
            *(uint32_t*)data_addr = (uint32_t)data_size;
            data_addr += sizeof(uint32_t);
            bq_log_custom_format(value, reinterpret_cast<typename _custom_type_helper<T>::char_type*>(data_addr), data_size);
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_member_c_str_func>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<T>());
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
            data_size -= (bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value + sizeof(uint32_t));
            *(uint32_t*)data_addr = (uint32_t)data_size;
            data_addr += sizeof(uint32_t);
            _serialize_str_helper_by_encode<typename _custom_type_helper<T>::char_type>::c_style_string_copy(data_addr, value.bq_log_format_str_chars(), data_size);
        }

        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::custom_type>::value && _custom_type_helper<T>::has_global_c_str_func>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, _get_log_param_type_enum<T>());
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
            data_size -= (bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value + sizeof(uint32_t));
            *(uint32_t*)data_addr = (uint32_t)data_size;
            data_addr += sizeof(uint32_t);
            _serialize_str_helper_by_encode<typename _custom_type_helper<T>::char_type>::c_style_string_copy(data_addr, bq_log_format_str_chars(value), data_size);
        }

        // string type_copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_bq_log_supported_string_type<T>::value>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            constexpr _string_type type = bq::condition_value<(_is_bq_string_like<T>::value || _is_std_string_view_like<T>::value),
                _string_type,
                _string_type::cls_type,
                bq::condition_value<bq::is_array<T>::value, _string_type,
                    _string_type::array_type, _string_type::c_style_type>::value>::value;
            _serialize_str_helper_by_type<INCLUDE_TYPE_INFO, type>::type_copy(value, data_addr, data_size);
        }

        // nullptr type_copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::null_pointer>::value>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            (void)value;
            (void)data_size;
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, log_arg_type_enum::null_type);
        }

        // pointer type_copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::pointer>::value>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            (void)data_size;
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, log_arg_type_enum::pointer_type);
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t, 4, 0>::value;
            uint64_t addr_value = (uint64_t)(value); // 64 bit
            *(uint64_t*)(data_addr) = addr_value;
        }

        // pod type_copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::pod>::value>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            (void)data_size;
            typedef bq::decay_t<T> pod_type;
            constexpr auto type_enum = _get_log_param_type_enum<T>();
            _type_copy_type_info<INCLUDE_TYPE_INFO>(data_addr, type_enum);
            data_addr += bq::condition_value<INCLUDE_TYPE_INFO, size_t,
                bq::condition_value<sizeof(pod_type) <= 2, size_t, 2, 4>::value, 0>::value;
            *(pod_type*)(data_addr) = (pod_type)value;
        }

        // other type type_copy
        template <bool INCLUDE_TYPE_INFO, typename T>
        bq_forceinline bq::enable_if_t<_is_serialize_func_type<T, _serialize_func_type::others>::value>
        _type_copy(const T& value, uint8_t* data_addr, size_t data_size)
        {
            (void)value;
            (void)data_addr;
            (void)data_size;
            static_assert(_get_serialize_func_type<T>() != _serialize_func_type::others, "unsupported log arg type");
            return 0;
        }
        //================================================type copy end========================================
    }
}
