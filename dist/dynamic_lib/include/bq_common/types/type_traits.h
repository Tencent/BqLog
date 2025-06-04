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
 * \file type_traits.h
 * \we do not depend on STL for compatible reason.
 * \author pippocao
 * \date 2022.08.05
 *
 *
 */
#include <stdint.h>
#include <stddef.h>
#include "bq_common/platform/macros.h"

namespace bq {
    template <bool B>
    struct bool_type {
        static constexpr bool value = false;
    };
    template <>
    struct bool_type<true> {
        static constexpr bool value = true;
    };
    using true_type = bool_type<true>;
    using false_type = bool_type<false>;
    template <typename T>
    struct always_false : bq::false_type {
    };
    //------------------------------------------------------------------------------------------
    template <typename T1, typename T2>
    struct is_same : public false_type {
    };
    template <typename T>
    struct is_same<T, T> : public true_type {
    };
#if defined(BQ_CPP_14)
    template <typename T1, typename T2>
    constexpr bool is_same_v = is_same<T1, T2>::value;
#endif
    //------------------------------------------------------------------------------------------
    template <typename... T>
    struct make_void {
        using type = void;
    };
    template <typename... T>
    using void_t = typename make_void<T...>::type;

    template <typename T>
    struct is_pod : public bool_type<__is_pod(T)> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_pod_v = is_pod<T>::value;
#endif
    //------------------------------------------------------------------------------------------

    template <typename Base, typename Derived>
    struct is_base_of : bool_type<__is_base_of(Base, Derived)> {
    };

#if defined(BQ_CPP_14)
    template <typename Base, typename Derived>
    constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;
#endif
    //------------------------------------------------------------------------------------------

    template <typename T>
    struct is_array : public false_type {
    };
    template <typename T, size_t N>
    struct is_array<T[N]> : public true_type {
    };
    template <typename T>
    struct is_array<T[]> : public true_type {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_array_v = is_array<T>::value;
#endif
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct is_enum : public bool_type<__is_enum(T)> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_enum_v = is_enum<T>::value;
#endif
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct is_pointer : public false_type {
    };
    template <typename T>
    struct is_pointer<T*> : public true_type {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_pointer_v = is_pointer<T>::value;
#endif
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct is_reference : public false_type {
    };
    template <typename T>
    struct is_reference<T&> : public true_type {
    };
    template <typename T>
    struct is_reference<T&&> : public true_type {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_reference_v = is_reference<T>::value;
#endif
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct add_const {
        using type = const T;
    };
    template <typename T>
    struct add_const<const T> {
        using type = const T;
    };
    template <typename T>
    using add_const_t = typename add_const<T>::type;

    //------------------------------------------------------------------------------------------
    template <typename T>
    struct is_const : public false_type {
    };
    template <typename T>
    struct is_const<const T> : public true_type {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_const_v = is_const<T>::value;
#endif
    //------------------------------------------------------------------------------------------
    // inspired by MSVC-libC++ : "only function types and reference types can't be const qualified"
    // simple but useful, GNU libC++ and boost list all the situations.
    // see https://gcc.gnu.org/onlinedocs/gcc-8.1.0/libstdc++/api/a00167_source.html#l00453
    template <typename T>
    struct is_function : public bool_type<!is_const<const T>::value && !is_reference<T>::value> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_function_v = is_function<T>::value;
#endif
    //-------------------------------------------------------------------------------------------

    template <typename T>
    struct remove_reference {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T&> {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T&&> {
        using type = T;
    };
    template <class T>
    using remove_reference_t = typename remove_reference<T>::type;
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct add_lvalue_reference {
        using type = typename remove_reference<T>::type&;
    };
    template <class T>
    using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;
    template <typename T>
    struct add_rvalue_reference {
        using type = typename remove_reference<T>::type&&;
    };
    template <class T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;
    //------------------------------------------------------------------------------------------
    template <typename T>
    struct remove_cv {
        using type = T;
    };
    template <typename T>
    struct remove_cv<const T> {
        using type = T;
    };
    template <typename T>
    struct remove_cv<volatile T> {
        using type = T;
    };
    template <typename T>
    struct remove_cv<const volatile T> {
        using type = T;
    };
    template <class T>
    using remove_cv_t = typename remove_cv<T>::type;
    //------------------------------------------------------------------------------------------

    template <class T>
    struct is_null_pointer : public bool_type<bq::is_same<decltype(nullptr), typename bq::remove_cv_t<T>>::value> {
    };
#if defined(BQ_CPP_14)
    template <class T>
    constexpr bool is_null_pointer_v = is_null_pointer<T>::value;
#endif
    //---------------------------------------------------------------------------------------------
    template <typename T>
    T&& forward(typename remove_reference<T>::type& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    template <class T>
    T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        return static_cast<T&&>(t);
    }
    //------------------------------------------------------------------------------------------

    template <typename T>
    constexpr typename remove_reference<T>::type&& move(T&& arg) noexcept
    {
        return static_cast<typename remove_reference<T>::type&&>(arg);
    }
    //------------------------------------------------------------------------------------------

    namespace decay_helper {
        template <typename T>
        struct decay_non_reference {
            typedef remove_cv_t<T> type;
        };

