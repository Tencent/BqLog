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
#include "bq_common/types/type_traits.h"

// we must supply a custom placement new, because we can compile without libc++.
// but in order to avoid pollute users code,
// we don't include <new> or overload default operator new.
namespace bq {
    enum class enum_new_dummy {
        dummy
    };
}
inline void* operator new(size_t, void* p, bq::enum_new_dummy) noexcept { return p; }
inline void* operator new[](size_t, void* p, bq::enum_new_dummy) noexcept { return p; }
inline void operator delete(void*, void*, bq::enum_new_dummy) noexcept { }
inline void operator delete[](void*, void*, bq::enum_new_dummy) noexcept { }

#ifdef BQ_VISUAL_STUDIO
#define EBCO __declspec(empty_bases) // in some MSVC compilers, EBCO(empty base class optimization) is not enabled by default
#else
#define EBCO
#endif

namespace bq {
    template <typename T>
    inline T& min_ref(T& left, T& right)
    {
        return left < right ? left : right;
    }

    template <typename T>
    inline T& max_ref(T& left, T& right)
    {
        return left < right ? right : left;
    }

    template <typename T>
    inline T min_value(const T& left, const T& right)
    {
        return left < right ? left : right;
    }

    template <typename T>
    inline T max_value(const T& left, const T& right)
    {
        return left < right ? right : left;
    }

    template <bool COND, typename T1, typename T2>
    struct condition_type {
        typedef T1 type;
    };

    template <typename T1, typename T2>
    struct condition_type<false, T1, T2> {
        typedef T2 type;
    };
    template <bool COND, typename T1, typename T2>
    using condition_type_t = typename condition_type<COND, T1, T2>::type;

    template <bool COND, typename T, T TRUE_VALUE, T FALSE_VALUE>
    struct condition_value {
        static constexpr T value = TRUE_VALUE;
    };
    template <typename T, T TRUE_VALUE, T FALSE_VALUE>
    struct condition_value<false, T, TRUE_VALUE, FALSE_VALUE> {
        static constexpr T value = FALSE_VALUE;
    };

#if defined(BQ_CPP_14)
    template <bool COND, typename T, T TRUE_VALUE, T FALSE_VALUE>
    constexpr T condition_value_v = condition_value<COND, T, TRUE_VALUE, FALSE_VALUE>::value;
#endif

    using size_t_to_int_t = bq::condition_type<sizeof(size_t) == sizeof(int32_t), int32_t, int64_t>::type;
    using size_t_to_uint_t = bq::condition_type<sizeof(size_t) == sizeof(uint32_t), uint32_t, uint64_t>::type;

    inline constexpr size_t align_4(size_t n)
    {
        return n == 0 ? n : (((n - 1) & (~((size_t)4 - 1))) + 4);
    }

    inline constexpr size_t align_8(size_t n)
    {
        return n == 0 ? n : (((n - 1) & (~((size_t)8 - 1))) + 8);
    }

    template <typename T>
    struct integer_type_hash_calculator {
        typedef uint64_t hash_value_type;
        static inline hash_value_type hash_code(const T& value)
        {
            return static_cast<hash_value_type>(value);
        }
    };

    template <typename T>
    struct decimal_type_hash_calculator {
        typedef uint64_t hash_value_type;
        union double_uint64_union_ {
        public:
            double d_value;
            uint64_t i_value;
        };
        static inline hash_value_type hash_code(const T& value)
        {
            static_assert(sizeof(hash_value_type) == sizeof(double), "wrong decimal type hash_code");
            static_assert(sizeof(double_uint64_union_) == sizeof(double), "wrong double_uint64_union_ size");
            double_uint64_union_ value_tmp;
            value_tmp.d_value = (double)(value);
            return value_tmp.i_value;
        }
    };

    template <typename T, typename hash_value_type>
    inline typename bq::enable_if<(bq::is_same<T, float>::value || bq::is_same<T, double>::value), hash_value_type>::type
    __inner_hash_calculate(const T& value)
    {
        return decimal_type_hash_calculator<T>::hash_code(value);
    }

    template <typename T, typename hash_value_type>
    inline typename bq::enable_if<!(bq::is_same<T, float>::value || bq::is_same<T, double>::value), hash_value_type>::type
    __inner_hash_calculate(const T& value)
    {
        return integer_type_hash_calculator<T>::hash_code(value);
    }

    template <typename T>
    struct number_type_hash_calculator {
        typedef uint64_t hash_value_type;
        static inline hash_value_type hash_code(const T& value)
        {
            return __inner_hash_calculate<T, hash_value_type>(value);
        }
    };

