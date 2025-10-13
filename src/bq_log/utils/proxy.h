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
 * \file proxy.h
 *
 * This class is intended to replace the functionality of polymorphisms.
 * It is an experimental toy, and its performance may not necessarily exceed
 * that of a virtual-table lookup because it depends on compiler optimizations,
 * and it involves at least one additional function-pointer call.
 *
 * Usage example:
 * \code{.cpp}
 * BQ_FACADE_DEC(AAA_Facade, AAA)  // Declares a facade named AAA_Facade, which represents a function called AAA
 * BQ_FACADE_DEC(BBB_Facade, BBB)  // Declares a facade named BBB_Facade, which represents a function called BBB
 *
 * // Generates a proxy class that has a member function int32_t AAA(int32_t, int32_t) const
 * // and another called int32_t BBB(int32_t) const.
 * struct PROXY
 *   : public bq::proxy<
 *       bq::facade<AAA_Facade, int32_t(int32_t, int32_t) const>,
 *       bq::facade<BBB_Facade, int32_t(int32_t) const>
 *     >
 * {
 * };
 *
 * // Then we create two actual classes that implement these functions,
 * // much like two subclasses of a base class.
 * struct TestClassA
 * {
 *   int32_t a;
 *   int32_t b;
 *   TestClassA(int32_t a1, int32_t b1)
 *   {
 *     a = a1;
 *     b = b1;
 *   }
 *   int32_t AAA(int32_t c, int32_t d) const
 *   {
 *     return a + b + c + d;
 *   }
 *   int32_t BBB(int32_t c) const
 *   {
 *     return 2 * (a + b + c + c);
 *   }
 * };
 *
 * struct TestClassB
 * {
 *   int32_t AAA(int32_t c, int32_t d) const
 *   {
 *     return c + d;
 *   }
 *   int32_t BBB(int32_t c) const
 *   {
 *     return 2 * (c + c);
 *   }
 * };
 *
 * // Then you can do whatever you like:
 * PROXY pr1 = bq::make_proxy<PROXY, TestClassA>(16, 17);
 * PROXY pr2 = bq::make_proxy<PROXY, TestClassB>();
 * PROXY pr3 = bq::make_proxy<PROXY, TestClassA>(22, 22);
 * PROXY pr4 = bq::make_proxy<PROXY, TestClassB>();
 *
 * pr2 = pr1;
 * pr3 = pr1;
 * pr3 = bq::move(pr4);
 *
 * int32_t result = pr1.BBB(30) + pr4.AAA(2, 2);
 * int32_t result2 = pr2.AAA(66, 22) + pr3.AAA(66, 22);
 * \endcode
 *
 * \author pippocao
 * \date 2024/12/24
 *
 */

#include "bq_common/bq_common_public_include.h"

namespace bq {
    struct proxy_base {
        template <typename Proxy, typename Class, typename... Args>
        friend Proxy make_proxy(Args&&...);
        friend const void* get_ptr_from_proxy_base(const proxy_base*);
        friend void* get_ptr_from_proxy_base(proxy_base*);

    private:
        void* obj_ptr_;
        void (*destruct_func_ptr_)(proxy_base&);
        void (*copy_constructor_func_ptr_)(proxy_base&, const proxy_base&);
        void (*copy_assignment_func_ptr_)(proxy_base&, const proxy_base&);

    private:
        template <typename Class>
        static void destruct(proxy_base& proxy)
        {
            if (proxy.obj_ptr_) {
                delete (Class*)proxy.obj_ptr_;
                proxy.obj_ptr_ = nullptr;
                proxy.destruct_func_ptr_ = nullptr;
                proxy.copy_constructor_func_ptr_ = nullptr;
                proxy.copy_assignment_func_ptr_ = nullptr;
            }
        }
        template <typename Class>
        static void copy_constructor(proxy_base& tar, const proxy_base& src)
        {
            tar.obj_ptr_ = new Class(*((const Class*)src.obj_ptr_));
            tar.destruct_func_ptr_ = src.destruct_func_ptr_;
            tar.copy_constructor_func_ptr_ = src.copy_constructor_func_ptr_;
            tar.copy_assignment_func_ptr_ = src.copy_assignment_func_ptr_;
        }
        template <typename Class>
        static void copy_assignment(proxy_base& tar, const proxy_base& src)
        {
            *((Class*)tar.obj_ptr_) = *((const Class*)src.obj_ptr_);
        }
        template <typename Class, typename... Args>
        static bq_forceinline void construct(proxy_base& proxy, Args&&... args)
        {
            proxy.obj_ptr_ = new Class(args...);
            proxy.destruct_func_ptr_ = &proxy_base::destruct<Class>;
            proxy.copy_constructor_func_ptr_ = &proxy_base::copy_constructor<Class>;
            proxy.copy_assignment_func_ptr_ = &proxy_base::copy_assignment<Class>;
        }

