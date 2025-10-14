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

#include "bq_common/bq_common_public_include.h"
#include "bq_common/types/basic_types.h"
#include "bq_common/platform/platform_misc.h"

namespace bq {
    class util {
    public:
        static void bq_assert(bool cond, bq::string msg);
        static void bq_record(bq::string msg, string file_name = "__bq_assert.log");

#if defined(BQ_TOOLS) || defined(BQ_UNIT_TEST)
        static void set_log_device_console_min_level(bq::log_level level);
#endif
        // this function is signal-safety but has limit buffer, the log content exceed buffer size will be truncated
        static void log_device_console(bq::log_level level, const char* format, ...)
#if defined(BQ_GCC) || defined(BQ_CLANG)
            __attribute__((format(printf, 2, 3)))
#endif
            ;

        static void log_device_console_plain_text(bq::log_level level, const char* text);

        typedef void(BQ_STDCALL* type_func_ptr_bq_util_consle_callback)(bq::log_level level, const char* text);
        static void set_console_output_callback(type_func_ptr_bq_util_consle_callback callback);

        // Output to console directly, no console callback will be called
        static void _default_console_output(bq::log_level level, const char* text);

        static uint32_t get_hash(const void* data, size_t size);

        static uint64_t get_hash_64(const void* data, size_t size);

        static bool is_little_endian();

        static void srand(uint32_t seek);

        static uint32_t rand();

        /// <summary>
        /// convert utf16 to utf8
        /// </summary>
        /// <param name="src_utf16_str"></param>
        /// <param name="src_character_num"></param>
        /// <param name="target_utf8_str"></param>
        /// <param name="target_utf8_character_num"></param>
        /// <returns>length of final utf8 str</returns>
        static uint32_t utf16_to_utf8(const char16_t* src_utf16_str, uint32_t src_character_num, char* target_utf8_str, uint32_t target_utf8_character_num);

        /// <summary>
        /// convert utf8 to utf16
        /// </summary>
        /// <param name="src_utf8_str"></param>
        /// <param name="src_character_num"></param>
        /// <param name="target_utf16_str"></param>
        /// <param name="target_utf16_character_num"></param>
        /// <returns>length of final utf16 str, it's len of char16_t*, not char*</returns>
        static uint32_t utf8_to_utf16(const char* src_utf8_str, uint32_t src_character_num, char16_t* target_utf16_str, uint32_t target_utf16_character_num);

        /// <summary>
        /// Allocates and constructs an object of type `T` with the specified alignment.
        ///
        /// If the compiler supports C++17 aligned `new` (indicated by `BQ_ALIGNAS_NEW`),
        /// the function uses the standard `new` operator, and the `alignment` parameter is ignored.
        /// In this case, the alignment is determined by the `alignas` specifier in the class definition of `T`.
        ///
        /// If the compiler does not support C++17 aligned `new`, the function uses
        /// `bq::platform::aligned_alloc` to allocate memory with the specified `alignment`,
        /// and constructs the object using placement new. The caller must ensure that the
        /// provided `alignment` matches the `alignas` specifier (if any) in the class definition
        /// of `T` to avoid undefined behavior.
        /// </summary>
        /// <typeparam name="T">The type of the object to allocate and construct.</typeparam>
        /// <typeparam name="V">Variadic template parameters for the constructor arguments of `T`.</typeparam>
        /// <param name="alignment">The alignment requirement for the allocated memory (in bytes).
        ///                        Ignored if `BQ_ALIGNAS_NEW` is defined. Otherwise, it must be a power of 2
        ///                        and should match the `alignas` specifier of `T` (if any).</param>
        /// <param name="args">Arguments forwarded to the constructor of `T`.</param>
        /// <returns>A pointer to the constructed object of type `T`, or `nullptr` if allocation fails.</returns>
        template <typename T, typename... V>
        static T* aligned_new(const size_t alignment, V&&... args)
        {
#if defined(BQ_ALIGNAS_NEW)
            (void)alignment;
            return new T(bq::forward<V>(args)...);
#else
            void* ptr = bq::platform::aligned_alloc(alignment, sizeof(T));
            if (!ptr) {
                return nullptr;
            }
            new ((void*)ptr, bq::enum_new_dummy::dummy) T(bq::forward<V>(args)...);
            return static_cast<T*>(ptr);
#endif
        }
        template <typename T, typename... V>
        static void aligned_delete(T* ptr)
        {
#if defined(BQ_ALIGNAS_NEW)
            delete ptr;
#else
            bq::object_destructor<T>::destruct(static_cast<T*>(ptr));
            bq::platform::aligned_free(ptr);
#endif
        }
    };

}
