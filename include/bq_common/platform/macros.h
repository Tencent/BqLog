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
#include "bq_common/platform/build_type.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define BQ_WIN 1
#ifdef _WIN64
#define BQ_WIN64 1
#else
#define BQ_WIN64 0
#endif
#elif __APPLE__
#include <TargetConditionals.h>
#define BQ_POSIX 1
#define BQ_APPLE 1
#if TARGET_IPHONE_SIMULATOR
#define BQ_IOS 1
#elif TARGET_OS_IPHONE
#define BQ_IOS 1
#elif TARGET_OS_MAC
#define BQ_MAC 1
#elif TARGET_OS_MACCATALYST
// Mac's Catalyst (ports iOS API into Mac, like UIKit).
#else
#endif
#elif __ANDROID__
#define BQ_ANDROID 1
#define BQ_POSIX 1
#define BQ_JAVA 1
#ifndef BQ_UNIT_TEST
#define BQ_NO_LIBCPP 1
#endif
#elif __ORBIS__
#define BQ_PS 1
#define BQ_POSIX 1
#elif __Prospero__
#define BQ_PS 1
#define BQ_POSIX 1
#elif __linux__
#define BQ_LINUX 1
#define BQ_POSIX 1
#elif __unix__
#define BQ_POSIX 1
#define BQ_UNIX 1
#elif defined(_POSIX_VERSION)
#define BQ_POSIX 1
#else

#endif

#ifdef __clang__
#define BQ_CLANG 1
#elif defined(_MSC_VER)
#define BQ_MSVC 1
#elif defined(__GNUC__)
#define BQ_GCC 1
#endif

#if defined(_MSC_VER)
#define BQ_VISUAL_STUDIO 1
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define BQ_ARM 1
#define BQ_ARM_64 1
#define BQ_ARM_V8 1 // ARMv8-A
#elif defined(_M_ARM) || defined(__arm__)
#define BQ_ARM 1
#define BQ_ARM_32 1
#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7__)
#define BQ_ARM_V7 1
#elif defined(__ARM_ARCH_6__)
#define BQ_ARM_V6 1
#else
#define BQ_ARM_UNKNOWN 1 
#endif
#elif defined(_M_IX86) || defined(__i386__)
#define BQ_X86 1
#define BQ_X86_32 1
#elif defined(_M_X64) || defined(__amd64__) || defined(__x86_64__)
#define BQ_X86 1
#define BQ_X86_64 1
#else
#define BQ_UNKNOWN_ARCH 1 
#endif

#ifdef BQ_MSVC
#define bq_forceinline __forceinline
#elif defined(BQ_GCC) || defined(BQ_CLANG)
#define bq_forceinline __inline__ __attribute__((always_inline))
#else
#define bq_forceinline inline
#endif

#if defined(_MSC_VER)
#define BQ_API_EXPORT extern "C" __declspec(dllexport)
#define BQ_API_IMPORT extern "C" __declspec(dllimport)
#else
#define BQ_API_EXPORT extern "C" __attribute__((visibility("default")))
#define BQ_API_IMPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(BQ_DYNAMIC_LIB)
#define BQ_API BQ_API_EXPORT
#elif defined(BQ_DYNAMIC_LIB_IMPORT)
#define BQ_API BQ_API_IMPORT
#else
#define BQ_API
#endif

#ifdef _MSC_VER
#define BQ_STDCALL __stdcall
#define BQ_TLS __declspec(thread)
#else
#define BQ_STDCALL
#define BQ_TLS __thread
#endif

// thread_local has use-after-free issue on MinGW GCC
template<size_t ID, typename T>
struct __bq_non_pod_holder_type { };

#define BQ_TLS_DEFINE(Type, Name, ID) BQ_TLS Type * ____BQ_TLS_##Name##_ptr; \
                                        template<> \
                                        struct __bq_non_pod_holder_type<ID, Type> { \
                                        __bq_non_pod_holder_type() \
                                        { \
                                            if (!____BQ_TLS_##Name##_ptr) { \
                                                ____BQ_TLS_##Name##_ptr = new Type(); \
                                            } \
                                        } \
                                        ~__bq_non_pod_holder_type() \
                                        { \
                                            if (____BQ_TLS_##Name##_ptr) { \
                                                delete ____BQ_TLS_##Name##_ptr; \
                                            ____BQ_TLS_##Name##_ptr = nullptr; \
                                            } \
                                        }                                                                      \
                                        bq_forceinline operator bool() { return ____BQ_TLS_##Name##_ptr; } \
                                        bq_forceinline Type& get() { return *____BQ_TLS_##Name##_ptr; } \
                                    }; \
                                    thread_local __bq_non_pod_holder_type<ID, Type> Name;
#define BQ_TLS_NON_POD(Type, Name) BQ_TLS_DEFINE(Type, Name, __COUNTER__)

#if defined(_MSC_VER) && !defined(__clang__)
#define BQ_PACK_BEGIN __pragma(pack(push, 1))
#define BQ_PACK_END ;__pragma(pack(pop))
#define BQ_ANONYMOUS_STRUCT_PACK(DECLARATION, NAME) __pragma(pack(push, 1)) struct DECLARATION NAME; __pragma(pack(pop))
#else
#define BQ_PACK_BEGIN
#define BQ_PACK_END __attribute__((__packed__));
#define BQ_ANONYMOUS_STRUCT_PACK(DECLARATION, NAME) struct DECLARATION __attribute__((__packed__)) NAME
#endif
// Template function used to isolate the casting operation for better code optimization and maintainability.
// We avoid performing reinterpret_cast directly in the macro to allow the compiler to potentially inline and optimize
// the code more effectively, rather than embedding the cast in a less predictable macro expansion.
template <typename TO>
bq_forceinline TO& __bq_macro_force_cast_ignore_alignment_warning(char* from)
{
    return *reinterpret_cast<TO*>(from);
}
// Macro designed to generate high-performance code by accessing a variable Var through a forced cast.
// CAUTION: Use carefully! This bypasses alignment checks for `speed ensure` Var is properly aligned for its type,
// as misalignment can cause undefined behavior. No alignment verification is performed here.
#define BQ_PACK_ACCESS_BY_TYPE(Var, Type) __bq_macro_force_cast_ignore_alignment_warning<Type>((char*)&Var)
#define BQ_PACK_ACCESS(Var) __bq_macro_force_cast_ignore_alignment_warning<decltype(Var)>((char*)&Var)

#if __cplusplus >= 201402L
#define BQ_CPP_14 1
#define BQ_FUNC_RETURN_CONSTEXPR constexpr
#else
#define BQ_FUNC_RETURN_CONSTEXPR
#endif

#if __cplusplus >= 201703L
#define BQ_CPP_17 1
#endif

#if __cplusplus >= 202002L
#define BQ_CPP_20 1
#endif


#if defined(__cpp_aligned_new)
#define BQ_ALIGNAS_NEW 1
#endif