    public:
        proxy_base()
            : obj_ptr_(nullptr)
            , destruct_func_ptr_(nullptr)
            , copy_constructor_func_ptr_(nullptr)
            , copy_assignment_func_ptr_(nullptr)
        {
        }
        ~proxy_base()
        {
            if (destruct_func_ptr_) {
                destruct_func_ptr_(*this);
            }
        }
        proxy_base(const proxy_base& rhs) noexcept
            : obj_ptr_(nullptr)
            , destruct_func_ptr_(nullptr)
            , copy_constructor_func_ptr_(nullptr)
            , copy_assignment_func_ptr_(nullptr)
        {
            rhs.copy_constructor_func_ptr_(*this, rhs);
        }
        proxy_base(proxy_base&& rhs) noexcept
        {
            obj_ptr_ = rhs.obj_ptr_;
            destruct_func_ptr_ = rhs.destruct_func_ptr_;
            copy_constructor_func_ptr_ = rhs.copy_constructor_func_ptr_;
            copy_assignment_func_ptr_ = rhs.copy_assignment_func_ptr_;
            rhs.obj_ptr_ = nullptr;
            rhs.destruct_func_ptr_ = nullptr;
            rhs.copy_constructor_func_ptr_ = nullptr;
            rhs.copy_assignment_func_ptr_ = nullptr;
        }
        proxy_base& operator=(const proxy_base& rhs)
        {
            if (destruct_func_ptr_ != rhs.destruct_func_ptr_) {
                // different class
                if (destruct_func_ptr_) {
                    destruct_func_ptr_(*this);
                }
                rhs.copy_constructor_func_ptr_(*this, rhs);
                return *this;
            }
            rhs.copy_assignment_func_ptr_(*this, rhs);
            return *this;
        }
        proxy_base& operator=(proxy_base&& rhs) noexcept
        {
            if (destruct_func_ptr_) {
                destruct_func_ptr_(*this);
            }
            obj_ptr_ = rhs.obj_ptr_;
            destruct_func_ptr_ = rhs.destruct_func_ptr_;
            copy_constructor_func_ptr_ = rhs.copy_assignment_func_ptr_;
            copy_assignment_func_ptr_ = rhs.copy_assignment_func_ptr_;
            rhs.obj_ptr_ = nullptr;
            rhs.destruct_func_ptr_ = nullptr;
            rhs.copy_constructor_func_ptr_ = nullptr;
            rhs.copy_assignment_func_ptr_ = nullptr;
            return *this;
        }
    };

    bq_forceinline const void* get_ptr_from_proxy_base(const proxy_base* proxy_ptr)
    {
        return proxy_ptr->obj_ptr_;
    }

    bq_forceinline void* get_ptr_from_proxy_base(proxy_base* proxy_ptr)
    {
        return proxy_ptr->obj_ptr_;
    }

    template <typename GeneratedFacade, typename Function>
    struct facade {
        using generated_facade_type = GeneratedFacade;
        using function_type = Function;
    };

    template <typename... Facades>
    struct proxy_facade_tree;

    template <>
    struct proxy_facade_tree<> : public proxy_base {
    };

    template <typename FirstFacade>
    struct proxy_facade_tree<FirstFacade>
        : public FirstFacade::generated_facade_type ::template facade_impl<proxy_facade_tree<FirstFacade>, typename FirstFacade::function_type>, public proxy_facade_tree<> {
        // ...
    };

    template <typename FirstFacade, typename... Rest>
    struct proxy_facade_tree<FirstFacade, Rest...>
        : public FirstFacade::generated_facade_type ::template facade_impl<proxy_facade_tree<FirstFacade, Rest...>, typename FirstFacade::function_type>, public proxy_facade_tree<Rest...> {
        // ...
    };

    template <typename... Facades>
    struct proxy : public proxy_facade_tree<Facades...> {
    private:
        typedef bq::tuple<Facades...> facade_types;