    template <typename T>
    struct pointer_type_hash_calculator {
        typedef uint64_t hash_value_type;
        static inline hash_value_type hash_code(const T& value)
        {
            static_assert(bq::is_pointer<T>::value || bq::is_null_pointer<T>::value, "wrong pointer type hash_code");
            uintptr_t ptr_value = reinterpret_cast<uintptr_t>(value);
            hash_value_type final_value = static_cast<hash_value_type>(ptr_value);
            return final_value;
        }
    };

    template <typename T>
    struct generic_type_hash_calculator {
        typedef uint64_t hash_value_type;
        static inline hash_value_type hash_code(const T& value)
        {
            return static_cast<hash_value_type>(value.hash_code());
        }
    };

    template <typename T>
    struct object_constructor {
    public:
        template <typename... Args>
        static inline void construct(T* _ptr, Args&&... _args)
        {
            new (_ptr, bq::enum_new_dummy::dummy) T(bq::forward<Args>(_args)...);
        }
    };

    template <typename T>
    struct object_destructor {
    private:
        struct trivial_destructor_type { };
        struct non_trivial_destructor_type { };

        template <typename U, typename destructor_type>
        struct destructor_impl;

        template <typename U>
        struct destructor_impl<U, trivial_destructor_type> {
            static inline void destruct(U*)
            {
            }
        };

        template <typename U>
        struct destructor_impl<U, non_trivial_destructor_type> {
            static inline void destruct(U* _ptr)
            {
                if (_ptr) {
                    _ptr->~T();
                }
            }
        };

    public:
        static inline void destruct(T* _ptr)
        {
            using destructor_type = typename bq::condition_type<bq::is_trivially_destructible<T>::value,
                trivial_destructor_type,
                non_trivial_destructor_type>::type;

            destructor_impl<T, destructor_type>::destruct(_ptr);
        }
    };

    template <typename T>
    struct object_array_destructor {
    private:
        struct trivial_destructor_type { };
        struct non_trivial_destructor_type { };

        template <typename U, typename destructor_type>
        struct destructor_impl;

        template <typename U>
        struct destructor_impl<U, trivial_destructor_type> {
            static inline void destruct(U*, size_t)
            {
            }
        };

        template <typename U>
        struct destructor_impl<U, non_trivial_destructor_type> {
            static inline void destruct(U* _ptr, size_t _count)
            {
                if (_ptr) {
                    for (size_t i = 0; i < _count; ++i) {
                        object_destructor<T>::destruct(_ptr + i);
                    }
                }
            }
        };

    public:
        static inline void destruct(T* _ptr, size_t count)
        {
            using destructor_type = typename bq::condition_type<bq::is_trivially_destructible<T>::value,
                trivial_destructor_type,
                non_trivial_destructor_type>::type;

            destructor_impl<T, destructor_type>::destruct(_ptr, count);
        }
    };

    template <typename T>
    inline T roundup_pow_of_two(T x)
    {
        if ((x & (x - 1)) == 0) {
            return x;
        }
        T result = 1;
        while (x > 0) {
            x = x >> 1;
            result = result << 1;
        }
        return result;
    }

    /***************************tuple begin***********************/
    // bq::tuple support EBCO
    template <typename... Types>
    class tuple;

    template <size_t Index, typename Tuple>
    struct tuple_element;

    template <size_t Index>
    struct tuple_element<Index, tuple<>> {
        static_assert(always_false<tuple_element<Index, tuple<>>>::value, "Tuple Index Out of Bounds");
    };

    template <typename ThisType, typename... Rest>
    struct tuple_element<0, tuple<ThisType, Rest...>> {
        using type = ThisType;
        using TupleType = tuple<ThisType, Rest...>;
    };

    template <size_t Index, typename _This, typename... Rest>
    struct tuple_element<Index, tuple<_This, Rest...>>
        : tuple_element<Index - 1, tuple<Rest...>> {
    };

    template <size_t Index, typename Tuple>
    using tuple_element_t = typename tuple_element<Index, Tuple>::type;

    template <size_t Index, typename Ele, bool IsEmpty>
    struct EBCO __tuple_element_value_impl {
    private:
        Ele value_;

    public:
        inline __tuple_element_value_impl() { }
        template <typename Ele_Input>
        inline __tuple_element_value_impl(Ele_Input&& input_value)
            : value_(bq::forward<Ele_Input>(input_value))
        {
        }
        inline Ele& get() { return value_; }
        inline const Ele& get() const { return value_; }
    };

