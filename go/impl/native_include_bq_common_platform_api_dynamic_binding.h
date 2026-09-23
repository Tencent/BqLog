/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
/*!
 * \file api_dynamic_binding.h
 *
 * Header-only dynamic API binding without STL or libc++ dependencies.
 */

#if !defined(__cplusplus)
#error "BQ late-bound APIs require C++"
#endif

#if !defined(BQ_API_DYNAMIC_LIBRARY_NAME)
#error "BQ_API_DYNAMIC_LIBRARY_NAME must be defined when BQ_LATE_BINDING is enabled"
#endif

#if defined(BQ_WIN)
#if BQ_IN_UNREAL
#include "Windows/AllowWindowsPlatformTypes.h"
#include <WinSock2.h>
#else
#include <WinSock2.h>
#include <windows.h>
#endif
#elif defined(BQ_POSIX)
#include <dlfcn.h>
#else
#error "bq api dynamic binding is not supported on this platform"
#endif

#if defined(BQ_MSVC)
#include <intrin.h>
#endif

#if defined(BQ_GCC) || defined(BQ_CLANG)
#pragma GCC visibility push(hidden)
#endif

namespace bq {
    namespace api_dynamic_binding {
#if defined(BQ_MSVC)
        template <typename pointer_type>
        bq_forceinline void* pointer_to_void(pointer_type pointer)
        {
            union pointer_cast {
                pointer_type typed_pointer;
                void* void_pointer;
            } value;
            value.typed_pointer = pointer;
            return value.void_pointer;
        }

        template <typename pointer_type>
        bq_forceinline pointer_type void_to_pointer(void* pointer)
        {
            union pointer_cast {
                void* void_pointer;
                pointer_type typed_pointer;
            } value;
            value.void_pointer = pointer;
            return value.typed_pointer;
        }
#endif

        template <typename pointer_type>
        bq_forceinline pointer_type atomic_load_acquire(pointer_type* address)
        {
            static_assert(sizeof(pointer_type) == sizeof(void*), "unsupported function pointer size");
#if defined(BQ_GCC) || defined(BQ_CLANG)
            return __atomic_load_n(address, __ATOMIC_ACQUIRE);
#elif defined(BQ_MSVC) && defined(BQ_ARM_64)
            union pointer_bits {
                unsigned __int64 bits;
                pointer_type pointer;
            } value;
            value.bits = __ldar64(reinterpret_cast<volatile unsigned __int64*>(address));
            return value.pointer;
#elif defined(BQ_MSVC) && defined(BQ_ARM_32)
            pointer_type value = *reinterpret_cast<volatile pointer_type*>(address);
            __dmb(_ARM_BARRIER_ISH);
            return value;
#elif defined(BQ_MSVC) && defined(BQ_X86)
            pointer_type value = *reinterpret_cast<volatile pointer_type*>(address);
            _ReadBarrier();
            return value;
#else
#error "bq api dynamic binding does not support this compiler or architecture"
#endif
        }