        template <typename P, typename C, typename... Args>
        friend P make_proxy(Args&&...);

    private:
        template <typename... FacadesInner>
        struct initer;
        template <>
        struct initer<> {
            template <typename Class, typename PROXY>
            static bq_forceinline void do_init(PROXY* this_ptr)
            {
                (void)this_ptr;
            }
        };
        template <typename FirstFacadesInner>
        struct initer<FirstFacadesInner> {
            template <typename Class, typename PROXY>
            static bq_forceinline void do_init(PROXY* this_ptr)
            {
                ((typename FirstFacadesInner::generated_facade_type::template facade_impl<proxy_facade_tree<FirstFacadesInner>, typename FirstFacadesInner::function_type>*)this_ptr)->template init<Class>();
            }
        };
        template <typename FirstFacadesInner, typename... RestFacadesInner>
        struct initer<FirstFacadesInner, RestFacadesInner...>
            : public initer<RestFacadesInner...> {
            template <typename Class, typename PROXY>
            static bq_forceinline void do_init(PROXY* this_ptr)
            {
                ((typename FirstFacadesInner::generated_facade_type::template facade_impl<proxy_facade_tree<FirstFacadesInner, RestFacadesInner...>, typename FirstFacadesInner::function_type>*)this_ptr)->template init<Class>();
                initer<RestFacadesInner...>::template do_init<Class>(this_ptr);
            }
        };

    private:
        template <typename Class, typename Facade>
        void init_facade()
        {
            ((typename Facade::generated_facade_type::template facade_impl<proxy<Facades...>, typename Facade::function_type>*)this)->template init<Class>();
        }

        template <typename Class>
        void init()
        {
            initer<Facades...>::template do_init<Class>(this);
        }
    };

    template <typename Proxy, typename Class, typename... Args>
    Proxy make_proxy(Args&&... args)
    {
        Proxy proxy;
        proxy_base::construct<Class>(proxy, args...);
        proxy.template init<Class>();
        return proxy;
    }
}

