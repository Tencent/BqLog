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
#include "bq_common/platform/build_type.h"
#include <stddef.h>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define BQ_WIN 1
#ifdef _WIN64
#define BQ_WIN64 1
#else
#define BQ_WIN64 0
#endif
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#define BQ_POSIX 1
#define BQ_APPLE 1
#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR != 0
#define BQ_IOS 1
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE != 0
#define BQ_IOS 1
#elif defined(TARGET_OS_MAC) && TARGET_OS_MAC != 0
#define BQ_MAC 1
#elif defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST != 0
// Mac's Catalyst (ports iOS API into Mac, like UIKit).
#else
#endif
#elif defined(__OHOS__)
#define BQ_OHOS 1
#define BQ_POSIX 1
#ifndef BQ_UNIT_TEST
#define BQ_NAPI 1
#endif
#elif defined(__ANDROID__)
#define BQ_ANDROID 1
#define BQ_POSIX 1
#define BQ_JAVA 1
#ifndef BQ_UNIT_TEST
#if defined(__has_include)
#if !__has_include(<vector>)
#define BQ_NO_LIBCPP 1
#endif
#endif
#endif
#elif defined(__ORBIS__)
#define BQ_PS 1
#define BQ_POSIX 1
#elif defined(__Prospero__)
#define BQ_PS 1
#define BQ_POSIX 1
#elif defined(__linux__)
#define BQ_LINUX 1
#define BQ_POSIX 1
#elif defined(__unix__)
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
#define BQ_API BQ_API_EXPORT
#endif

#ifdef _MSC_VER
#define BQ_STDCALL __stdcall
#define BQ_TLS __declspec(thread)
#else
#define BQ_STDCALL
#define BQ_TLS __thread
#endif

