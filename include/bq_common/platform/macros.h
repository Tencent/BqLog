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

#if defined(_M_ARM) || defined(__arm__) || defined(__thumb__)
#define BQ_ARM 1
#define BQ_ARM_V7 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define BQ_ARM 1
#define BQ_ARM_V8 1
#elif defined(_M_IX86) || defined(__i386__) || defined(_X86_)
#define BQ_X86 1
#define BQ_X86_32 1
#elif defined(_M_X64) || defined(__amd64__) || defined(__x86_64__)
#define BQ_X86 1
#define BQ_X86_64 1
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
#define BQ_TLS_STRUCT_DECLARE_BEGIN(STRUCT_NAME)
#define BQ_TLS_STRUCT_FIELD(STRUCT_NAME, TYPE, NAME, DEFAULT_VALUE) BQ_TLS TYPE STRUCT_NAME##_##NAME = DEFAULT_VALUE;
#define BQ_TLS_STRUCT_DECLARE_END(STRUCT_NAME)
#define BQ_TLS_FIELD_REF(STRUCT_NAME, FIELD_NAME) (STRUCT_NAME##_##FIELD_NAME)
#else
#define BQ_STDCALL
#define BQ_TLS __thread
#define BQ_TLS_STRUCT_DECLARE_BEGIN(STRUCT_NAME) BQ_TLS struct {
#define BQ_TLS_STRUCT_FIELD(STRUCT_NAME, TYPE, NAME, DEFAULT_VALUE) TYPE NAME = DEFAULT_VALUE;
#define BQ_TLS_STRUCT_DECLARE_END(STRUCT_NAME) }STRUCT_NAME;
#define BQ_TLS_FIELD_REF(STRUCT_NAME, FIELD_NAME) (STRUCT_NAME.FIELD_NAME)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define BQ_STRUCT_PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ ; __pragma(pack(pop))
#else
#define BQ_STRUCT_PACK(__Declaration__) __Declaration__  __attribute__((__packed__));
#endif

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