#define __BQ_FACADE_CPP17_PART(FUNCTION_NAME)                                                                                     \
    template <typename Class, typename Ret, typename... Args>                                                                     \
    struct EBCO dispatcher<Ret (Class::*)(Args...) noexcept> {                                                                    \
        static Ret dispatch(void* obj, Args... args)                                                                              \
        {                                                                                                                         \
            return ((Class*)obj)->FUNCTION_NAME(args...);                                                                         \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Class, typename Ret, typename... Args>                                                                     \
    struct EBCO dispatcher<Ret (Class::*)(Args...) const noexcept> {                                                              \
        static Ret dispatch(const void* obj, Args... args)                                                                        \
        {                                                                                                                         \
            return ((const Class*)obj)->FUNCTION_NAME(args...);                                                                   \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Proxy, typename Ret, typename... Args>                                                                     \
    struct EBCO facade_impl<Proxy, Ret(Args...) noexcept> {                                                                       \
        template <typename... Facades>                                                                                            \
        friend struct bq::proxy;                                                                                                  \
                                                                                                                                  \
    private:                                                                                                                      \
        Ret (*dispatcher_ptr_)(void*, Args...);                                                                                   \
                                                                                                                                  \
    public:                                                                                                                       \
        template <typename Class>                                                                                                 \
        bq_forceinline void init()                                                                                                \
        {                                                                                                                         \
            dispatcher_ptr_ = &dispatcher<Ret (Class::*)(Args...) noexcept>::dispatch;                                            \
        }                                                                                                                         \
                                                                                                                                  \
    public:                                                                                                                       \
        bq_forceinline Ret FUNCTION_NAME(Args... args) noexcept                                                                   \
        {                                                                                                                         \
            return dispatcher_ptr_(bq::get_ptr_from_proxy_base((bq::proxy_base*)(Proxy*)this), args...);                          \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Proxy, typename Ret, typename... Args>                                                                     \
    struct EBCO facade_impl<Proxy, Ret(Args...) const noexcept> {                                                                 \
        template <typename... Facades>                                                                                            \
        friend struct bq::proxy;                                                                                                  \
                                                                                                                                  \
    private:                                                                                                                      \
        Ret (*dispatcher_ptr_)(const void*, Args...);                                                                             \
                                                                                                                                  \
    public:                                                                                                                       \
        template <typename Class>                                                                                                 \
        bq_forceinline void init()                                                                                                \
        {                                                                                                                         \
            dispatcher_ptr_ = &dispatcher<Ret (Class::*)(Args...) const noexcept>::dispatch;                                      \
        }                                                                                                                         \
                                                                                                                                  \
    public:                                                                                                                       \
        bq_forceinline Ret FUNCTION_NAME(Args... args) const noexcept                                                             \
        {                                                                                                                         \
            return dispatcher_ptr_((const void*)bq::get_ptr_from_proxy_base((const bq::proxy_base*)(const Proxy*)this), args...); \
        }                                                                                                                         \
    };

#define __BQ_FACADE_CPP11_PART(FUNCTION_NAME)                                                                                     \
    template <typename FunctionType>                                                                                              \
    struct EBCO dispatcher {};                                                                                                    \
    template <typename Class, typename Ret, typename... Args>                                                                     \
    struct EBCO dispatcher<Ret (Class::*)(Args...)> {                                                                             \
        static Ret dispatch(void* obj, Args... args)                                                                              \
        {                                                                                                                         \
            return ((Class*)obj)->FUNCTION_NAME(args...);                                                                         \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Class, typename Ret, typename... Args>                                                                     \
    struct EBCO dispatcher<Ret (Class::*)(Args...) const> {                                                                       \
        static Ret dispatch(const void* obj, Args... args)                                                                        \
        {                                                                                                                         \
            return ((const Class*)obj)->FUNCTION_NAME(args...);                                                                   \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Proxy, typename FunctionType>                                                                              \
    struct EBCO facade_impl {};                                                                                                   \
    template <typename Proxy, typename Ret, typename... Args>                                                                     \
    struct EBCO facade_impl<Proxy, Ret(Args...)> {                                                                                \
        template <typename... Facades>                                                                                            \
        friend struct bq::proxy;                                                                                                  \
                                                                                                                                  \
    private:                                                                                                                      \
        Ret (*dispatcher_ptr_)(void*, Args...);                                                                                   \
                                                                                                                                  \
    public:                                                                                                                       \
        template <typename Class>                                                                                                 \
        bq_forceinline void init()                                                                                                \
        {                                                                                                                         \
            dispatcher_ptr_ = &dispatcher<Ret (Class::*)(Args...)>::dispatch;                                                     \
        }                                                                                                                         \
                                                                                                                                  \
    public:                                                                                                                       \
        bq_forceinline Ret FUNCTION_NAME(Args... args)                                                                            \
        {                                                                                                                         \
            return dispatcher_ptr_(bq::get_ptr_from_proxy_base((bq::proxy_base*)(Proxy*)this), args...);                          \
        }                                                                                                                         \
    };                                                                                                                            \
    template <typename Proxy, typename Ret, typename... Args>                                                                     \
    struct EBCO facade_impl<Proxy, Ret(Args...) const> {                                                                          \
        template <typename... Facades>                                                                                            \
        friend struct bq::proxy;                                                                                                  \
                                                                                                                                  \
    private:                                                                                                                      \
        Ret (*dispatcher_ptr_)(const void*, Args...);                                                                             \
                                                                                                                                  \
    public:                                                                                                                       \
        template <typename Class>                                                                                                 \
        bq_forceinline void init()                                                                                                \
        {                                                                                                                         \
            dispatcher_ptr_ = &dispatcher<Ret (Class::*)(Args...) const>::dispatch;                                               \
        }                                                                                                                         \
                                                                                                                                  \
    public:                                                                                                                       \
        bq_forceinline Ret FUNCTION_NAME(Args... args) const                                                                      \
        {                                                                                                                         \
            return dispatcher_ptr_((const void*)bq::get_ptr_from_proxy_base((const bq::proxy_base*)(const Proxy*)this), args...); \
        }                                                                                                                         \
    };

#ifdef BQ_CPP17
#define BQ_FACADE_DEC(FACADE_NAME, FUNCTION_NAME) \
    struct FACADE_NAME {                          \
        __BQ_FACADE_CPP11_PART(FUNCTION_NAME)     \
        __BQ_FACADE_CPP17_PART(FUNCTION_NAME)     \
    };
#else
#define BQ_FACADE_DEC(FACADE_NAME, FUNCTION_NAME) \
    struct FACADE_NAME {                          \
        __BQ_FACADE_CPP11_PART(FUNCTION_NAME)     \
    };
#endif