    template <size_t Index, typename Ele>
    struct EBCO __tuple_element_value_impl<Index, Ele, true> : private Ele {
        inline __tuple_element_value_impl() { }
        template <typename Ele_Input>
        inline __tuple_element_value_impl(Ele_Input&& input_value) { (void)input_value; }
        inline Ele& get() { return static_cast<Ele&>(*this); }
        inline const Ele& get() const { return static_cast<const Ele&>(*this); }
    };

    template <size_t Index, typename Ele>
    struct EBCO __tuple_element_value : public __tuple_element_value_impl<Index, Ele, __is_empty(Ele)> {
        using Super_Type = __tuple_element_value_impl<Index, Ele, __is_empty(Ele)>;
        inline __tuple_element_value() { }
        template <typename Ele_Input>
        inline __tuple_element_value(Ele_Input&& input_value)
            : Super_Type(bq::forward<Ele_Input>(input_value))
        {
        }
    };

    template <>
    class tuple<> {
    };

    template <typename ThisType, typename... Rest>
    class EBCO tuple<ThisType, Rest...> : private __tuple_element_value<sizeof...(Rest), ThisType>, private tuple<Rest...> {
    public:
        inline tuple() noexcept { }

        template <typename ThisType_Input, typename... Rest_Input>
        inline tuple(ThisType_Input&& this_input, Rest_Input&&... rest_input) noexcept
            : __tuple_element_value<sizeof...(Rest), ThisType>(bq::forward<ThisType_Input>(this_input))
            , tuple<Rest...>(bq::forward<Rest_Input>(rest_input)...)
        {
        }

        template <size_t _Index, class... _Types>
        friend constexpr tuple_element_t<_Index, tuple<_Types...>>& get(tuple<_Types...>& _Tuple) noexcept;

        template <size_t _Index, class... _Types>
        friend constexpr const tuple_element_t<_Index, tuple<_Types...>>& get(const tuple<_Types...>& _Tuple) noexcept;

        template <size_t _Index, class... _Types>
        friend constexpr tuple_element_t<_Index, tuple<_Types...>>&& get(tuple<_Types...>&& _Tuple) noexcept;

        template <size_t _Index, class... _Types>
        friend constexpr const tuple_element_t<_Index, tuple<_Types...>>&& get(const tuple<_Types...>&& _Tuple) noexcept;

        template <class _Ty, class... _Types>
        friend constexpr _Ty& get(tuple<_Types...>& _Tuple) noexcept;

        template <class _Ty, class... _Types>
        friend constexpr const _Ty& get(const tuple<_Types...>& _Tuple) noexcept;

        template <class _Ty, class... _Types>
        friend constexpr _Ty&& get(tuple<_Types...>&& _Tuple) noexcept;

        template <class _Ty, class... _Types>
        friend constexpr const _Ty&& get(const tuple<_Types...>&& _Tuple) noexcept;
    };

    template <size_t Index, typename... Types>
    constexpr tuple_element_t<Index, tuple<Types...>>& get(tuple<Types...>& input_tuple) noexcept
    {
        using ValueType = tuple_element_t<Index, tuple<Types...>>;
        using ElementValueType = __tuple_element_value<sizeof...(Types) - Index - 1, ValueType>;
        return static_cast<ElementValueType&>(input_tuple).get();
    }

    template <size_t Index, typename... Types>
    constexpr const tuple_element_t<Index, tuple<Types...>>& get(const tuple<Types...>& input_tuple) noexcept
    {
        using ValueType = tuple_element_t<Index, tuple<Types...>>;
        using ElementValueType = __tuple_element_value<sizeof...(Types) - Index - 1, ValueType>;
        return static_cast<const ElementValueType&>(input_tuple).get();
    }

    template <size_t Index, typename... Types>
    constexpr tuple_element_t<Index, tuple<Types...>>&& get(tuple<Types...>&& input_tuple) noexcept
    {
        using ValueType = tuple_element_t<Index, tuple<Types...>>;
        using ElementValueType = __tuple_element_value<sizeof...(Types) - Index - 1, ValueType>;
        return static_cast<ValueType&&>(static_cast<ElementValueType&>(input_tuple).get());
    }

    template <size_t Index, typename... Types>
    constexpr const tuple_element_t<Index, tuple<Types...>>&& get(const tuple<Types...>&& input_tuple) noexcept
    {
        using ValueType = tuple_element_t<Index, tuple<Types...>>;
        using ElementValueType = __tuple_element_value<sizeof...(Types) - Index - 1, ValueType>;
        return static_cast<const ValueType&&>(static_cast<const ElementValueType&>(input_tuple).get());
    }