        template <typename pointer_type>
        bq_forceinline bool atomic_compare_exchange(pointer_type* address, pointer_type* expected, pointer_type desired)
        {
#if defined(BQ_GCC) || defined(BQ_CLANG)
            return __atomic_compare_exchange_n(
                address, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#elif defined(BQ_MSVC)
            void* old_value = _InterlockedCompareExchangePointer(
                reinterpret_cast<void* volatile*>(address),
                pointer_to_void(desired),
                pointer_to_void(*expected));
            pointer_type old_pointer = void_to_pointer<pointer_type>(old_value);
            if (old_pointer == *expected) {
                return true;
            }
            *expected = old_pointer;
            return false;
#else
#error "bq api dynamic binding does not support this compiler"
#endif
        }

        template <typename function_type>
        bq_forceinline function_type symbol_to_function(void* symbol)
        {
#if defined(BQ_MSVC)
            return void_to_pointer<function_type>(symbol);
#else
            return reinterpret_cast<function_type>(symbol);
#endif
        }

        template <typename tag_type, typename pointer_type>
        struct pointer_slot {
            static pointer_type& storage()
            {
                // A null pointer is constant-initialized, so no guard variable
                // or dynamic initialization is needed.
                static pointer_type pointer = nullptr;
                return pointer;
            }
        };

        bq_forceinline void* open_library()
        {
#if defined(BQ_WIN)
            HMODULE module = GetModuleHandleA(BQ_API_DYNAMIC_LIBRARY_NAME);
            if (module == nullptr) {
                module = LoadLibraryA(BQ_API_DYNAMIC_LIBRARY_NAME);
            }
            return reinterpret_cast<void*>(module);
#else
            void* module = dlopen(BQ_API_DYNAMIC_LIBRARY_NAME, RTLD_NOW | RTLD_LOCAL);
#if defined(BQ_API_DYNAMIC_LIBRARY_FALLBACK_NAME)
            if (module == nullptr) {
                module = dlopen(BQ_API_DYNAMIC_LIBRARY_FALLBACK_NAME, RTLD_NOW | RTLD_LOCAL);
            }
#endif
            return module;
#endif
        }

        bq_forceinline void* find_symbol(void* library_handle, const char* symbol_name)
        {
#if defined(BQ_WIN)
            return reinterpret_cast<void*>(
                GetProcAddress(reinterpret_cast<HMODULE>(library_handle), symbol_name));
#else
            return dlsym(library_handle, symbol_name);
#endif
        }

        bq_forceinline void fail_fast()
        {
#if defined(BQ_MSVC)
            __fastfail(7);
            __assume(0);
#else
            __builtin_trap();
#endif
        }

        struct library_handle_tag {};

        struct library_unavailable_tag {};

        bq_forceinline void* get_library_handle()
        {
            void*& storage = pointer_slot<library_handle_tag, void*>::storage();
            void* handle = atomic_load_acquire(&storage);
            BQ_UNLIKELY_IF(handle == nullptr) {
                void* candidate = open_library();
                if (candidate != nullptr) {
                    void* expected = nullptr;
                    if (atomic_compare_exchange(&storage, &expected, candidate)) {
                        handle = candidate;
                    } else {
                        handle = expected;
                    }
                }
            }
            return handle;
        }

        bq_forceinline bool is_library_ready()
        {
            void*& handle_storage = pointer_slot<library_handle_tag, void*>::storage();
            void* handle = atomic_load_acquire(&handle_storage);
            if (handle != nullptr) {
                return true;
            }

            void*& unavailable_storage = pointer_slot<library_unavailable_tag, void*>::storage();
            if (atomic_load_acquire(&unavailable_storage) != nullptr) {
                return false;
            }

            handle = get_library_handle();
            if (handle != nullptr) {
                return true;
            }

            handle = atomic_load_acquire(&handle_storage);
            if (handle != nullptr) {
                return true;
            }

            void* expected = nullptr;
            void* unavailable_marker = &unavailable_storage;
            atomic_compare_exchange(&unavailable_storage, &expected, unavailable_marker);
            return false;
        }

        template <typename tag_type, typename function_type>
        bq_forceinline function_type get_function(const char* symbol_name)
        {
            function_type& storage = pointer_slot<tag_type, function_type>::storage();
            function_type function = atomic_load_acquire(&storage);
            BQ_UNLIKELY_IF(function == nullptr) {
                void* library_handle = get_library_handle();
                void* symbol =
                    library_handle == nullptr ? nullptr : find_symbol(library_handle, symbol_name);
                function_type candidate = symbol_to_function<function_type>(symbol);
                if (candidate != nullptr) {
                    function_type expected = nullptr;
                    if (atomic_compare_exchange(&storage, &expected, candidate)) {
                        function = candidate;
                    } else {
                        function = expected;
                    }
                }
                if (function == nullptr) {
                    fail_fast();
                }
            }
            return function;
        }
    }
}

namespace bq {
    /// <summary>
    /// Returns true when the late-bound dynamic library can be loaded.
    /// If it has not been loaded yet, this call attempts to load and cache it.
    /// Unlike invoking an API symbol, this function never fail-fasts on load failure.
    /// </summary>
    bq_forceinline bool is_library_ready()
    {
        return ::bq::api_dynamic_binding::is_library_ready();
    }
}

#if defined(BQ_GCC) || defined(BQ_CLANG)
#pragma GCC visibility pop
#define BQ_API_DYNAMIC_BINDING_VISIBILITY __attribute__((visibility("hidden")))
#else
#define BQ_API_DYNAMIC_BINDING_VISIBILITY
#endif

#define BQ_API_DEF(return_type, name, parameters, arguments)                                      \
    struct bq_api_binding_tag_##name;                                                              \
    BQ_API_DYNAMIC_BINDING_VISIBILITY bq_forceinline return_type name parameters                   \
    {                                                                                              \
        typedef decltype(&name) bq_api_function_type_##name;                                       \
        bq_api_function_type_##name function =                                                     \
            ::bq::api_dynamic_binding::get_function<bq_api_binding_tag_##name,                     \
                bq_api_function_type_##name>(#name);                                               \
        return function arguments;                                                                 \
    }

#if defined(BQ_WIN) && BQ_IN_UNREAL
#include "Windows/HideWindowsPlatformTypes.h"
#endif