        template <typename T>
        struct decay_non_reference<T[]> {
            typedef T* type;
        };

        template <typename T, uint32_t N>
        struct decay_non_reference<T[N]> {
            typedef T* type;
        };
    }

    template <typename T>
    struct decay {
        using type = typename decay_helper::decay_non_reference<remove_reference_t<T>>::type;
    };
    template <typename T>
    using decay_t = typename decay<T>::type;
    //------------------------------------------------------------------------------------------

    template <bool Test, typename T = void>
    struct enable_if {
    };

    template <typename T>
    struct enable_if<true, T> {
        using type = T;
    };
    template <bool Test, typename T = void>
    using enable_if_t = typename enable_if<Test, T>::type;

    template <bool Test, typename T>
    struct lazy_enable_if {
    };

    template <typename T>
    struct lazy_enable_if<true, T> {
        using type = T;
    };
    template <bool Test, typename T = void>
    using lazy_enable_if_t = typename lazy_enable_if<Test, T>::type;

    //------------------------------------------------------------------------------------------

    template <typename T>
    typename add_rvalue_reference<T>::type declval() noexcept
    {
        static_assert(bq::always_false<T>::value, "calling bq::declval()");
    }
    template <typename T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

    //------------------------------------------------------------------------------------------
    template <typename T>
    struct is_basic_types : bool_type<is_same<typename decay<T>::type, char>::value
                                || is_same<typename decay<T>::type, wchar_t>::value
                                || is_same<typename decay<T>::type, char16_t>::value
                                || is_same<typename decay<T>::type, char32_t>::value
                                || is_same<typename decay<T>::type, bool>::value
                                || is_same<typename decay<T>::type, uint8_t>::value
                                || is_same<typename decay<T>::type, int8_t>::value
                                || is_same<typename decay<T>::type, uint16_t>::value
                                || is_same<typename decay<T>::type, int16_t>::value
                                || is_same<typename decay<T>::type, uint32_t>::value
                                || is_same<typename decay<T>::type, int32_t>::value
                                || is_same<typename decay<T>::type, uint64_t>::value
                                || is_same<typename decay<T>::type, int64_t>::value
                                || is_same<typename decay<T>::type, size_t>::value
                                || is_same<typename decay<T>::type, float>::value
                                || is_same<typename decay<T>::type, double>::value
                                || is_enum<typename decay<T>::type>::value
                                || is_pointer<typename decay<T>::type>::value
                                || is_null_pointer<typename decay<T>::type>::value> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_basic_types_v = is_basic_types<T>::value;
#endif

    //--------------------------------------------------------------------------------------------
    template <typename T, typename... Args>
    struct is_trivially_constructible : bool_type<__is_trivially_constructible(T, Args...)> {
    };
#if defined(BQ_CPP_14)
    template <typename T, typename... Args>
    constexpr bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;
#endif

    template <typename T>
    struct is_trivially_copy_constructible : bool_type<__is_constructible(T, typename add_lvalue_reference<const T>::type)
                                                 && __is_trivially_constructible(T, typename add_lvalue_reference<const T>::type)> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<T>::value;
#endif

    template <typename T>
    struct is_trivially_move_constructible : bool_type<__is_constructible(T, typename add_rvalue_reference<T>::type)
                                                 && __is_trivially_constructible(T, typename add_rvalue_reference<T>::type)> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_trivially_move_constructible_v = is_trivially_move_constructible<T>::value;
#endif

#ifdef BQ_GCC
    template <typename T>
    struct ___gcc_is_trivially_destructible_helper {
        static constexpr bool value = __has_trivial_destructor(T); // in GCC, __has_trivial_destructor is not visible in template parameter
    };
    template <typename T>
    struct is_trivially_destructible : bool_type<___gcc_is_trivially_destructible_helper<T>::value> {
    };
#else
    template <typename T>
    struct is_trivially_destructible : bool_type<__is_trivially_destructible(T)> {
    };
#endif
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;
#endif

    template <typename T>
    struct is_trivially_copy_assignable : bool_type<__is_trivially_assignable(typename add_lvalue_reference<T>::type, typename add_lvalue_reference<const T>::type)> {
    };
    template <typename T>
    struct is_trivially_move_assignable : bool_type<__is_trivially_assignable(typename add_lvalue_reference<T>::type, typename add_rvalue_reference<T>::type)> {
    };
#if defined(BQ_CPP_14)
    template <typename T>
    constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<T>::value;
#endif

    template <typename T>
    bq_forceinline T* launder (T* p) noexcept {
#if defined(BQ_MSVC)
#if defined(BQ_CPP_17)
        return __builtin_launder(p);
#else
        return p;
#endif
#elif defined(BQ_CLANG)
        return __builtin_launder(p);
#elif defined(BQ_GCC)
        if (__has_builtin(__builtin_launder)) {
            return __builtin_launder(p);
        }
        T* result = p;
        __asm__ __volatile__("" : "+r"(result) : : "memory");
        return result;
#else
        T* result = p;
        __asm__ __volatile__("" : "+r"(result) : : "memory");
        return result;
#endif

    }
}