// thread_local has use-after-free issue on MinGW GCC
// use BQ_TLS_NON_POD instead of thread_local can avoid crash when thread exit.
namespace bq {
    template <size_t ID, typename T>
    struct _bq_non_pod_holder_type { };
}
#define BQ_TLS_DEFINE(Type, Name, ID)                                            \
    BQ_TLS Type* ____BQ_TLS_##Name##_ptr;                                        \
    BQ_TLS bool ____BQ_TSL_##Name##_recycled;                                    \
    template <>                                                                  \
    struct _bq_non_pod_holder_type<ID, Type> {                                   \
        _bq_non_pod_holder_type()                                                \
        {                                                                        \
        }                                                                        \
        ~_bq_non_pod_holder_type()                                               \
        {                                                                        \
            if (____BQ_TLS_##Name##_ptr) {                                       \
                delete ____BQ_TLS_##Name##_ptr;                                  \
                ____BQ_TLS_##Name##_ptr = nullptr;                               \
                ____BQ_TSL_##Name##_recycled = true;                             \
            }                                                                    \
        }                                                                        \
        bq_forceinline operator bool() { return !____BQ_TSL_##Name##_recycled; } \
        bq_forceinline Type& get()                                               \
        {                                                                        \
            if (!____BQ_TLS_##Name##_ptr) {                                      \
                ____BQ_TLS_##Name##_ptr = new Type();                            \
            }                                                                    \
            return *____BQ_TLS_##Name##_ptr;                                     \
        }                                                                        \
    };                                                                           \
    thread_local _bq_non_pod_holder_type<ID, Type> Name;
#define BQ_TLS_NON_POD(Type, Name) BQ_TLS_DEFINE(Type, Name, __COUNTER__)

#if defined(_MSC_VER) && !defined(__clang__)
#define BQ_PACK_BEGIN __pragma(pack(push, 1))
#define BQ_PACK_END \
    ;               \
    __pragma(pack(pop))
#define BQ_ANONYMOUS_STRUCT_PACK(DECLARATION, NAME)  \
    __pragma(pack(push, 1)) struct DECLARATION NAME; \
    __pragma(pack(pop))
#else
#define BQ_PACK_BEGIN
#define BQ_PACK_END __attribute__((__packed__));
#define BQ_ANONYMOUS_STRUCT_PACK(DECLARATION, NAME) struct DECLARATION __attribute__((__packed__)) NAME
#endif
// Template function used to isolate the casting operation for better code optimization and maintainability.
// We avoid performing reinterpret_cast directly in the macro to allow the compiler to potentially inline and optimize
// the code more effectively, rather than embedding the cast in a less predictable macro expansion.
template <typename TO>
bq_forceinline TO& __bq_macro_force_cast_ignore_alignment_warning(const char* from)
{
    return *reinterpret_cast<TO*>(const_cast<char*>(from));
}
// Macro designed to generate high-performance code by accessing a variable Var through a forced cast.
// CAUTION: Use carefully! This bypasses alignment checks for `speed ensure` Var is properly aligned for its type,
// as misalignment can cause undefined behavior. No alignment verification is performed here.
#define BQ_PACK_ACCESS_BY_TYPE(Var, Type) __bq_macro_force_cast_ignore_alignment_warning<Type>((const char*)&Var)
#define BQ_PACK_ACCESS(Var) __bq_macro_force_cast_ignore_alignment_warning<decltype(Var)>((const char*)&Var)

#if defined(__cplusplus) && (__cplusplus >= 201402L)
#define BQ_CPP_14 1
#define BQ_FUNC_RETURN_CONSTEXPR constexpr
#else
#define BQ_FUNC_RETURN_CONSTEXPR
#endif

#if defined(__cplusplus) && (__cplusplus >= 201703L)
#define BQ_CPP_17 1
#define BQ_CONSTEXPR_IF if constexpr 
#else
#define BQ_CONSTEXPR_IF if
#endif

#if defined(__cplusplus) && (__cplusplus >= 202002L)
#define BQ_CPP_20 1
#endif

#if defined(__cplusplus) && (__cplusplus >= 202302L)
#define BQ_CPP_23 1
#endif

#if defined(__cpp_aligned_new)
#define BQ_ALIGNAS_NEW 1
#endif

#if defined(BQ_GCC) || defined(BQ_CLANG)
#define BQ_SUPPRESS_NULL_DEREF_BEGIN() \
    _Pragma("GCC diagnostic push")     \
        _Pragma("GCC diagnostic ignored \"-Wnull-dereference\"")
#define BQ_SUPPRESS_NULL_DEREF_END() \
    _Pragma("GCC diagnostic pop")
#elif defined(BQ_MSVC)
#define BQ_SUPPRESS_NULL_DEREF_BEGIN()                                          \
    __pragma(warning(push))                                                     \
        __pragma(warning(disable : 6011)) /* C6011: Dereference Null Pointer */ \
        __pragma(warning(disable : 6387)) /* C6387: 'Maybe NULL' */
#define BQ_SUPPRESS_NULL_DEREF_END() \
    __pragma(warning(pop))
#else
#define BQ_SUPPRESS_NULL_DEREF_BEGIN()
#define BQ_SUPPRESS_NULL_DEREF_END()
#endif

#if (defined(BQ_CLANG) || defined(BQ_GCC)) && defined(__has_builtin)
#define BQ_GCC_CLANG_BUILTIN(X) __has_builtin(X)
#else
#define BQ_GCC_CLANG_BUILTIN(X) 0
#endif

#if defined(BQ_CPP_20)
#define BQ_LIKELY_IF(expr) if (expr) [[likely]]
#define BQ_UNLIKELY_IF(expr) if (expr) [[unlikely]]
#define BQ_LIKELY_DEFINED
#elif defined(BQ_GCC_CLANG_BUILTIN)
#if BQ_GCC_CLANG_BUILTIN(__builtin_expect)
#define BQ_LIKELY_IF(expr) if (__builtin_expect(!!(expr), 1))
#define BQ_UNLIKELY_IF(expr) if (__builtin_expect(!!(expr), 0))
#define BQ_LIKELY_DEFINED
#endif
#endif

#ifndef BQ_LIKELY_DEFINED
#define BQ_LIKELY_IF(expr) if (expr)
#define BQ_UNLIKELY_IF(expr) if (expr)
#endif

#if defined(UE_BUILD_DEBUG) || defined(UE_BUILD_DEVELOPMENT) || defined(UE_BUILD_TEST) || defined(UE_BUILD_SHIPPING) || defined(UE_GAME) || defined(UE_EDITOR) || defined(UE_BUILD_SHIPPING_WITH_EDITOR) || defined(UE_BUILD_DOCS)
#define BQ_IN_UNREAL 1
#else
#define BQ_IN_UNREAL 0
#endif


#if defined(BQ_ANDROID) || defined(BQ_IOS) || defined(BQ_OHOS)
#define BQ_MOBILE_PLATFORM
#endif

#if defined(BQ_GCC) || defined(BQ_CLANG)
#define BQ_RESTRICT __restrict__
#elif defined(BQ_MSVC)
#define BQ_RESTRICT __restrict
#else
#define BQ_RESTRICT
#endif

// Target attribute for GCC/Clang to enable specific instruction sets for specific functions.
#if (defined(BQ_CLANG) || defined(BQ_GCC))
    #if defined(BQ_ARM)
        #define BQ_HW_CRC_TARGET __attribute__((target("crc")))
        #define BQ_HW_SIMD_TARGET
    #elif defined(BQ_X86)
        #define BQ_HW_CRC_TARGET __attribute__((target("sse4.2")))
        #define BQ_HW_SIMD_TARGET __attribute__((target("avx2")))
    #else
        #define BQ_HW_CRC_TARGET
        #define BQ_HW_SIMD_TARGET
    #endif
    #define BQ_CRC_HW_INLINE inline
    #define BQ_SIMD_HW_INLINE inline
#else
    #define BQ_HW_CRC_TARGET
    #define BQ_HW_SIMD_TARGET
    #define BQ_CRC_HW_INLINE bq_forceinline
    #define BQ_SIMD_HW_INLINE bq_forceinline
#endif