    template <typename Tuple>
    struct tuple_size {
        static constexpr size_t value = 0;
    };

    template <typename... Types>
    struct tuple_size<tuple<Types...>> {
        static constexpr size_t value = sizeof...(Types);
    };

    template <typename... Types>
    inline tuple<typename bq::decay<Types>::type...> make_tuple(Types&&... args)
    {
        using tuple_type = tuple<typename bq::decay<Types>::type...>;
        return tuple_type(bq::forward<Types>(args)...);
    }
    /***************************tuple end*************************/

    template <typename FuncType, size_t Index>
    struct function_argument_type;
    template <typename Ret, typename... Args, size_t Index>
    struct function_argument_type<Ret (*)(Args...), Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
    template <typename Ret, typename ClassType, typename... Args, size_t Index>
    struct function_argument_type<Ret (ClassType::*)(Args...), Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
    template <typename Ret, typename ClassType, typename... Args, size_t Index>
    struct function_argument_type<Ret (ClassType::*)(Args...) const, Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
#if defined(BQ_CPP_17)
    template <typename Ret, typename... Args, size_t Index>
    struct function_argument_type<Ret (*)(Args...) noexcept, Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
    template <typename Ret, typename ClassType, typename... Args, size_t Index>
    struct function_argument_type<Ret (ClassType::*)(Args...) noexcept, Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
    template <typename Ret, typename ClassType, typename... Args, size_t Index>
    struct function_argument_type<Ret (ClassType::*)(Args...) const noexcept, Index> {
        using type = tuple_element_t<Index, tuple<Args...>>;
    };
#endif
    template <typename FuncType, size_t Index>
    using function_argument_type_t = typename function_argument_type<FuncType, Index>::type;

    template <typename FuncType>
    struct function_return_type;
    template <typename Ret, typename... Args>
    struct function_return_type<Ret (*)(Args...)> {
        using type = Ret;
    };
    template <typename Ret, typename ClassType, typename... Args>
    struct function_return_type<Ret (ClassType::*)(Args...)> {
        using type = Ret;
    };
    template <typename Ret, typename ClassType, typename... Args>
    struct function_return_type<Ret (ClassType::*)(Args...) const> {
        using type = Ret;
    };
#if defined(BQ_CPP_17)
    template <typename Ret, typename ClassType, typename... Args>
    struct function_return_type<Ret (ClassType::*)(Args...) noexcept> {
        using type = Ret;
    };
    template <typename Ret, typename ClassType, typename... Args>
    struct function_return_type<Ret (ClassType::*)(Args...) const noexcept> {
        using type = Ret;
    };
#endif
    template <typename FuncType>
    using function_return_type_t = typename function_return_type<FuncType>::type;

    template <typename T>
    bq_forceinline T* launder(T* p) noexcept
    {
#if defined(BQ_MSVC)
#if defined(BQ_CPP_17)
        return __builtin_launder(p);
#else
        T* result = p;
        _ReadWriteBarrier();
        return result;
#endif
#elif BQ_GCC_CLANG_BUILTIN(__builtin_launder)
        return __builtin_launder(p);
#elif defined(BQ_GCC)
        T* result = p;
        __asm__ __volatile__("" : "+r"(result) : : "memory");
        return result;
#else
        T* result = p;
        __asm__ __volatile__("" : "+r"(result) : : "memory");
        return result;
#endif
    }

    // Template function used to isolate the casting operation for better code optimization and maintainability.
    // We avoid performing reinterpret_cast directly in the macro to allow the compiler to potentially inline and optimize
    // the code more effectively, rather than embedding the cast in a less predictable macro expansion.
    template <typename TO>
    bq_forceinline TO& __bq_macro_force_cast_ignore_alignment_warning(const char* from)
    {
        return *reinterpret_cast<TO*>(const_cast<char*>(from));
    }
}

// Macro designed to generate high-performance code by accessing a variable Var through a forced cast.
// CAUTION: Use carefully! This bypasses alignment checks for `speed ensure` Var is properly aligned for its type,
// as misalignment can cause undefined behavior. No alignment verification is performed here.
#define BQ_PACK_ACCESS_BY_TYPE(Var, Type) bq::__bq_macro_force_cast_ignore_alignment_warning<Type>((const char*)&Var)
#define BQ_PACK_ACCESS(Var) bq::__bq_macro_force_cast_ignore_alignment_warning<decltype(Var)>((const char*)&Var)